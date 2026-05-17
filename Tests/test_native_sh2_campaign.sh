#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Tests/test_native_sh2_campaign.sh --cd-name <name> [options]

Runs a Saturn SH2 unit-test campaign image: build -> run.

Required:
  --cd-name <name>                       Test CD name (e.g. noiz2sa_bulletml_parity_ut)

Options:
  --emulator <mednafen|kronos|USBGamers> Emulator target (default: mednafen)
  --skip-build                            Skip CMake build step for SH2 test disc image
  --skip-run                              Skip running emulator (requires existing log)
  --log <path>                            Path to log file (default: <repo>/logs/uts_<cd-name>.log)
  --strict                                Exit non-zero if log contains FAILED output
  -h, --help                              Show help
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

EMULATOR="mednafen"
CD_NAME=""
SKIP_BUILD=0
SKIP_RUN=0
STRICT=0
LOG_FILE=""

ensure_emulator_log_output() {
  if [[ "$EMULATOR" == "USBGamers" ]]; then
    return
  fi

  echo "[campaign] Configuring build for emulator-visible logs (SRL_LOG_OUTPUT=EMULATOR)"
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" \
    -DSRL_LOG_OUTPUT=EMULATOR
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cd-name)
      CD_NAME="$2"; shift 2;;
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

if [[ -z "$CD_NAME" ]]; then
  echo "--cd-name is required" >&2
  usage
  exit 2
fi

if [[ -z "$LOG_FILE" ]]; then
  LOG_FILE="$ROOT_DIR/logs/uts_${CD_NAME}.log"
  mkdir -p "$ROOT_DIR/logs"
fi

BIN_CUE_TARGET="${CD_NAME}_bin_cue"
CUE_PATH="BuildDrop/${CD_NAME}.cue"

if [[ $SKIP_BUILD -eq 0 ]]; then
  ensure_emulator_log_output
  echo "[campaign] Building SH2 test image target: ${BIN_CUE_TARGET}"
  cmake --build "$ROOT_DIR/build" --target "$BIN_CUE_TARGET"
fi

if [[ $SKIP_RUN -eq 0 ]]; then
  echo "[campaign] Running SH2 test image via emulator: $EMULATOR ($CUE_PATH)"
  (
    cd "$ROOT_DIR/Tests"
    MEDNAFEN_ALLOWMULTI="${MEDNAFEN_ALLOWMULTI:-1}" \
      UT_LOG_FILE="$LOG_FILE" \
      bash "$ROOT_DIR/Tests/run_tests.bat" "$EMULATOR" "$CUE_PATH"
  )
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "Log not found at: $LOG_FILE" >&2
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
        if re.search(r"FAILED|failed:", line):
            failed += 1
if failed:
    raise SystemExit(f"Strict mode: {failed} failure marker(s) found in {log_file}")
print("Strict mode: 0 failure markers")
PY
fi

echo "[campaign] Done. Log: $LOG_FILE"
