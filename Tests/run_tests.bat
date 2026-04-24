@goto(){
  # Linux test runner script for SH2 unit tests
  # Usage: ./run_tests.bat [kronos|mednafen|USBGamers] [cue-path]

  if [ -z "$1" ]; then
    echo "Usage: $0 [kronos|mednafen|USBGamers] [cue-path]"
    exit 1
  fi

  cue_path="${2:-BuildDrop/noiz2sa_collision_ut.cue}"

  stop_emulator() {
    if [[ -n ${EMULATOR_PID:-} ]] && kill -0 "$EMULATOR_PID" 2>/dev/null; then
      kill -15 "$EMULATOR_PID" 2>/dev/null || true

      # Give the emulator a brief grace period to terminate cleanly.
      for _ in 1 2 3 4 5; do
        if ! kill -0 "$EMULATOR_PID" 2>/dev/null; then
          break
        fi
        sleep 1
      done

      if kill -0 "$EMULATOR_PID" 2>/dev/null; then
        kill -9 "$EMULATOR_PID" 2>/dev/null || true
      fi

      # Reap the child so no stdout/stderr capture stays open.
      wait "$EMULATOR_PID" 2>/dev/null || true
    fi
  }

  cleanup() {
    status=$?
    stop_emulator
    exit $status
  }

  trap cleanup EXIT

  prepare_mednafen_test_files() {
    local mednafen_base="${MEDNAFEN_BASE_DIR:-/opt/saturn/mednafen}"
    local pgconfig_dir="$mednafen_base/pgconfig"
    local cheats_dir="$mednafen_base/cheats"
    local cue_base

    cue_base="$(basename "$cue_path" .cue)"

    mkdir -p "$pgconfig_dir" "$cheats_dir"
    : > "$mednafen_base/ss.cfg"
    : > "$pgconfig_dir/${cue_base}.ss.cfg"
    : > "$cheats_dir/ss.cht"

    # Do not create an SBI placeholder: an empty SBI is treated as malformed
    # patch data by Mednafen and causes startup failure.
  }

  if [ "$1" = "mednafen" ]; then
    prepare_mednafen_test_files
    command="mednafen -sound 0 -ss.cart debug -force_module ss $cue_path"
  elif [ "$1" = "kronos" ]; then
    command="kronos -a -ns -i $cue_path"
  elif [ "$1" = "USBGamers" ]; then
    if [ ! -f cd/data/0.bin ]; then
      echo "ERROR: cd/data/0.bin not found. Build tests first."
      exit 1
    fi
    command="ftx -c"
    ftx -x cd/data/0.bin 0x06004000
  else
    echo "No valid emulator specified"
    exit 1
  fi

  log="${UT_LOG_FILE:-uts.log}"
  match="***UT_END***"

  echo "Test command: $command"
  $command > >(tee "$log") 2>&1 &
  EMULATOR_PID=$!

  TIMER_START=$(date +%s)
  TIMER_LIMIT=300

  while sleep 1
  do
      TIMER_NOW=$(date +%s)
      TIMER_ELAPSED=$((TIMER_NOW - TIMER_START))
      if [ $TIMER_ELAPSED -ge $TIMER_LIMIT ]; then
          echo "Test timed out after $TIMER_LIMIT seconds"
          stop_emulator
          exit 1
      fi

      if fgrep --quiet "$match" "$log"; then
          echo "Test completion marker found"
          stop_emulator
          echo "Tests completed successfully"
          exit 0
      fi

      if ! kill -0 $EMULATOR_PID 2>/dev/null; then
          echo "Emulator process has terminated unexpectedly"
          exit 1
      fi
  done
}

@goto $@
