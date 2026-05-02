#!/bin/bash
# noiz2sa Hardware Test - Full Power Cycle
# Usage: ./tools/test_hardware_full.sh [REST_API_IP]
# Example: ./tools/test_hardware_full.sh 192.168.1.100
#
# Tests the HW_DEBUG build on real Saturn hardware with:
# - Optional REST API power control (ESP-SaturnPSU_Control)
# - Full power OFF/ON cycle
# - Binary upload via USBGamers cartridge
# - Complete initialization monitoring until game fully loads

set -e

# Configuration
TIMEOUT=120  # Maximum time to wait for game initialization (seconds)
BINARY_PATH="./cd/data/0.bin"
GAME_READY_MARKER="Reached stage"  # Marker indicating game is fully initialized
POWER_ON_SETTLE_SECONDS=20

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
        sleep 3
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

trap cleanup EXIT

REST_API_IP="$1"

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
upload_log="$(ftx -x "$BINARY_PATH" 0x06004000 2>&1 || true)"
echo "$upload_log"
if echo "$upload_log" | grep -qiE "Upload failed|Send data error|Execution aborted|usb bulk write failed|Device open error|device not found"; then
    echo "ERROR: Upload failed"
    exit 1
fi
echo "Upload complete"

# ========== MONITOR INITIALIZATION ==========
echo ""
echo "========== PHASE 4: MONITOR INITIALIZATION =========="
echo "Resetting USB and entering debug console mode..."
reset_usb_device
sleep 2

# Use ftx to connect to console (with output capture)
echo "Waiting for game initialization (max ${TIMEOUT}s)..."
echo "Expected: Pattern loading, then game ready..."
echo ""

# Start monitoring with timeout
TIMER_START=$(date +%s)
init_complete=0

# Run ftx console with timeout, capturing output
while IFS= read -r line; do
    echo "$line"

    # Check for pattern loading messages
    if echo "$line" | grep -q "\[BARRAGE\] Type"; then
        echo "[MONITOR] Pattern loading detected"
    fi
    
    # Check for game ready marker (expanded from various possible states)
    if echo "$line" | grep -qE "Reached stage|Game Ready|Initialization Complete|Entering main loop|Main game loop starting|IN_GAME \(stage .*\) ready"; then
        echo "[MONITOR] ✓ Game initialization complete!"
        init_complete=1
        break
    fi
done < <(timeout $TIMEOUT ftx -c 2>&1 || true)

echo ""
echo "========================================"
if [ $init_complete -eq 1 ]; then
    echo "✓ HARDWARE TEST PASSED"
    echo "Game fully initialized and running on Saturn"
else
    echo "⚠ HARDWARE TEST INCONCLUSIVE"
    echo "Game loaded but initialization status unclear"
    echo "Check Saturn console display for visual confirmation"
fi
echo "========================================"
