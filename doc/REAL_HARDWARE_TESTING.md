# Real Hardware Testing

This document covers running noiz2sa on a physical Sega Saturn with a USBGamers cartridge and an ESP-SaturnPSU_Control power switch.

## Current PSU Endpoint

The PSU controller currently used in this workspace is:

- `http://192.168.1.106/`

The hardware test script accepts the IP as either a positional argument or via `SATURN_PSU_IP`.

## PSU REST API

The ESP-SaturnPSU_Control firmware exposes a small REST API. The hardware test flow relies on these endpoints:

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/status` | Read relay state and latch status |
| `POST` | `/api/v1/on` | Turn relay on |
| `POST` | `/api/v1/off` | Turn relay off |
| `POST` | `/api/v1/toggle` | Toggle relay state |
| `GET` | `/api/v1/latch` | Read current latch period |
| `POST` | `/api/v1/latch` | Set latch period (`0` disables it) |
| `POST` | `/api/v1/reset` | Clear latch and force relay low for test setup |
| `GET` | `/menu` | Plain-text status summary |

Example status request:

```bash
curl -sS http://192.168.1.106/api/v1/status
```

Expected JSON shape:

```json
{ "relay_status": "ON", "latch": 0 }
```

Example power-off request:

```bash
curl -sS -X POST http://192.168.1.106/api/v1/off
```

Example power-on request:

```bash
curl -sS -X POST http://192.168.1.106/api/v1/on
```

## Hardware Test Flow

The main end-to-end test script is [`tools/test_hardware_full.sh`](../tools/test_hardware_full.sh).

It performs:

1. Power off the Saturn through the PSU REST API.
2. Power on the Saturn through the PSU REST API.
3. Upload the `HW_DEBUG` binary through the USBGamers cartridge.
4. Monitor startup logs until the game is ready.
5. Watch gameplay for allocation/runtime violations.

Typical invocation:

```bash
./tools/test_hardware_full.sh 192.168.1.106
```

You can also set the IP once:

```bash
export SATURN_PSU_IP=192.168.1.106
./tools/test_hardware_full.sh
```

To control power only:

```bash
./tools/test_hardware_full.sh --power-status 192.168.1.106
./tools/test_hardware_full.sh --power-off 192.168.1.106
./tools/test_hardware_full.sh --power-on 192.168.1.106
./tools/test_hardware_full.sh --power-cycle 192.168.1.106
```

## Preflight Checklist

Before running on real hardware:

1. Build the `HW_DEBUG` target.
2. Confirm the PSU controller is reachable at `192.168.1.106`.
3. Confirm the USBGamers cartridge is connected and the host can access the USB device.
4. Verify the USB reset/uploader tools are installed and in `PATH`.

## Running SH2 Unit Tests On Real Hardware

The SH2 campaign scripts support `USBGamers` directly:

- `Tests/test_campaign.sh` (collision campaign)
- `Tests/test_factory_campaign.sh` (factory campaign)
- `Tests/test_bulletml_campaign.sh` (BulletML campaign)

Recommended stable sequence:

```bash
# 1) Clean hardware state
./tools/test_hardware_full.sh --power-cycle 192.168.1.106

# 2) Reset USB cartridge state
usbreset "FT245R USB FIFO"

# 3) Give Saturn a short settle window
sleep 10

# 4) Run campaign (choose one)
bash Tests/test_campaign.sh --emulator USBGamers --skip-build --strict
bash Tests/test_factory_campaign.sh --emulator USBGamers --skip-build --strict
bash Tests/test_bulletml_campaign.sh --emulator USBGamers --skip-build --strict
```

Important behavior notes:

- The BulletML SH2 campaign currently runs CD-independent smoke checks by default (`BULLETML_SKIP_CD_TESTS` enabled in build definitions).
- If `ftx -x` fails with `usb bulk write failed`, rerun the full sequence above (power-cycle + usbreset + short wait) before retrying the campaign.

## Troubleshooting

- If the PSU status request fails, verify the device is on the same network and the IP is correct.
- If USB upload fails with `device not found`, check the cartridge connection, FTDI driver, and permissions.
- If the Saturn powers on but no logs appear, confirm the debug cartridge is installed and the HW_DEBUG build was uploaded.
