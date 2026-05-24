#!/bin/bash
# Automated Mednafen Hi-Score Persistence End-to-End Test
# This script tests that hi-scores are correctly saved and loaded across game reboots
# 
# Prerequisites:
#   - Non-HW_DEBUG build at build-mednafen/ 
#   - Mednafen emulator installed at /opt/saturn/mednafen/
#   - CUE file built from `cd BuildDrop/ && make -f Makefile_buildDrop_mednafen_noiz2sa`

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Go up one level to the workspace root
WORKSPACE_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to workspace root for relative path resolution
cd "$WORKSPACE_ROOT"

# Configuration
readonly MEDNAFEN_BASE="${MEDNAFEN_BASE_DIR:-/opt/saturn/mednafen}"
readonly BUILD_DIR="./build-mednafen"
readonly CUE_PATH="./BuildDrop/noiz2sa.cue"
readonly BINARY_PATH="./cd/data/0.bin"
readonly HISCORE_TEST_LOG="hiscore_e2e_test.log"
readonly MEDNAFEN_LOG="mednafen_hiscore_e2e.log"
readonly SAV_DIR="${MEDNAFEN_BASE}/sav"
readonly STARTUP_TIMEOUT_SECONDS="${STARTUP_TIMEOUT_SECONDS:-180}"
readonly SESSION1_SECONDS="${SESSION1_SECONDS:-90}"
readonly SESSION2_SECONDS="${SESSION2_SECONDS:-60}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

cleanup() {
    local status=$?
    if [[ -n ${MEDNAFEN_PID:-} ]] && kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
        log_info "Terminating Mednafen (PID: $MEDNAFEN_PID)"
        kill -15 "$MEDNAFEN_PID" 2>/dev/null || true
        
        # Wait briefly for graceful shutdown
        for i in {1..5}; do
            if ! kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
                break
            fi
            sleep 1
        done
        
        # Force kill if still running
        if kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
            kill -9 "$MEDNAFEN_PID" 2>/dev/null || true
        fi
        
        wait "$MEDNAFEN_PID" 2>/dev/null || true
    fi
    
    exit $status
}

trap cleanup EXIT

# Validation
if [[ ! -f "$CUE_PATH" ]]; then
    log_error "CUE file not found: $CUE_PATH"
    log_error "Please build the non-HW_DEBUG binary first:"
    log_error "  cd build-mednafen && cmake --build . && cd BuildDrop && make -f Makefile_buildDrop_mednafen_noiz2sa"
    exit 1
fi

if [[ ! -d "$MEDNAFEN_BASE" ]]; then
    log_error "Mednafen directory not found: $MEDNAFEN_BASE"
    exit 1
fi

# Create necessary directories
mkdir -p "$SAV_DIR" "${MEDNAFEN_BASE}/pgconfig" "${MEDNAFEN_BASE}/cheats"

# Prepare Mednafen config files
log_info "Preparing Mednafen configuration..."
: > "${MEDNAFEN_BASE}/ss.cfg"
: > "${MEDNAFEN_BASE}/pgconfig/noiz2sa.ss.cfg"
: > "${MEDNAFEN_BASE}/cheats/ss.cht"

# Function to find the current backup RAM file
find_main_game_backup_file() {
    # Main game backup files are named like: noiz2sa.<hash>.bkr
    find "$SAV_DIR" -maxdepth 1 -type f -name "noiz2sa.*.bkr" -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | awk '{print $2}'
}

# Function: Run game and simulate some gameplay
run_game_session() {
    local session_num="$1"
    local session_duration="${2:-30}"
    
    log_info "=== Hi-Score Persistence Test - Session $session_num ==="
    log_info "Starting Mednafen with hi-score enabled..."
    
    # Clear previous logs
    rm -f "$MEDNAFEN_LOG"
    
    # Start Mednafen with debug cart and sound disabled
    MEDNAFEN_ALLOWMULTI=1 mednafen -sound 0 -ss.cart debug -force_module ss "$CUE_PATH" > >(tee -a "$MEDNAFEN_LOG") 2>&1 &
    MEDNAFEN_PID=$!
    
    log_info "Mednafen started (PID: $MEDNAFEN_PID)"
    
    # Wait for game to initialize (look for "Noiz2sa startup" log)
    local timeout="$STARTUP_TIMEOUT_SECONDS"
    local elapsed=0
    while [[ $elapsed -lt $timeout ]]; do
        if grep -q "Noiz2sa startup" "$MEDNAFEN_LOG" 2>/dev/null; then
            log_info "Game startup detected"
            break
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    
    if [[ $elapsed -ge $timeout ]]; then
        log_warn "Game did not fully initialize within ${timeout}s (timeout)"
    fi
    
    # Let game run for a bit to potentially increment score
    log_info "Letting game run for ${session_duration}s (score may change in gameplay)..."
    sleep "$session_duration"
    
    # Check if game is still running and hi-score logs appeared
    if ! kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
        log_warn "Game process terminated unexpectedly"
    fi
    
    # Try to get a clean shutdown by sending SIGTERM
    log_info "Cleanly shutting down game (triggering save)..."
    kill -15 "$MEDNAFEN_PID" 2>/dev/null || true
    
    # Wait for graceful shutdown with save
    local shutdown_timeout=10
    for i in $(seq 1 $shutdown_timeout); do
        if ! kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
            log_info "Game shutdown complete after ${i}s"
            wait "$MEDNAFEN_PID" 2>/dev/null || true
            MEDNAFEN_PID=""
            return 0
        fi
        sleep 1
    done
    
    # Force kill if needed
    if [[ -n ${MEDNAFEN_PID:-} ]] && kill -0 "$MEDNAFEN_PID" 2>/dev/null; then
        log_warn "Forcing game shutdown..."
        kill -9 "$MEDNAFEN_PID" 2>/dev/null || true
        wait "$MEDNAFEN_PID" 2>/dev/null || true
    fi
    MEDNAFEN_PID=""
}

# Test: Save-Load Cycle
test_save_load_cycle() {
    log_info "===================== TEST: Save-Load Cycle ====================="
    
    local backup_before=""
    local backup_after=""

    backup_before="$(find_main_game_backup_file || true)"
    if [[ -n "$backup_before" ]]; then
        log_info "Existing main-game backup file: $backup_before"
    else
        log_info "No existing main-game backup file found before test"
    fi

    # Session 1: Fresh game (may have hi-score from earlier)
    run_game_session 1 "$SESSION1_SECONDS"
    
    # Check for backup file
    local backup_file
    backup_file="$(find_main_game_backup_file || true)"
    backup_after="$backup_file"
    
    if [[ -z "$backup_file" ]]; then
        log_warn "No main-game backup file found after first session"
        backup_file="$(find "$SAV_DIR" -maxdepth 1 -type f -name "noiz2sa.*.bkr" | head -1)"
        if [[ -z "$backup_file" ]]; then
            log_error "No hi-score backup file exists at all"
            return 1
        fi
        log_info "Using existing backup file: $backup_file"
    else
        log_info "Backup file created/updated: $backup_file"
    fi
    
    # Check if backup file contains the hi-score header
    if file "$backup_file" | grep -q "data"; then
        log_info "Backup file exists and contains data"
        hexdump -C "$backup_file" | head -5
    fi

    if [[ -n "$backup_before" && -n "$backup_after" && "$backup_before" == "$backup_after" ]]; then
        log_warn "Main-game backup filename did not change (this can still be valid if file was updated in-place)"
    fi
    
    # Session 2: Reload the game and verify hi-score was loaded
    log_info "--- Restarting game to verify hi-score load ---"
    run_game_session 2 "$SESSION2_SECONDS"
    
    # Check logs for hi-score load indication
    local load_found=0
    if grep -q "\[HISCORE\] Loaded" "$MEDNAFEN_LOG" 2>/dev/null; then
        log_info "✓ Hi-score load confirmed in logs: [HISCORE] Loaded"
        load_found=1
    elif grep -q "Loaded hi-score" "$MEDNAFEN_LOG" 2>/dev/null; then
        log_info "✓ Hi-score load confirmed in logs: Loaded hi-score"
        load_found=1
    else
        log_warn "No hi-score load log message found in Mednafen output"
        log_info "Searching for any HISCORE-related logs..."
        grep -i hiscore "$MEDNAFEN_LOG" 2>/dev/null || log_warn "No HISCORE logs found"
    fi
    
    return $((1 - load_found))
}

# Main execution
main() {
    log_info "Hi-Score Persistence Automated Test"
    log_info "===================================="
    log_info "CUE: $CUE_PATH"
    log_info "Mednafen: $MEDNAFEN_BASE"
    log_info "SAV Dir: $SAV_DIR"
    log_info "Startup timeout: ${STARTUP_TIMEOUT_SECONDS}s"
    log_info "Session durations: ${SESSION1_SECONDS}s then ${SESSION2_SECONDS}s"
    log_info ""
    
    # Run the test
    if test_save_load_cycle; then
        log_info ""
        log_info "===================================="
        log_info "✓ TEST PASSED: Hi-score persistence verified!"
        log_info "===================================="
        return 0
    else
        log_info ""
        log_info "===================================="
        log_error "✗ TEST INCONCLUSIVE: Could not verify hi-score load"
        log_info "===================================="
        log_info "Check $MEDNAFEN_LOG for details"
        return 1
    fi
}

main
