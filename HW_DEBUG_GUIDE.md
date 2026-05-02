# Hardware Debug Mode (HW_DEBUG) Guide

## Overview

**HW_DEBUG** is a specialized build configuration for testing noiz2sa directly on real Sega Saturn hardware using the USBGamers cartridge and the ESP-SaturnPSU_Control device.

### Key Features

- **No CD-backed assets**: All BulletML patterns are embedded at compile time
- **No audio dependencies**: PCM SFX and CDDA BGM are disabled
- **Direct hardware output**: Logs output to DEV_CART (Saturn debug cartridge)
- **Instant boot**: Skips CD loading and jumps directly to an endless stage

### When to Use HW_DEBUG

Use HW_DEBUG mode when:
- Testing on real Saturn hardware with a development cartridge (USBGamers)
- Prototyping new features without CD image dependencies
- Debugging collision, rendering, or game logic issues in hardware context
- Running rapid iteration cycles without CD image build overhead

## Building HW_DEBUG

### Configuration

```bash
cmake -B build_hw_debug -DHW_DEBUG=ON

# Optional: Set which endless stage to boot (10-13)
# Default: 10 (Stage 1)
cmake -B build_hw_debug -DHW_DEBUG=ON -DHW_DEBUG_ENDLESS_STAGE=11
```

### Compile

```bash
cmake --build build_hw_debug
```

**Output**: `BuildDrop/noiz2sa.elf` + `BuildDrop/noiz2sa.bin`

### Configuration Details

When `HW_DEBUG=ON`, CMake automatically:
1. Disables sound subsystem (`NOIZ2SA_ENABLE_SOUND=OFF`)
2. Disables PCM SFX loading (`NOIZ2SA_ENABLE_PCM_SFX=OFF`)
3. Embeds all BulletML patterns (zako, middle, boss) at compile time
4. Sets log output to `DEV_CART` (hardware debug cartridge)
5. Configures logging for hardware environment

### Available Endless Stages

The `HW_DEBUG_ENDLESS_STAGE` option sets which stage to boot directly:

| Value | Stage |
|-------|-------|
| 10 | Stage 1 (Zako) |
| 11 | Stage 2 (Middle) |
| 12 | Stage 3 (Boss) |
| 13 | Stage 4 (Bonus) |

Example: Boot directly to Boss stage
```bash
cmake -B build_hw_debug -DHW_DEBUG=ON -DHW_DEBUG_ENDLESS_STAGE=12
cmake --build build_hw_debug
```

## Running on Real Hardware

### Prerequisites

1. **Saturn Hardware**: Sega Saturn console (NTSC or PAL)
2. **Development Cartridge**: USBGamers cartridge with USB connection
3. **ESP-SaturnPSU_Control Device**: Optional, for automated hardware control (reset/power)
4. **Host Tools**: 
   - SRL_INSTALL_ROOT properly configured
   - USB driver for USBGamers (CP2102 serial)
   - ESP device on local network (for REST API control)

### Launch on Hardware

Use the provided launch script:

```bash
./tools/run_on_saturn.bat
```

This script:
1. Verifies `BuildDrop/noiz2sa.elf` exists
2. Uses the SRL USBGamers uploader tool
3. Transfers the binary to the cartridge via USB
4. Boots the game on Saturn

**Expected Output**:
```
BuildDrop/noiz2sa.elf found
Uploading via USBGamers...
[device logs appear here]
```

### Monitor Hardware Output

Hardware debug output goes to the DEV_CART debug cartridge. Monitor output via:

```bash
# Via SRL serial monitor (if DEV_CART is serial-connected)
${SRL_INSTALL_ROOT}/tools/scripts/monitor.sh
```

or examine logs after running via the SRL logger tools.

## Hardware Control via ESP-SaturnPSU_Control

The optional **ESP-SaturnPSU_Control** device allows automated hardware control through a REST API. This is useful for automated test campaigns.

### Device Setup

See: https://github.com/willll/ESP-SaturnPSU_Control

The device provides:
- **GPIO5 (D1)**: Relay output to control Saturn power/reset
- **GPIO4 (D2)**: Physical button for manual control
- **REST API**: HTTP endpoints for remote control
- **Web UI**: Browser-based control panel

### REST API Endpoints

Assuming the device is at IP `192.168.1.100`:

#### Power Control

```bash
# Turn ON
curl -X POST http://192.168.1.100/api/v1/on
# Response: {"relay_status":"ON"}

# Turn OFF
curl -X POST http://192.168.1.100/api/v1/off
# Response: {"relay_status":"OFF"}

# Toggle
curl -X POST http://192.168.1.100/api/v1/toggle
# Response: {"relay_status":"ON"} or {"relay_status":"OFF"}
```

#### Check Status

```bash
curl http://192.168.1.100/api/v1/status
# Response: {"relay_status":"ON", "latch":0}
```

#### Latch Control (Prevent Rapid Toggles)

```bash
# Set 10-second latch (prevents toggles for 10s)
curl -X POST http://192.168.1.100/api/v1/latch -H "Content-Type: application/json" \
  -d '{"latch":10}'
# Response: {"latch":10}

# Get current latch period
curl http://192.168.1.100/api/v1/latch
# Response: {"latch":10}
```

#### Reset (Test Setup)

```bash
# Clear latch and set relay OFF
curl -X POST http://192.168.1.100/api/v1/reset
# Response: {"reset":true}
```

### Hardware-Assisted Test Campaign Example

A typical automated test flow:

```bash
#!/bin/bash
DEVICE_IP="192.168.1.100"

# Power ON Saturn
curl -X POST http://${DEVICE_IP}/api/v1/on
sleep 2

# Upload and boot HW_DEBUG build
./tools/run_on_saturn.bat

# Wait for test to complete (via log monitoring)
sleep 30

# Power OFF Saturn for next iteration
curl -X POST http://${DEVICE_IP}/api/v1/off
sleep 1
```

### Latch Behavior

The device supports a **latch period** to prevent rapid toggling (useful for relay protection):

- **Latch Period**: Number of seconds during which toggle commands are rejected
- **Range**: 1-3600 seconds (0 = disabled)
- **Response when locked**: HTTP 423 with `{"error":"Latch active"}`

Example: Set 5-second latch to protect relay:
```bash
curl -X POST http://192.168.1.100/api/v1/latch -H "Content-Type: application/json" \
  -d '{"latch":5}'
```

## Debugging HW_DEBUG Builds

### Common Issues

#### "BuildDrop does not exist"
```bash
# Ensure build completed successfully
cmake --build build_hw_debug
```

#### "SRL_INSTALL_ROOT not set"
```bash
# Set environment variable
export SRL_INSTALL_ROOT=/saturn/SaturnRingLib
```

#### Device upload fails (USB connection)
1. Check USBGamers cartridge is properly connected
2. Verify USB driver: `ls -la /dev/ttyUSB*`
3. Ensure user has serial permissions: `groups` should include `dialout`
4. Try manual upload:
   ```bash
   ${SRL_INSTALL_ROOT}/tools/scripts/run.sh USBGamers BuildDrop/noiz2sa.elf
   ```

### Debug Output

HW_DEBUG builds log to DEV_CART by default. To see logs:

1. **Via serial monitor** (if available):
   ```bash
   mednafen BuildDrop/noiz2sa.bin  # Emulated
   ```

2. **Via SRL logger** (hardware):
   ```bash
   # Depends on your DEV_CART configuration
   # Check SRL_INSTALL_ROOT/tools for logger utilities
   ```

## Performance Notes

HW_DEBUG builds typically have **identical performance** to normal builds because:
- BulletML patterns are pre-compiled (same as loaded from CD)
- No I/O overhead for asset loading
- Identical render and physics code paths
- Logging overhead is minimal (DEV_CART is asynchronous)

## Configuration Comparison

| Aspect | Normal Build | HW_DEBUG Build |
|--------|-------------|----------------|
| CD Image | Required | Not needed |
| Audio | Enabled | Disabled |
| BulletML | Loaded from CD | Embedded |
| Boot Time | ~5-10s (CD load) | ~1s (RAM) |
| Log Output | Emulator/Serial | DEV_CART |
| Testing | CD emulators | Real hardware |

## Related Documentation

- [BUILD_GUIDE.md](BUILD_GUIDE.md) — All CMake configuration options
- [AGENTS.md](AGENTS.md) — Development environment setup
- [doc/DRAWING_SYSTEM.md](doc/DRAWING_SYSTEM.md) — Rendering pipeline
- https://github.com/willll/ESP-SaturnPSU_Control — Hardware control device
