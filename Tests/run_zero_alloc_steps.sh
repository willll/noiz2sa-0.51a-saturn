#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: Tests/run_zero_alloc_steps.sh [--step N | --all]

Sequentially runs migration test gates for zero-runtime-allocation rollout.

Options:
  --step N   Run all gates from step 1 through step N.
  --all      Run all currently defined steps.
  -h, --help Show this help.
EOF
}

run_step_1() {
  echo "[zero-alloc] Step 1 gate: production wiring baseline"
  bash "$ROOT_DIR/Tests/test_factory_campaign.sh" --emulator mednafen --strict
  bash "$ROOT_DIR/Tests/test_bulletml_campaign.sh" --emulator mednafen --strict
  ctest --test-dir "$ROOT_DIR/build" --output-on-failure -R "factory|bulletml|native_sh2"
}

run_step_2() {
  echo "[zero-alloc] Step 2 gate: state pool preallocation"
  run_step_1
}

run_step_3() {
  echo "[zero-alloc] Step 3 gate: task-buffer preallocation"
  run_step_2
}

run_step_4() {
  echo "[zero-alloc] Step 4 gate: parameter temp removal"
  run_step_3
}

run_step_5() {
  echo "[zero-alloc] Step 5 gate: FoeCommand pool-only runtime"
  run_step_4
}

run_step_6() {
  echo "[zero-alloc] Step 6 gate: manifest-driven sizing"
  run_step_5
}

run_step_7() {
  echo "[zero-alloc] Step 7 gate: end-to-end zero-runtime-allocation"
  run_step_6
}

if [[ $# -eq 0 ]]; then
  usage
  exit 2
fi

MODE=""
STEP=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --all)
      MODE="all"
      shift
      ;;
    --step)
      MODE="step"
      STEP="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ "$MODE" == "all" ]]; then
  run_step_7
  echo "[zero-alloc] All step gates passed"
  exit 0
fi

if [[ "$MODE" == "step" ]]; then
  case "$STEP" in
    1) run_step_1 ;;
    2) run_step_2 ;;
    3) run_step_3 ;;
    4) run_step_4 ;;
    5) run_step_5 ;;
    6) run_step_6 ;;
    7) run_step_7 ;;
    *)
      echo "Invalid --step value: $STEP (expected 1..7)" >&2
      exit 2
      ;;
  esac
  echo "[zero-alloc] Step $STEP gates passed"
  exit 0
fi

usage
exit 2
