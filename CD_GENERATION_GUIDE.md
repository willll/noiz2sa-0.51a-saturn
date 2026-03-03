# CD Image Generation Guide

This guide explains how to generate Sega Saturn CD images from the noiz2sa project using the CMake build system.

## Overview

The CMakeLists.txt has been updated with automatic support for generating complete Saturn CD images, including:
- **ISO** generation from game assets and IP.BIN
- **BIN/CUE** conversion for emulator compatibility
- **Audio track** integration into the CD image
- **CUE sheet** generation with proper sector alignment

## Build Targets

The following CMake targets are available after running `cmake -B build`:

### `cd-iso` - Generate ISO Image
Creates an ISO 9660 image suitable for burning or emulation.

```bash
cmake --build build --target cd-iso
```

**Requirements:**
- `xorrisofs` (libburnia package)
  - Linux: `apt-get install xorriso`
  - macOS: `brew install xorriso`

**Output:** `build/cd.iso`

### `cd-bin-cue` - Convert ISO to BIN/CUE Format
Converts the ISO image into BIN (binary data) and CUE (track/index sheet) format compatible with most emulators.

```bash
cmake --build build --target cd-bin-cue
```

**Requirements:**
- `iso2raw` tool
  - Linux: Build from source https://github.com/gianlucarenzi/iso2raw
  - Windows: Precompiled binary available
  - macOS: Build from source

**Output:** 
- `build/cd.bin` (CD image data)
- `build/cd.cue` (Cue sheet with track information)

### `cd-audio` - Add Audio Tracks
Discovers and converts audio files, then appends them to the CD image with proper CUE sheet entries.

```bash
cmake --build build --target cd-audio
```

**Requirements:**
- `sox` (Sound eXchange)
  - Linux: `apt-get install sox libsox-fmt-all`
  - macOS: `brew install sox`

**Requirements for audio files:**
- Audio files in `noiz2sa_share/music/` directory (optional)
- Or a `tracklist` file listing audio files to include
- Supported formats: MP3, WAV, OGG, FLAC, AAC, M4A, WMA

### `cd-all` - Complete CD Generation
Builds the complete CD image pipeline: ISO → BIN/CUE → Audio

```bash
cmake --build build --target cd-all
```

This target runs all three previous targets in sequence.

## Build Process

### Step 1: Build the ELF Executable
```bash
cd /saturn/noiz2sa-0.51a-saturn
cmake -B build
cmake --build build
```

This creates:
- `build/noiz2sa.elf` - Compiled game executable
- `build/cd/data/0.bin` - Game binary (extracted from ELF)
- `build/cd/data/ABS.TXT`, `BIB.TXT`, `CPY.TXT` - Required metadata files

### Step 2: Generate ISO (if xorrisofs is available)
```bash
cmake --build build --target cd-iso
```

Creates `build/cd.iso` with:
- IP.BIN (system boot sector)
- Game binary and assets
- Proper ISO 9660 filesystem

### Step 3: Convert to BIN/CUE (if iso2raw is available)
```bash
cmake --build build --target cd-bin-cue
```

Creates:
- `build/cd.bin` - Complete CD image data
- `build/cd.cue` - Cue sheet with track 1 (data)

### Step 4: Add Audio Tracks (if sox is available)
```bash
cmake --build build --target cd-audio
```

Processes audio files:
- Auto-discovers music in `noiz2sa_share/music/` or reads `tracklist` file
- Converts to 44.1kHz 16-bit PCM WAV
- Sector-aligns to 2352 bytes (CD sector size)
- Appends tracks to BIN file
- Updates CUE sheet with track indices in MM:SS:FF format

## Audio File Setup

### Option 1: Auto-Discovery (Recommended)
Create a `noiz2sa_share/music/` directory and place audio files:
```
noiz2sa_share/music/
    bgm_stage1.mp3
    bgm_stage2.wav
    bgm_boss.ogg
```

The build system will automatically discover and convert them in alphabetical order.

### Option 2: Tracklist File
Create `noiz2sa_share/music/tracklist` file listing audio sources:
```
/saturn/noiz2sa-0.51a-saturn/noiz2sa_share/music/bgm_stage1.mp3
/saturn/noiz2sa-0.51a-saturn/noiz2sa_share/music/bgm_stage2.wav
```

## Generated Scripts

The CMake configuration generates three bash scripts in the build directory:

1. **generate_iso.sh** - Calls xorrisofs to create ISO
2. **generate_bin_cue.sh** - Calls iso2raw to convert ISO to BIN/CUE
3. **add_audio_tracks.sh** - Discovers, converts, and appends audio

These scripts can be run manually if needed:
```bash
cd build
bash generate_iso.sh
bash generate_bin_cue.sh
bash add_audio_tracks.sh
```

## IP.BIN Location

The build system searches for IP.BIN in this order:
1. `SRL_IPBIN` CMake variable (custom path)
2. `cd/data/IP.BIN` (project root)
3. `SaturnRingLib/resources/IP.BIN`
4. SGL default location

If not found, the build will warn but not fail (ISO generation will fail at runtime).

## Emulator Usage

### Mednafen
```bash
mednafen build/cd.cue
```

### Yabause
1. Load `build/cd.cue` as CD image
2. Boot the emulator

### SSF
Use `build/cd.bin` + `build/cd.cue` (both files must be in same directory)

## Troubleshooting

### "xorrisofs not found"
Install with: `apt-get install xorriso` (Linux) or `brew install xorriso` (macOS)

### "iso2raw not found"
- Linux/macOS: Build from https://github.com/gianlucarenzi/iso2raw
- Windows: Download precompiled binary
- Set PATH or specify full path in CMake

### "sox not found"
Install with: `apt-get install sox libsox-fmt-all` (Linux) or `brew install sox` (macOS)

### Audio files not converting
- Check that `noiz2sa_share/music/` directory exists
- Verify audio file permissions and formats
- Run `sox` manually to test: `sox input.mp3 -r 44100 -b 16 -e signed output.wav`
- Check build log for error messages

### Sector alignment errors
- Ensure audio files are properly converted to PCM WAV format
- sox should handle this automatically, but verify with: `file output.wav`
- Error typically means audio wasn't padded to 2352-byte multiple

## CMakeLists.txt Configuration

Key variables in CMakeLists.txt:

```cmake
set(BUILD_ISO "${CMAKE_BINARY_DIR}/cd.iso")       # ISO output path
set(BUILD_BIN "${CMAKE_BINARY_DIR}/cd.bin")       # BIN output path
set(BUILD_CUE "${CMAKE_BINARY_DIR}/cd.cue")       # CUE output path
set(MUSIC_DIR "${CMAKE_SOURCE_DIR}/noiz2sa_share/music")  # Audio directory
set(ASSETS_DIR "${CMAKE_BINARY_DIR}/cd/data")     # Game assets directory
```

Override MUSIC_DIR if audio is in a different location:
```bash
cmake -B build -DMUSIC_DIR=/path/to/audio
```

## Additional Resources

- [Sega Saturn CD Specification](https://en.wikipedia.org/wiki/Sega_Saturn#Technical_specifications)
- [xorrisofs Documentation](https://www.gnu.org/software/xorriso/man_1_xorrisofs.html)
- [iso2raw GitHub](https://github.com/gianlucarenzi/iso2raw)
- [sox Documentation](http://sox.sourceforge.net/)
- [CUE Sheet Format](https://en.wikipedia.org/wiki/Cue_sheet_(computing))
