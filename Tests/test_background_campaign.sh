#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Tests/test_background_campaign.sh [options]

Runs the SH2 background animation regression campaign: build -> run.

Options:
  --emulator <mednafen|kronos|USBGamers>   Emulator target (default: mednafen)
  --skip-build                            Skip CMake build step for SH2 test disc image
  --skip-run                              Skip running emulator (requires existing Tests/uts_background.log)
  --log <path>                            Path to uts_background.log (default: <repo>/Tests/uts_background.log)
  --strict                                Exit non-zero if uts_background.log contains any FATAL failures
  -h, --help                              Show help
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

EMULATOR="mednafen"
SKIP_BUILD=0
SKIP_RUN=0
STRICT=0
LOG_FILE="$ROOT_DIR/Tests/uts_background.log"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --emulator)
      EMULATOR="$2"; shift 2;;
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
  echo "[campaign] Building SH2 background tests (cmake --build build --target noiz2sa_background_ut_bin_cue)"
  cmake --build "$ROOT_DIR/build" --target noiz2sa_background_ut_bin_cue
fi

if [[ $SKIP_RUN -eq 0 ]]; then
  echo "[campaign] Running SH2 background tests via emulator: $EMULATOR"
  (
    cd "$ROOT_DIR/Tests"
    UT_LOG_FILE="$LOG_FILE" bash "$ROOT_DIR/Tests/run_tests.bat" "$EMULATOR" "BuildDrop/noiz2sa_background_ut.cue"
  )
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "uts_background.log not found at: $LOG_FILE" >&2
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
