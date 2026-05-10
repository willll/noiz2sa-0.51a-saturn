#!/bin/bash
# noiz2sa Hardware Test - Full Power Cycle / PSU Control
# Usage:
#   ./tools/test_hardware_full.sh [REST_API_IP]
#   ./tools/test_hardware_full.sh --power-off [REST_API_IP]
#   ./tools/test_hardware_full.sh --power-on [REST_API_IP]
#   ./tools/test_hardware_full.sh --power-cycle [REST_API_IP]
#   ./tools/test_hardware_full.sh --power-status [REST_API_IP]
#
# You can also provide SATURN_PSU_IP instead of positional REST_API_IP.
# Examples:
#   ./tools/test_hardware_full.sh 192.168.1.100
#   SATURN_PSU_IP=192.168.1.100 ./tools/test_hardware_full.sh --power-cycle
#
# Tests the HW_DEBUG build on real Saturn hardware with:
# - Optional REST API power control (ESP-SaturnPSU_Control)
# - Full power OFF/ON cycle
# - Binary upload via USBGamers cartridge
# - Complete initialization monitoring until game fully loads

set -e

MODE="full-test"

print_usage() {
        cat <<'EOF'
Usage:
    ./tools/test_hardware_full.sh [REST_API_IP]
    ./tools/test_hardware_full.sh --power-off [REST_API_IP]
    ./tools/test_hardware_full.sh --power-on [REST_API_IP]
    ./tools/test_hardware_full.sh --power-cycle [REST_API_IP]
    ./tools/test_hardware_full.sh --power-status [REST_API_IP]

Environment:
    SATURN_PSU_IP              Optional REST API IP for PSU controller
    TIMEOUT                    Init timeout for full test (default 90)
    GAMEPLAY_MONITOR_SECONDS   Gameplay monitor window (default 90)
    POWER_ON_SETTLE_SECONDS    Settle delay after power on (default 10)
EOF
}

# Configuration
TIMEOUT="${TIMEOUT:-90}"               # Maximum time to wait for game initialization (seconds)
GAMEPLAY_MONITOR_SECONDS="${GAMEPLAY_MONITOR_SECONDS:-90}"  # Seconds to monitor gameplay after init for alloc violations
BINARY_PATH="./cd/data/0.bin"
GAME_READY_MARKER="Reached stage"  # Marker indicating game is fully initialized
POWER_ON_SETTLE_SECONDS="${POWER_ON_SETTLE_SECONDS:-10}"

cleanup() {
    status=$?
    echo ""
    echo "========================================"
    echo "Test cleanup (exit code: $status)"
    echo "========================================"
    exit $status
}

reset_usb_device() {
    echo "Resetting USB device..."
    if ! usbreset "FT245R USB FIFO" 2>/dev/null; then
        echo "  (USB reset skipped or tool not available)"
    else
        echo "  USB device reset successfully"
    fi
}

power_control() {
    local action=$1  # "on" or "off"
    local ip=$2

    if [ -z "$ip" ]; then
        echo "  Power control not available (no REST API IP specified)"
        echo "  Please manually power $action the Saturn console"
        if [ "$action" = "on" ]; then
            read -r -p "  Press Enter when ready: " || true
        fi
        return 0
    fi

    echo "Sending power $action command to $ip..."
    
    # Check if REST API is reachable
    if ! curl -s --connect-timeout 3 "http://$ip/api/v1/status" > /dev/null 2>&1; then
        echo "  ERROR: REST API not reachable at $ip"
        echo "  Please manually power $action the Saturn console"
        if [ "$action" = "on" ]; then
            read -r -p "  Press Enter when ready: " || true
        fi
        return 1
    fi

    if [ "$action" = "off" ]; then
        curl -s -X POST "http://$ip/api/v1/off" > /dev/null 2>&1 || true
        sleep 1
        relay_status=$(curl -s "http://$ip/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
        if [ "$relay_status" != "OFF" ]; then
            # Some firmware revisions require toggle when direct off doesn't stick.
            curl -s -X POST "http://$ip/api/v1/toggle" > /dev/null 2>&1 || true
            sleep 1
            relay_status=$(curl -s "http://$ip/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
        fi
        echo "  Relay status after OFF command: ${relay_status:-UNKNOWN}"
        if [ "$relay_status" != "OFF" ]; then
            echo "  ERROR: Failed to power OFF system"
            return 1
        fi
        echo "  Waiting 12s for capacitors to discharge..."
        sleep 12
    elif [ "$action" = "on" ]; then
        curl -s -X POST "http://$ip/api/v1/on" > /dev/null 2>&1 || true
        sleep 3

        # Query relay status to confirm; try toggle fallback if still OFF.
        relay_status=$(curl -s "http://$ip/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
        if [ "$relay_status" != "ON" ]; then
            curl -s -X POST "http://$ip/api/v1/toggle" > /dev/null 2>&1 || true
            sleep 1
            relay_status=$(curl -s "http://$ip/api/v1/status" 2>/dev/null | grep -o '"relay_status":"[A-Z]*"' | cut -d'"' -f4)
        fi
        echo "  Relay status after ON command: ${relay_status:-UNKNOWN}"
        if [ "$relay_status" != "ON" ]; then
            echo "  ERROR: Failed to power ON system"
            return 1
        fi
    fi
    
    return 0
}

power_status() {
    local ip=$1

    if [ -z "$ip" ]; then
        echo "ERROR: No REST API IP specified"
        echo "Provide an IP as argument or set SATURN_PSU_IP"
        return 1
    fi

    echo "Querying PSU status from $ip..."
    if ! curl -s --connect-timeout 3 "http://$ip/api/v1/status"; then
        echo ""
        echo "ERROR: Could not query PSU status"
        return 1
    fi
    echo ""
    return 0
}

trap cleanup EXIT

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    print_usage
    exit 0
fi

if [ "$1" = "--power-off" ] || [ "$1" = "--power-on" ] || [ "$1" = "--power-cycle" ] || [ "$1" = "--power-status" ]; then
    MODE="${1#--power-}"
    shift
fi

REST_API_IP="${1:-${SATURN_PSU_IP:-}}"

if [ "$MODE" != "full-test" ]; then
    echo "========================================"
    echo "noiz2sa PSU Control"
    echo "========================================"
    [ -n "$REST_API_IP" ] && echo "REST API IP: $REST_API_IP"
    echo "Mode: $MODE"

    case "$MODE" in
        off)
            power_control "off" "$REST_API_IP"
            ;;
        on)
            power_control "on" "$REST_API_IP"
            ;;
        cycle)
            power_control "off" "$REST_API_IP"
            power_control "on" "$REST_API_IP"
            ;;
        status)
            power_status "$REST_API_IP"
            ;;
    esac

    echo "Done"
    exit 0
fi

echo "========================================"
echo "noiz2sa Hardware Full Test"
echo "========================================"

# Precondition check
if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: $BINARY_PATH not found"
    echo "Please build the HW_DEBUG binary first:"
    echo "  cmake -B build_hw_debug -DHW_DEBUG=ON"
    echo "  cmake --build build_hw_debug"
    exit 1
fi

echo ""
echo "Binary: $(file -b $BINARY_PATH | cut -c1-50)..."
echo "Size: $(ls -lh $BINARY_PATH | awk '{print $5}')"

if [ -n "$REST_API_IP" ]; then
    echo "REST API IP: $REST_API_IP"
fi

# ========== POWER OFF ==========
echo ""
echo "========== PHASE 1: POWER OFF =========="
power_control "off" "$REST_API_IP"

# ========== POWER ON ==========
echo ""
echo "========== PHASE 2: POWER ON =========="
power_control "on" "$REST_API_IP"

# ========== UPLOAD ==========
echo ""
echo "========== PHASE 3: UPLOAD BINARY =========="
echo "Waiting ${POWER_ON_SETTLE_SECONDS}s for hardware to stabilize after power-on..."
sleep "$POWER_ON_SETTLE_SECONDS"
reset_usb_device
sleep 2

echo "Uploading HW_DEBUG binary via USBGamers cartridge..."
upload_ok=0
for attempt in 1 2 3; do
    upload_log="$(ftx -x "$BINARY_PATH" 0x06004000 2>&1 || true)"
    echo "[attempt $attempt] $upload_log"
    if echo "$upload_log" | grep -qiE "Upload failed|Send data error|Execution aborted|usb bulk write failed|Device open error|device not found"; then
        echo "Upload attempt $attempt failed — resetting USB and retrying in 5s..."
        reset_usb_device
        sleep 5
    else
        upload_ok=1
        break
    fi
done
if [ $upload_ok -eq 0 ]; then
    echo "ERROR: Upload failed after 3 attempts"
    exit 1
fi
echo "Upload complete"

# ========== MONITOR INITIALIZATION + ALLOC STRESS ==========
echo ""
echo "========== PHASE 4: MONITOR INITIALIZATION =========="
# Connect console immediately after upload — no USB reset, minimal sleep
# (matching run.sh: sleep 1 then ftx -c). A usbreset here adds 5+ seconds
# of delay and causes boot log lines to be missed.
sleep 1
echo "Connecting debug console (max init=${TIMEOUT}s + stress=${GAMEPLAY_MONITOR_SECONDS}s)..."
echo ""

TIMER_START=$(date +%s)
init_complete=0
init_time=0
alloc_baseline_seen=0
alloc_violations=0
alloc_violation_lines=""

TOTAL_TIMEOUT=$(( TIMEOUT + GAMEPLAY_MONITOR_SECONDS ))

while IFS= read -r line; do
    NOW=$(date +%s)
    echo "$line"

    if [ $init_complete -eq 0 ]; then
        if echo "$line" | grep -q "\[BARRAGE\] Type"; then
            echo "[MONITOR] Pattern loading detected"
        fi
        if echo "$line" | grep -qE "Reached stage|Game Ready|Initialization Complete|Entering main loop|Main game loop starting|IN_GAME \(stage .*\) ready"; then
            echo "[MONITOR] ✓ Game initialization complete — starting alloc-stress window (${GAMEPLAY_MONITOR_SECONDS}s)"
            init_complete=1
            init_time=$NOW
        fi
        if [ $(( NOW - TIMER_START )) -ge $TIMEOUT ]; then
            echo "[MONITOR] Init timeout reached without ready marker"
            break
        fi
    else
        if echo "$line" | grep -q "\[ALLOC_STRESS\] baseline captured"; then
            echo "[ALLOC_STRESS] ✓ Baseline snapshot captured"
            alloc_baseline_seen=1
        fi
        if echo "$line" | grep -q "\[ALLOC_STRESS\] RUNTIME ALLOC DURING GAMEPLAY"; then
            echo "[ALLOC_STRESS] ✗ VIOLATION DETECTED: $line"
            alloc_violations=$(( alloc_violations + 1 ))
            alloc_violation_lines="${alloc_violation_lines}  ${line}\n"
        fi
        if [ $(( NOW - init_time )) -ge $GAMEPLAY_MONITOR_SECONDS ]; then
            echo "[MONITOR] Alloc-stress window complete"
            break
        fi
    fi
done < <(timeout -s TERM -k 5 "$TOTAL_TIMEOUT" ftx -c 2>&1 || true)

echo ""
echo "========================================"
echo "TEST RESULTS"
echo "========================================"
if [ $init_complete -eq 0 ]; then
    echo "⚠  HARDWARE TEST INCONCLUSIVE"
    echo "   Game did not reach IN_GAME within ${TIMEOUT}s"
    echo "   Check Saturn console display for visual confirmation"
    exit 2
fi
echo "Init:     PASS (game reached IN_GAME)"
if [ $alloc_baseline_seen -eq 0 ]; then
    echo "Baseline: WARNING — [ALLOC_STRESS] baseline log line not seen"
    echo "          (HW_DEBUG build may not have been used, or log was cut off)"
else
    echo "Baseline: PASS (snapshot captured at IN_GAME entry)"
fi
if [ $alloc_violations -eq 0 ]; then
    echo "Alloc:    PASS — 0 runtime allocation violations in ${GAMEPLAY_MONITOR_SECONDS}s gameplay window"
    echo ""
    echo "✓ HARDWARE STRESS TEST PASSED"
else
    echo "Alloc:    FAIL — ${alloc_violations} runtime allocation violation(s) detected:"
    printf '%b' "$alloc_violation_lines"
    echo ""
    echo "✗ HARDWARE STRESS TEST FAILED"
    exit 1
fi
echo "========================================"
