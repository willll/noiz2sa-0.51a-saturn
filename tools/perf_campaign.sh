#!/bin/bash
# perf_campaign.sh — Multi-iteration Saturn 30 FPS performance campaign
#
# Usage:
#   ./tools/perf_campaign.sh [OPTIONS]
#
# Options:
#   -i IP       ESP-SaturnPSU_Control REST API IP (default: 192.168.1.106)
#   -n N        Number of hardware iterations (default: 10)
#   -l DIR      Log output directory (default: logs/perf_<TIMESTAMP>)
#   -t TAG      Variant tag appended to log names (default: "baseline")
#   -b PATH     Binary path to upload (default: ./cd/data/0.bin)
#   -s SECS     Duration to capture serial per iteration (default: 120)
#   -f THRESH   FPS threshold for drop detection (default: 28)
#   --no-power  Skip REST API power cycle (assume hardware already on)
#
# Examples:
#   ./tools/perf_campaign.sh                               # baseline, 10 iters
#   ./tools/perf_campaign.sh -t no-smoke -n 10             # smoke-off variant
#   ./tools/perf_campaign.sh -t cull-opt -n 5 -s 60        # quick 5-iter pass
#
# After all iterations, prints FPS mean/p90/p99/worst and outputs a summary CSV.

set -euo pipefail

# ─── Defaults ────────────────────────────────────────────────────────────────
REST_IP="192.168.1.106"
ITERATIONS=10
LOG_DIR=""
VARIANT_TAG="baseline"
BINARY_PATH="./cd/data/0.bin"
CAPTURE_SECS=120
FPS_THRESHOLD=28
POWER_CYCLE=true
POWER_ON_SETTLE=20

# ─── Argument parsing ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    -i) REST_IP="$2"; shift 2 ;;
    -n) ITERATIONS="$2"; shift 2 ;;
    -l) LOG_DIR="$2"; shift 2 ;;
    -t) VARIANT_TAG="$2"; shift 2 ;;
    -b) BINARY_PATH="$2"; shift 2 ;;
    -s) CAPTURE_SECS="$2"; shift 2 ;;
    -f) FPS_THRESHOLD="$2"; shift 2 ;;
    --no-power) POWER_CYCLE=false; shift ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

if [[ -z "$LOG_DIR" ]]; then
  LOG_DIR="logs/perf_$(date +%Y%m%d_%H%M%S)_${VARIANT_TAG}"
fi
mkdir -p "$LOG_DIR"

# ─── Utilities ────────────────────────────────────────────────────────────────
log() { echo "[$(date +%H:%M:%S)] $*"; }

reset_usb() {
  if command -v usbreset &>/dev/null; then
    usbreset "FT245R USB FIFO" 2>/dev/null || true
    sleep 1
  fi
}

power_off() {
  if [[ "$POWER_CYCLE" == false ]]; then return 0; fi
  log "Power OFF → $REST_IP"
  curl -s -X POST "http://$REST_IP/api/v1/off" >/dev/null 2>&1 || true
  sleep 1
  local relay
  relay=$(curl -s "http://$REST_IP/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
  if [[ "$relay" != "OFF" ]]; then
    curl -s -X POST "http://$REST_IP/api/v1/toggle" >/dev/null 2>&1 || true
    sleep 1
  fi
  sleep 3
}

power_on() {
  if [[ "$POWER_CYCLE" == false ]]; then return 0; fi
  log "Power ON → $REST_IP"
  curl -s -X POST "http://$REST_IP/api/v1/on" >/dev/null 2>&1 || true
  sleep 3
  local relay
  relay=$(curl -s "http://$REST_IP/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
  if [[ "$relay" != "ON" ]]; then
    curl -s -X POST "http://$REST_IP/api/v1/toggle" >/dev/null 2>&1 || true
    sleep 1
  fi
  log "Waiting ${POWER_ON_SETTLE}s for hardware stabilization..."
  sleep "$POWER_ON_SETTLE"
}

upload_binary() {
  reset_usb
  sleep 2
  log "Uploading $BINARY_PATH → 0x06004000"
  local upload_out
  upload_out=$(ftx -x "$BINARY_PATH" 0x06004000 2>&1 || true)
  if echo "$upload_out" | grep -qiE "Upload failed|Send data error|Execution aborted|usb bulk write failed|Device open error|device not found"; then
    log "ERROR: Upload failed — $upload_out"
    return 1
  fi
  log "Upload OK"
}

# ─── Pre-flight checks ────────────────────────────────────────────────────────
if [[ ! -f "$BINARY_PATH" ]]; then
  echo "ERROR: Binary not found: $BINARY_PATH"
  echo "Build with: cmake -B build_hw_debug -DHW_DEBUG=ON && cmake --build build_hw_debug"
  exit 1
fi

echo "========================================================================"
echo " noiz2sa Saturn 30 FPS Performance Campaign"
echo "========================================================================"
echo " Variant : $VARIANT_TAG"
echo " Binary  : $BINARY_PATH  ($(ls -lh "$BINARY_PATH" | awk '{print $5}'))"
echo " Iters   : $ITERATIONS × ${CAPTURE_SECS}s capture"
echo " Log dir : $LOG_DIR"
echo " FPS gate: $FPS_THRESHOLD"
if [[ "$POWER_CYCLE" == true ]]; then
  echo " Power   : REST API at $REST_IP"
else
  echo " Power   : SKIPPED (--no-power)"
fi
echo "========================================================================"
echo ""

# ─── Per-iteration loop ───────────────────────────────────────────────────────
SUCCESSES=0
FAILURES=0

for iter in $(seq 1 "$ITERATIONS"); do
  log "═══ Iteration $iter / $ITERATIONS ═══"
  iter_log="$LOG_DIR/${VARIANT_TAG}_iter$(printf '%02d' "$iter").log"

  power_off

  if ! power_on; then
    log "SKIP iter $iter: power-on failed"
    FAILURES=$((FAILURES + 1))
    continue
  fi

  if ! upload_binary; then
    log "SKIP iter $iter: upload failed"
    FAILURES=$((FAILURES + 1))
    continue
  fi

  reset_usb
  sleep 2

  log "Capturing serial for ${CAPTURE_SECS}s → $iter_log"
  # Discard first ~10s (warm-up; cache settle and game-init chatter)
  # then capture the measurement window
  local_timeout=$((CAPTURE_SECS + 15))
  timeout "$local_timeout" ftx -c 2>&1 | tee "$iter_log" &
  FTX_PID=$!

  # Wait for game-ready marker (up to 45s)
  ready_wait=0
  while [[ $ready_wait -lt 45 ]]; do
    if grep -qE "Reached stage|IN_GAME.*ready" "$iter_log" 2>/dev/null; then
      log "Game ready marker found after ${ready_wait}s"
      break
    fi
    sleep 2
    ready_wait=$((ready_wait + 2))
  done

  # Let the capture run to completion
  wait "$FTX_PID" 2>/dev/null || true

  if [[ -s "$iter_log" ]] && grep -qE "\[PERF\]|\[FPS\]" "$iter_log" 2>/dev/null; then
    SUCCESSES=$((SUCCESSES + 1))
    log "Iter $iter: log captured ($(wc -l < "$iter_log") lines)"
  else
    log "WARN iter $iter: no perf data in log"
    FAILURES=$((FAILURES + 1))
  fi

  power_off
  log "Cooling down 5s between iterations..."
  sleep 5
done

# ─── Aggregate analysis ───────────────────────────────────────────────────────
echo ""
echo "========================================================================"
echo " Results: $SUCCESSES / $ITERATIONS iterations captured"
echo "========================================================================"

# Collect all FPS values across logs
FPS_FILE="$LOG_DIR/all_fps.txt"
> "$FPS_FILE"
for f in "$LOG_DIR"/${VARIANT_TAG}_iter*.log; do
  [[ -f "$f" ]] || continue
  grep "\[PERF\]" "$f" | grep "fps=" | sed 's/.*fps=\([0-9][0-9]*\.[0-9]*\).*/\1/' >> "$FPS_FILE" || true
done

if [[ ! -s "$FPS_FILE" ]]; then
  echo "No [PERF] fps= data found. Check serial output and HW_DEBUG build."
  exit 1
fi

# FPS statistics via awk
echo ""
echo " FPS statistics across $SUCCESSES iterations:"
awk '
BEGIN { n=0; sum=0; sumsq=0; min=9999; max=0 }
{
  v=$1+0
  sum+=v; sumsq+=v*v; n++
  if(v<min) min=v
  if(v>max) max=v
}
END {
  if(n==0) { print "  No data"; exit }
  mean=sum/n
  stddev=sqrt(sumsq/n - mean*mean)
  printf "  Samples : %d\n", n
  printf "  Mean FPS: %.2f\n", mean
  printf "  StdDev  : %.2f\n", stddev
  printf "  Min FPS : %.2f\n", min
  printf "  Max FPS : %.2f\n", max
}' "$FPS_FILE"

# Compute p90/p99 via sort
echo ""
echo " Percentiles:"
sort -n "$FPS_FILE" | awk -v n="$(wc -l < "$FPS_FILE")" '
NR==int(n*0.01)+1 { printf "  p1  FPS: %.2f\n", $1 }
NR==int(n*0.10)+1 { printf "  p10 FPS: %.2f\n", $1 }
NR==int(n*0.50)+1 { printf "  p50 FPS: %.2f\n", $1 }
NR==int(n*0.90)+1 { printf "  p90 FPS: %.2f\n", $1 }
NR==int(n*0.99)+1 { printf "  p99 FPS: %.2f\n", $1 }
'

# Gate check
p1_fps=$(sort -n "$FPS_FILE" | awk -v n="$(wc -l < "$FPS_FILE")" 'NR==int(n*0.01)+1{print $1}')
echo ""
if awk "BEGIN { exit ($p1_fps >= $FPS_THRESHOLD) ? 0 : 1 }"; then
  echo "  ✓ PASS — p1 FPS $p1_fps >= threshold $FPS_THRESHOLD"
else
  echo "  ✗ FAIL — p1 FPS $p1_fps < threshold $FPS_THRESHOLD (target: 30)"
fi

# Run perf_drop_report if available
if command -v python3 &>/dev/null && [[ -f "tools/perf_drop_report.py" ]]; then
  echo ""
  echo " Per-iteration drop report (fps-threshold=$FPS_THRESHOLD, top 5 worst windows):"
  for f in "$LOG_DIR"/${VARIANT_TAG}_iter*.log; do
    [[ -f "$f" ]] || continue
    echo "  ── $(basename "$f") ──"
    python3 tools/perf_drop_report.py "$f" \
      --fps-threshold "$FPS_THRESHOLD" \
      --top 5 2>/dev/null | sed 's/^/    /' || true
  done
fi

# Summary CSV
CSV="$LOG_DIR/summary.csv"
echo "variant,iter,fps_mean,fps_min,fps_max" > "$CSV"
for f in "$LOG_DIR"/${VARIANT_TAG}_iter*.log; do
  [[ -f "$f" ]] || continue
  iter_num=$(basename "$f" | grep -o 'iter[0-9]*' | tr -d 'iter')
  awk -v tag="$VARIANT_TAG" -v it="$iter_num" '
    /\[PERF\]/ && /fps=/ {
      match($0, /fps=([0-9]+\.[0-9]+)/, a)
      v=a[1]+0; sum+=v; n++
      if(n==1||v<mn) mn=v
      if(n==1||v>mx) mx=v
    }
    END { if(n>0) printf "%s,%s,%.2f,%.2f,%.2f\n",tag,it,sum/n,mn,mx }
  ' "$f" >> "$CSV" || true
done
echo ""
echo " Summary CSV: $CSV"
echo "========================================================================"
