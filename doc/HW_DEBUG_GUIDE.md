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

## Hardware Full-Test Script

For automated end-to-end testing on real hardware with power control, use the provided test script:

```bash
./tools/test_hardware_full.sh <PSU_IP_ADDRESS>
```

For the PSU controller currently used in this workspace, the IP is:

```text
http://192.168.1.106/
```

See [REAL_HARDWARE_TESTING.md](REAL_HARDWARE_TESTING.md) for the REST API details and the full power-cycle workflow.

### What the Test Script Does

The `test_hardware_full.sh` script orchestrates a complete hardware test cycle:

1. **Power OFF Phase** - Safely power down the Saturn via REST API (allows capacitor discharge)
2. **Power ON Phase** - Restore power to the console
3. **Binary Upload Phase** - Upload the HW_DEBUG build via USB-GamersCartridge
4. **Initialization Monitoring** - Captures DEV_CART console output during startup
5. **Allocation Stress Window** - Runs 120 seconds of continuous gameplay, monitoring for allocation violations
6. **Test Results** - Reports pass/fail verdict for Init, Alloc, and Runtime violations

### Usage

```bash
# Build HW_DEBUG first
cmake -B build_hw_debug -DHW_DEBUG=ON
cmake --build build_hw_debug

# Run full hardware test (requires ESP-SaturnPSU_Control at 192.168.1.106)
./tools/test_hardware_full.sh 192.168.1.106
```

The script will:
- Power cycle the Saturn automatically
- Upload `BuildDrop/noiz2sa.elf` via USB
- Monitor console output and check for startup errors
- Run 120-second gameplay window to verify stable allocation behavior
- Display final test verdict (PASS or FAIL)

### PSU Endpoint Configuration

The script expects the ESP-SaturnPSU_Control device to be reachable at the provided IP:

```bash
curl -sS http://192.168.1.106/api/v1/status  # Should return {"relay_status":"...", "latch":...}
```

If the device is unreachable, ensure:
- Device is powered on and on the same network
- Network cable is connected
- Firewall allows HTTP on port 80
- Check device IP via router or device web UI (typically at `http://<ip>/`)

### Example Output

Successful test run:

```
========== PHASE 1: POWER OFF ==========
Relay status after OFF command: OFF
Waiting 12s for capacitors to discharge...

========== PHASE 2: POWER ON ==========
Relay status after ON command: ON

========== PHASE 3: UPLOAD BINARY ==========
Waiting 30s for hardware to stabilize after power-on...
USB device reset successfully
[attempt 1] Upload complete.

========== PHASE 4: MONITOR INITIALIZATION ==========
[InitComms] FTDI device initialized successfully.
[DoConsole] Entering debug console mode...
INFO : [TRACE] initSDL: surfaces bound
INFO : [TRACE] initSDL: palette done
INFO : [TRACE] initSDL: panel/smoke done
INFO : [TRACE] initSDL: complete
INFO : [HW_DEBUG] Skipping CD-backed sprite loading
INFO : [MAIN] Sound disabled for startup
INFO : [TRACE] main: initFirst begin
INFO : [INIT] First initialization starting
INFO : [BARRAGE] Type 0: Loaded 18 patterns
INFO : [BARRAGE] Type 1: Loaded 23 patterns
INFO : [BARRAGE] Type 2: Loaded 32 patterns
INFO : [INIT] First initialization complete
INFO : [HW_DEBUG] Skipping title/menu and booting directly into endless INSANE stage 10
INFO : [STATE] Entering IN_GAME (stage 10)
INFO : [BACKGROUND] NBG0 VRAM addr=0x25e20000 size=262144
INFO : [BACKGROUND] Allocating CPU background buffers (minimal mode)
INFO : [BACKGROUND] bg buffers allocated OK
INFO : [BACKGROUND] NBG0 enabled priority=2 transparent=off base=0xffff
INFO : [STATE] IN_GAME (stage 10) ready - Starting gameplay
[MONITOR] ✓ Game initialization complete — starting alloc-stress window (120s)
INFO : [GAMEPAD] Gamepad initialized successfully
INFO : [MAIN] Main game loop starting
[MONITOR] Alloc-stress window complete

========================================
TEST RESULTS
========================================
Init:     PASS (game reached IN_GAME)
Alloc:    PASS — 0 runtime allocation violations in 120s gameplay window

✓ HARDWARE STRESS TEST PASSED
========================================
```

## DEV_CART Console Output Interpretation

When running on real hardware, DEV_CART console output provides detailed startup traces. Key markers to watch:

### Initialization Sequence

| Log Message | What It Means | Status |
|---|---|---|
| `[TRACE] initSDL: SDL_Init(video) done` | SDL video subsystem initialized | ✅ Normal |
| `[TRACE] initSDL: surfaces bound` | Frame buffer surfaces created | ✅ Normal |
| `[TRACE] initSDL: palette done` | Color palette initialized | ✅ Normal |
| `[TRACE] initSDL: panel/smoke done` | UI panels and effects loaded | ✅ Normal |
| `[HW_DEBUG] Skipping CD-backed sprite loading` | CD sprites not loaded (expected in HW_DEBUG) | ✅ Normal |
| `[TRACE] main: initFirst begin` | Game state initialization started | ✅ Normal |
| `[BARRAGE] Type 0: Loaded N patterns` | Enemy patterns embedded in binary | ✅ Normal |
| `[STATE] Entering IN_GAME (stage 10)` | Game reached playable state | ✅ **Success** |
| `[STATE] IN_GAME (stage 10) ready - Starting gameplay` | Game loop active, ready for input | ✅ **Success** |
| `[GAMEPAD] Gamepad initialized successfully` | Controller detected and ready | ✅ Normal |
| `[MAIN] Main game loop starting` | Main loop ticking (game is running) | ✅ **Success** |

### Error/Warning Messages

| Log Message | What It Means | Action |
|---|---|---|
| `ERROR: HighWorkRam allocation failed` | Out of work RAM | 🔴 **FAIL** — Insufficient VRAM or work RAM |
| `ERROR: Allocation violation detected` | Heap corruption or double-free | 🔴 **FAIL** — Memory management bug |
| `[WARN] Pattern load failed: boss/pattern_01.blb` | Embedded pattern missing/corrupted | 🔴 **FAIL** — Rebuild with correct BulletML |
| `[SYNC_HANG]` or no output for >10s | Synchronization stall (emulator-only issue) | ⚠️ **Skip on hardware** — Use test script instead |

### Performance Indicators

Watch for these in the gameplay window:

```
INFO : [PERF_US] Frame: 16.5ms (60 FPS target)
INFO : [BLIT_US] Render: 14.3ms
INFO : [ALLOC] HighWorkRam: 2048 KB free
```

- **Frame time < 20ms**: Good performance (60 FPS)
- **Render time < 15ms**: Rendering efficient
- **Free VRAM decreasing over time**: Check for leaks

## Allocation Stress Testing

The hardware test script runs a **120-second continuous gameplay window** to validate allocation stability. This tests:

1. **Enemy spawning** under high pressure (allocates/deallocates bullets and patterns)
2. **Background updates** (VRAM reallocation for scrolling)
3. **Particle effects** (smoke, explosions if enabled)
4. **Score and bonus systems** (runtime memory churn)

### Pass Criteria

✅ **PASS** when:
- Game reaches `IN_GAME` state without crashes
- No allocation violations logged in 120s window
- Main loop continues ticking throughout
- Render performance stays consistent (no frame glitches)

❌ **FAIL** when:
- Hang/stall during initialization (no output for >10s)
- `ERROR: Allocation violation` appears in console
- Crash (console stops outputting mid-gameplay)
- Gameplay loop stops before 120s window ends

### Manual Stress Test (Without Script)

If you want to manually monitor during gameplay:

1. Upload build via USB: `./tools/run_on_saturn.bat`
2. Monitor console in separate terminal: `${SRL_INSTALL_ROOT}/tools/scripts/monitor.sh`
3. Play continuously for 120+ seconds
4. Watch for allocation warnings or crashes

```bash
# Terminal 1: Upload
./tools/run_on_saturn.bat

# Terminal 2: Monitor (in parallel)
${SRL_INSTALL_ROOT}/tools/scripts/monitor.sh

# Play for ~2 minutes, then check for violations
```

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

Assuming the device is at IP `192.168.1.100` (or your actual device IP, e.g., `192.168.1.106`):

#### Power Control

```bash
# Turn ON
curl -X POST http://192.168.1.106/api/v1/on
# Response: {"relay_status":"ON"}

# Turn OFF
curl -X POST http://192.168.1.106/api/v1/off
# Response: {"relay_status":"OFF"}

# Toggle
curl -X POST http://192.168.1.106/api/v1/toggle
# Response: {"relay_status":"ON"} or {"relay_status":"OFF"}
```

#### Check Status

```bash
curl http://192.168.1.106/api/v1/status
# Response: {"relay_status":"ON", "latch":0}
```

#### Latch Control (Prevent Rapid Toggles)

```bash
# Set 10-second latch (prevents toggles for 10s)
curl -X POST http://192.168.1.106/api/v1/latch -H "Content-Type: application/json" \
  -d '{"latch":10}'
# Response: {"latch":10}

# Get current latch period
curl http://192.168.1.106/api/v1/latch
# Response: {"latch":10}
```

#### Reset (Test Setup)

```bash
# Clear latch and set relay OFF
curl -X POST http://192.168.1.106/api/v1/reset
# Response: {"reset":true}
```

### Hardware-Assisted Test Campaign Example

A typical automated test flow:

```bash
#!/bin/bash
DEVICE_IP="192.168.1.106"  # Your ESP-SaturnPSU_Control IP

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

Or simply use the provided test script:

```bash
./tools/test_hardware_full.sh 192.168.1.106  # Orchestrates entire cycle
```

### Latch Behavior

The device supports a **latch period** to prevent rapid toggling (useful for relay protection):

- **Latch Period**: Number of seconds during which toggle commands are rejected
- **Range**: 1-3600 seconds (0 = disabled)
- **Response when locked**: HTTP 423 with `{"error":"Latch active"}`

Example: Set 5-second latch to protect relay:
```bash
curl -X POST http://192.168.1.106/api/v1/latch -H "Content-Type: application/json" \
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
