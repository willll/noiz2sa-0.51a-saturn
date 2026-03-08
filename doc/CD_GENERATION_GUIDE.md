# CD Image Generation Guide

This guide explains how Saturn CD images are automatically generated from the noiz2sa project using the CMake build system.

## Overview

The CMakeLists.txt is configured to **automatically generate CD images during the default build** when the required tools are available:
- **ISO** generation from game assets and IP.BIN (requires `xorrisofs`)
- **BIN/CUE** conversion for emulator compatibility (requires `iso2raw`)
- **Audio track** integration into the CD image (requires `sox`)
- **CUE sheet** generation with proper sector alignment

## Quick Start

Simply build the project normally - CD image generation happens automatically:

```bash
cd /saturn/noiz2sa-0.51a-saturn
cmake -B build
cmake --build build    # This builds ELF, ISO, and audio automatically!
```

**Output files created:**
- `build/noiz2sa.elf` - Compiled game executable
- `build/cd.iso` - ISO 9660 image (if xorrisofs available)
- `build/cd.bin` & `build/cd.cue` - BIN/CUE format (if iso2raw available)
- `build/cd.bin` - Updated with audio tracks (if sox available)

## Build Targets

After the default build completes, you can manually regenerate individual components without rebuilding the executable using these targets:

### `cd-iso` - Regenerate ISO Image
Re-generates ISO without rebuilding the executable.

```bash
cmake --build build --target cd-iso
```

**Requirements:**
- `xorrisofs` (libburnia package)
  - Linux: `apt-get install xorriso`
  - macOS: `brew install xorriso`

**Output:** `build/cd.iso`

### `cd-bin-cue` - Manually Convert ISO to BIN/CUE Format
Re-generates BIN/CUE format without rebuilding.

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

### `cd-audio` - Manually Add Audio Tracks
Re-processes and adds audio tracks without full rebuild.

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

## Build Process

### Automatic CD Generation (Default)

Simply build the project - CD images are generated automatically if tools are available:

```bash
cd /saturn/noiz2sa-0.51a-saturn
cmake -B build
cmake --build build
```

This automatically creates:
- `build/noiz2sa.elf` - Compiled game executable
- `build/cd/data/0.bin` - Game binary (extracted from ELF)
- `build/cd/data/ABS.TXT`, `BIB.TXT`, `CPY.TXT` - Required metadata files
- `build/cd.iso` - ISO image (if xorrisofs available)
- `build/cd.bin` & `build/cd.cue` - BIN/CUE format (if iso2raw available later)
- Audio tracks in `build/cd.bin` (if sox available)

### What Happens During Build

1. **Compilation (noiz2sa.elf)**
   - C++ source compiled to ELF executable
   - Standard Saturn toolchain configuration

2. **Asset Setup (POST_BUILD, always)**
   - Create `build/cd/` directory structure
   - Extract game binary: `0.bin` from ELF via objcopy
   - Create metadata files: `ABS.TXT`, `BIB.TXT`, `CPY.TXT`

3. **ISO Generation (POST_BUILD, if xorrisofs available)**
   - Runs `generate_iso.sh` automatically
   - Creates `build/cd.iso` with all assets

4. **BIN/CUE Conversion (POST_BUILD, if iso2raw available)**
   - Runs `generate_bin_cue.sh` automatically
   - Creates `build/cd.bin` and `build/cd.cue`
   - Converts ISO to emulator-compatible format

5. **Audio Integration (POST_BUILD, if sox available)**
   - Runs `add_audio_tracks.sh` automatically
   - Appends audio tracks to `build/cd.bin`
   - Updates `build/cd.cue` with track indices

### Manual Regeneration

If you modify audio files or assets but don't want to rebuild the executable:

```bash
# Re-generate ISO without rebuild
cmake --build build --target cd-iso

# Re-generate BIN/CUE without rebuild
cmake --build build --target cd-bin-cue

# Re-add audio tracks without rebuild
cmake --build build --target cd-audio
```

### Step-by-Step Build (Legacy)

If you want to see each step, you can run the generated scripts manually:

```bash
cd build
bash generate_iso.sh         # Creates cd.iso
bash generate_bin_cue.sh     # Creates cd.bin and cd.cue
bash add_audio_tracks.sh     # Appends audio and updates cue
```

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
