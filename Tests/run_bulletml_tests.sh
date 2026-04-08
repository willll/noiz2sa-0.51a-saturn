#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [kronos|mednafen|USBGamers]"
  exit 1
fi

EMULATOR="$1"
LOG="uts_bulletml.log"
MATCH="***UT_END***"
CUE="BuildDrop/noiz2sa_bulletml_ut.cue"

stop_emulator() {
  if [[ -n ${EMULATOR_PID:-} ]] && kill -0 "$EMULATOR_PID" 2>/dev/null; then
    kill -15 "$EMULATOR_PID" 2>/dev/null || true
    for _ in 1 2 3 4 5; do
      if ! kill -0 "$EMULATOR_PID" 2>/dev/null; then
        break
      fi
      sleep 1
    done
    if kill -0 "$EMULATOR_PID" 2>/dev/null; then
      kill -9 "$EMULATOR_PID" 2>/dev/null || true
    fi
    wait "$EMULATOR_PID" 2>/dev/null || true
  fi
}

cleanup() {
  status=$?
  stop_emulator
  exit $status
}
trap cleanup EXIT

if [[ "$EMULATOR" == "mednafen" ]]; then
  CMD="mednafen -sound 0 -ss.cart debug -force_module ss $CUE"
elif [[ "$EMULATOR" == "kronos" ]]; then
  CMD="kronos -a -ns -i $CUE"
elif [[ "$EMULATOR" == "USBGamers" ]]; then
  if [[ ! -f cd_bulletml/data/0.bin ]]; then
    echo "ERROR: cd_bulletml/data/0.bin not found. Build tests first."
    exit 1
  fi
  CMD="ftx -c"
  ftx -x cd_bulletml/data/0.bin 0x06004000
else
  echo "No valid emulator specified"
  exit 1
fi

echo "Test command: $CMD"
$CMD > >(tee "$LOG") 2>&1 &
EMULATOR_PID=$!

START=$(date +%s)
LIMIT=300

while sleep 1; do
  NOW=$(date +%s)
  ELAPSED=$((NOW - START))
  if [[ $ELAPSED -ge $LIMIT ]]; then
    echo "Test timed out after $LIMIT seconds"
    stop_emulator
    exit 1
  fi

  if fgrep --quiet "$MATCH" "$LOG"; then
    echo "Test completion marker found"
    stop_emulator
    echo "Tests completed successfully"
    exit 0
  fi

  if ! kill -0 "$EMULATOR_PID" 2>/dev/null; then
    echo "Emulator process has terminated unexpectedly"
    exit 1
  fi
done
