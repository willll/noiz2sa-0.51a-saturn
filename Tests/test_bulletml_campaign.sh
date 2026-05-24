#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Tests/test_bulletml_campaign.sh [options]

Runs the SH2 BulletML parser campaign: build -> run.

Options:
  --emulator <mednafen|kronos|USBGamers>   Emulator target (default: mednafen)
  --psu-ip <ip-or-url>                    Optional PSU controller endpoint for USBGamers preflight
  --skip-build                            Skip CMake build step for SH2 test disc image
  --skip-run                              Skip running emulator (requires existing logs/uts_bulletml.log)
  --log <path>                            Path to uts_bulletml.log (default: <repo>/logs/uts_bulletml.log)
  --strict                                Exit non-zero if uts log contains any FATAL failures
  -h, --help                              Show help
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

EMULATOR="mednafen"
SKIP_BUILD=0
SKIP_RUN=0
STRICT=0
PSU_IP=""
SATURN_PSU_IP_FALLBACK="${SATURN_PSU_IP_FALLBACK:-192.168.1.106}"
LOG_FILE="$ROOT_DIR/logs/uts_bulletml.log"
mkdir -p "$ROOT_DIR/logs"

ensure_emulator_log_output() {
  # UT runners detect completion via emulator console marker (***UT_END***).
  # Force EMULATOR log sink so markers are visible when using emulator backends.
  if [[ "$EMULATOR" == "USBGamers" ]]; then
    return
  fi

  echo "[campaign] Configuring build for emulator-visible logs (SRL_LOG_OUTPUT=EMULATOR)"
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" \
    -DSRL_LOG_OUTPUT=EMULATOR
}

run_usbgamers_preflight() {
  if [[ "$EMULATOR" != "USBGamers" ]]; then
    return
  fi

  local settle_seconds="${USB_SETTLE_SECONDS:-2}"

  echo "[campaign] USBGamers preflight: verifying hardware link"

  if [[ -n "$PSU_IP" ]]; then
    local psu_base
    psu_base="$(resolve_psu_base "$PSU_IP")"

    echo "[campaign] USBGamers preflight: PSU OFF (${psu_base}/api/v1/off)"
    curl -fsS -m 5 -X POST "${psu_base}/api/v1/off" >/dev/null
    sleep 2
    echo "[campaign] USBGamers preflight: PSU ON (${psu_base}/api/v1/on)"
    curl -fsS -m 5 -X POST "${psu_base}/api/v1/on" >/dev/null
    sleep 10
  fi

  if ! command -v usbreset >/dev/null 2>&1; then
    echo "[campaign] ERROR: usbreset not found. Install usbreset for USBGamers preflight." >&2
    exit 4
  fi

  usbreset "FT245R USB FIFO" || true
  sleep "$settle_seconds"

  local probe_log
  local probe_status=0
  local probe_timeout="${FTX_PROBE_TIMEOUT_SECONDS:-3}"
  probe_log="$(mktemp)"

  # ftx -c is interactive; run it under timeout and inspect probe output.
  if ! timeout "${probe_timeout}s" ftx -c >"$probe_log" 2>&1; then
    probe_status=$?
  fi

  if grep -q "Device open error: device not found" "$probe_log"; then
    echo "[campaign] ERROR: USBGamers preflight failed (ftx probe did not open device)." >&2
    rm -f "$probe_log"
    exit 4
  fi

  if ! grep -q "FTDI device initialized successfully" "$probe_log"; then
    echo "[campaign] ERROR: USBGamers preflight failed (ftx probe did not confirm FTDI init)." >&2
    cat "$probe_log" >&2
    rm -f "$probe_log"
    exit 4
  fi

  if [[ $probe_status -ne 0 && $probe_status -ne 124 ]]; then
    echo "[campaign] ERROR: USBGamers preflight failed (ftx probe exit=$probe_status)." >&2
    cat "$probe_log" >&2
    rm -f "$probe_log"
    exit 4
  fi

  rm -f "$probe_log"
}

resolve_psu_base() {
  local psu_base="$1"
  if [[ "$psu_base" != http://* && "$psu_base" != https://* ]]; then
    psu_base="http://$psu_base"
  fi

  local hostport="${psu_base#*://}"
  hostport="${hostport%%/*}"
  local host="${hostport%%:*}"

  # Accept common typo by mapping it to the intended local hostname.
  if [[ "$host" == "aturnpsu.local" ]]; then
    psu_base="${psu_base/$host/saturnpsu.local}"
    host="saturnpsu.local"
  fi

  # Hostnames may not resolve inside dev containers; fall back to IP if needed.
  if [[ "$host" == *[A-Za-z]* ]]; then
    if command -v getent >/dev/null 2>&1 && ! getent hosts "$host" >/dev/null 2>&1; then
      echo "[campaign] WARNING: Could not resolve '$host'; using fallback ${SATURN_PSU_IP_FALLBACK}" >&2
      psu_base="${psu_base/$host/$SATURN_PSU_IP_FALLBACK}"
    fi
  fi

  echo "${psu_base%/}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --emulator)
      EMULATOR="$2"; shift 2;;
    --psu-ip)
      PSU_IP="$2"; shift 2;;
    --skip-build)
      SKIP_BUILD=1; shift;;
    --skip-run)
      SKIP_RUN=1; shift;;
    --log)
      LOG_FILE="$2"; shift 2;;
    --strict)
      STRICT=1; shift;;
    -h|--help)
      usage; exit 0;;
    *)
      echo "Unknown arg: $1" >&2
      usage
      exit 2;;
  esac
done

if [[ $SKIP_BUILD -eq 0 ]]; then
  ensure_emulator_log_output
  echo "[campaign] Building SH2 bulletml tests (cmake --build build --target noiz2sa_bulletml_ut_bin_cue)"
  cmake --build "$ROOT_DIR/build" --target noiz2sa_bulletml_ut_bin_cue
fi

if [[ $SKIP_RUN -eq 0 ]]; then
  run_usbgamers_preflight
  echo "[campaign] Running SH2 bulletml tests via emulator: $EMULATOR"
  (
    cd "$ROOT_DIR/Tests"
    MEDNAFEN_ALLOWMULTI="${MEDNAFEN_ALLOWMULTI:-1}" \
      bash "$ROOT_DIR/Tests/run_bulletml_tests.sh" "$EMULATOR"
  )
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "uts_bulletml.log not found at: $LOG_FILE" >&2
  exit 3
fi

if [[ $STRICT -eq 1 ]]; then
  echo "[campaign] Strict mode enabled"
  python3 - <<PY
import re
log_file = r'''$LOG_FILE'''
failed = 0
with open(log_file, 'r', encoding='utf-8', errors='replace') as f:
    for line in f:
        if re.match(r"FATAL\s*:\s*.*\s+failed:", line):
            failed += 1
if failed:
    raise SystemExit(f"Strict mode: {failed} failures found in {log_file}")
print("Strict mode: 0 failures")
PY
fi

echo "[campaign] Done. Log: $LOG_FILE"
