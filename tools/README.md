# Noiz2sa Tools

This directory contains utility tools for the Noiz2sa Saturn port.

## Emulator Launch Scripts

- `run_mednafen.sh`: Launches Mednafen with the default image `BuildDrop/noiz2sa.cue`
- `run_kronos.sh`: Launches Kronos with `QT_QPA_PLATFORM="xcb"` and `BuildDrop/noiz2sa.cue`
- `run_on_saturn.bat`: Polyglot script to launch on Saturn hardware via USBGamers (requires SRL_INSTALL_ROOT)

## Emulator Campaign Logging

When running the SH2 campaign scripts on an emulator, keep `SRL_LOG_OUTPUT=EMULATOR` enabled so the test harness can see runtime traces and completion markers in the emulator log.

## USBGamers Campaign Preflight

Before running campaign scripts with `--emulator USBGamers`, use this recovery/probe sequence:

```bash
# Power-cycle Saturn first (manual or PSU controller)
usbreset "FT245R USB FIFO"
sleep 2
ftx -c
```

If `ftx -c` reports `device not found`, do not start campaign scripts yet. Repeat power-cycle + reset until probe succeeds.

Campaign scripts support PSU-assisted preflight directly:

```bash
bash Tests/test_campaign.sh --emulator USBGamers --psu-ip 192.168.1.106 --strict
```

Supported scripts:
- `Tests/test_campaign.sh`
- `Tests/test_background_campaign.sh`
- `Tests/test_bulletml_campaign.sh`
- `Tests/test_factory_campaign.sh`

## Hardware Full Test Policy

Use `tools/test_hardware_full.sh` for real Saturn HW_DEBUG validation with PSU power-cycle, upload, and runtime monitoring.

Default campaign policy is stability-first:
- `REQUIRE_HEARTBEAT=0` (default): missing or stale heartbeat is warn-only
- init and alloc checks still determine pass/fail verdict

Strict diagnostic policy is still available:

```bash
REQUIRE_HEARTBEAT=1 TIMEOUT=300 GAMEPLAY_MONITOR_SECONDS=10 ./tools/test_hardware_full.sh saturnpsu.local
```

Recommended stability run:

```bash
TIMEOUT=300 GAMEPLAY_MONITOR_SECONDS=10 ./tools/test_hardware_full.sh saturnpsu.local
```

Usage:

```bash
./tools/run_mednafen.sh
./tools/run_kronos.sh
./tools/run_on_saturn.bat
```

## BulletML Converter

The `bulletml_converter.py` script provides bidirectional conversion between BulletML XML and binary formats.

The BLB string table stores 7-bit ASCII text in 8-bit bytes. XML input and output remain UTF-8.

### Features

- **XML → Binary**: Convert `.xml` BulletML files to compact `.blb` binary format
- **Binary → XML**: Decompile `.blb` files back to human-readable XML
- **Batch Processing**: Convert entire directories of files at once
- **Automatic Detection**: Conversion direction is automatically detected from file extensions
- **Format Validation**: Validates both input and output formats

### Binary Format Benefits

- **40-80% smaller** than equivalent XML files
- **Faster loading** - no XML parsing overhead
- **Platform-independent** - explicit endianness and fixed-size types
- **Complete** - supports all BulletML features

### Usage

#### Single File Conversion

```bash
# XML to Binary
python3 tools/bulletml_converter.py input.xml output.blb

# Binary to XML
python3 tools/bulletml_converter.py input.blb output.xml
```

#### Batch Directory Conversion

```bash
# Convert all XML files in a directory to binary
python3 tools/bulletml_converter.py noiz2sa_share/boss/ binary_output/

# Convert all binary files in a directory back to XML
python3 tools/bulletml_converter.py binary_output/ xml_output/
```

#### Direct Execution

The script is executable and can be run directly:

```bash
./tools/bulletml_converter.py input.xml output.blb
```

### Examples

```bash
# Convert a single boss pattern
./tools/bulletml_converter.py noiz2sa_share/boss/88way.xml patterns/88way.blb

# Roundtrip test (XML → Binary → XML)
./tools/bulletml_converter.py noiz2sa_share/boss/88way.xml /tmp/88way.blb
./tools/bulletml_converter.py /tmp/88way.blb /tmp/88way_restored.xml

# Batch convert all boss patterns
./tools/bulletml_converter.py noiz2sa_share/boss/ binary_patterns/

# Batch convert middle stage patterns
./tools/bulletml_converter.py noiz2sa_share/middle/ binary_patterns/
```

### File Format

See [`../doc/BINARY_FORMAT.md`](../doc/BINARY_FORMAT.md) for complete binary format specification.

### Requirements

- Python 3.6 or higher
- Standard library only (no external dependencies)

### Error Handling

The converter validates input files and provides clear error messages:

```bash
$ python3 tools/bulletml_converter.py invalid.txt output.blb
error: Cannot determine conversion mode from extensions: .txt -> .blb
Supported conversions: .xml -> .blb or .blb -> .xml
```

### Performance

Typical conversion times on Saturn development machine:

- Single file: < 10ms
- Directory (30 files): < 200ms
- Large pattern (10KB XML): < 5ms

### Integration

To use binary BulletML files in your Saturn port:

1. Convert XML patterns to `.blb` format
2. Place binary files in `cd/data/` directory
3. Use `BulletMLParserBLB` to load `.blb` files
4. Load patterns from binary files

See `../doc/BINARY_PARSER_README.md` for C++ integration details.

## Performance Drop Report

The `perf_drop_report.py` script parses runtime timing lines (`[PERF_US]` and `[BLIT_US]`) and highlights where FPS drops happen.

### What It Reports

- FPS summary: average, p50, p90, p99
- Frame time summary (`total_us`): average, p50, p90, p99
- Drop detection using thresholds (`fps < X` or `total_us > Y`)
- Worst spikes with context windows around each drop
- BLIT hotspots (layer/line/panel DMA costs)

### Usage

```bash
# Default input path: Tests/uts.log
python3 tools/perf_drop_report.py

# Explicit log path
python3 tools/perf_drop_report.py Tests/uts.log

# Custom drop thresholds and context window
python3 tools/perf_drop_report.py Tests/uts.log --fps-threshold 20 --total-threshold 50000 --window 30 --top 10
```

### Notes

- The script expects runtime logs that include `[PERF_US]` and/or `[BLIT_US]` lines.
- If no `[PERF_US]` entries are found, run a profiling build/run where these debug logs are enabled.
