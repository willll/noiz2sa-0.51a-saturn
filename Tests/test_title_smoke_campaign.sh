#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Tests/test_title_smoke_campaign.sh [options]

Runs a full-game Mednafen smoke test: the title/menu is reached, a stage is
started from the menu (the path players use), and nothing crashes.

Options:
  --emulator <mednafen>                  Emulator target (default: mednafen)
  --timeout <seconds>                    Mednafen runtime timeout (default: 240)
  --sound <0|1>                          Mednafen sound setting (default: 0 for quiet CI smoke)
  --skip-build                           Skip CMake configure/build
  --log <path>                           Emulator log path (default: <repo>/logs/mednafen_mainmenu_smoke.log)
  --strict                               Require both TITLE enter and TITLE ready markers
  -h, --help                             Show help
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

EMULATOR="mednafen"
TIMEOUT_SECONDS=240
SOUND_SETTING=0
SKIP_BUILD=0
STRICT=0
LOG_FILE="$ROOT_DIR/logs/mednafen_mainmenu_smoke.log"
mkdir -p "$ROOT_DIR/logs"

ensure_emulator_log_output() {
  echo "[smoke] Configuring build for emulator-visible logs (SRL_LOG_OUTPUT=EMULATOR)"
  # SRL_LOG_LEVEL=INFO is required: this harness waits for the LogInfo-level
  # "[STATE] TITLE screen ready" marker, which the production default
  # (WARNING) compiles out.
  # NOIZ2SA_TEST_MENU_AUTOSELECT: press start on the title menu after 300 ticks;
  # reset on exit so later builds from build/ don't keep it.
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DSRL_LOG_OUTPUT=EMULATOR -DSRL_LOG_LEVEL=INFO \
    -DNOIZ2SA_TEST_MENU_AUTOSELECT=300
  trap 'cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DNOIZ2SA_TEST_MENU_AUTOSELECT=0 >/dev/null' EXIT
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --emulator)
      EMULATOR="$2"; shift 2;;
    --timeout)
      TIMEOUT_SECONDS="$2"; shift 2;;
    --sound)
      SOUND_SETTING="$2"; shift 2;;
    --skip-build)
      SKIP_BUILD=1; shift;;
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

if [[ "$EMULATOR" != "mednafen" ]]; then
  echo "[smoke] Unsupported emulator '$EMULATOR' for this smoke test" >&2
  exit 2
fi

if [[ "$SOUND_SETTING" != "0" && "$SOUND_SETTING" != "1" ]]; then
  echo "[smoke] Invalid --sound value '$SOUND_SETTING' (expected 0 or 1)" >&2
  exit 2
fi

if [[ $SKIP_BUILD -eq 0 ]]; then
  ensure_emulator_log_output
  echo "[smoke] Building full game image (build_bin_cue)"
  cmake --build "$ROOT_DIR/build" --target build_bin_cue
fi

echo "[smoke] Running full game CUE in Mednafen with timeout ${TIMEOUT_SECONDS}s"
set +e
(
  cd "$ROOT_DIR"
  MEDNAFEN_ALLOWMULTI="${MEDNAFEN_ALLOWMULTI:-1}" \
    timeout -k 5s "${TIMEOUT_SECONDS}s" \
    mednafen -sound "$SOUND_SETTING" -ss.cart debug -force_module ss BuildDrop/noiz2sa.cue \
    >"$LOG_FILE" 2>&1
)
run_status=$?
set -e

if [[ $run_status -ne 0 && $run_status -ne 124 ]]; then
  echo "[smoke] Emulator run failed (exit=$run_status)" >&2
  tail -n 120 "$LOG_FILE" >&2 || true
  exit 3
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "[smoke] Expected log file missing: $LOG_FILE" >&2
  exit 3
fi

title_enter_count=$(rg -n "\\[STATE\\] Entering TITLE screen" "$LOG_FILE" | wc -l || true)
title_ready_count=$(rg -n "\\[STATE\\] TITLE screen ready" "$LOG_FILE" | wc -l || true)

echo "[smoke] Marker counts: enter=$title_enter_count ready=$title_ready_count"

if [[ $STRICT -eq 1 ]]; then
  if [[ "$title_enter_count" -lt 1 || "$title_ready_count" -lt 1 ]]; then
    echo "[smoke] Strict mode failed: title markers missing" >&2
    tail -n 120 "$LOG_FILE" >&2 || true
    exit 4
  fi
fi

if [[ "$title_ready_count" -lt 1 ]]; then
  echo "[smoke] Title screen was not reached before timeout" >&2
  tail -n 120 "$LOG_FILE" >&2 || true
  exit 4
fi

game_ready_count=$(grep -c -E "IN_GAME \(stage [0-9]+\) ready" "$LOG_FILE" || true)
crash_count=$(grep -c "CRASH DUMP BEGIN" "$LOG_FILE" || true)
echo "[smoke] Stage started from menu: $game_ready_count, crash dumps: $crash_count"
if [[ "$crash_count" -gt 0 ]]; then
  echo "[smoke] CPU crash dump in the log" >&2
  grep -m1 -A6 "CRASH DUMP BEGIN" "$LOG_FILE" >&2 || true
  exit 5
fi
if [[ $SKIP_BUILD -eq 0 && "$game_ready_count" -lt 1 ]]; then
  echo "[smoke] No stage started from the title menu" >&2
  tail -n 60 "$LOG_FILE" >&2 || true
  exit 6
fi

echo "[smoke] PASS: title/menu reached, stage started from the menu. Log: $LOG_FILE"
