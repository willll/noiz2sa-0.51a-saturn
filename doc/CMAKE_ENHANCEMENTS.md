# CMakeLists.txt Build System Enhancement Summary

## Overview of Changes

The CMakeLists.txt has been enhanced with comprehensive CD image generation capabilities, integrating build rules from shared.mk into the CMake build system.

## Key Additions to CMakeLists.txt

### 1. **Tool Detection & Configuration**

Added automatic detection for required external tools:
- **xorrisofs** - For ISO image creation
- **iso2raw** - For BIN/CUE conversion (platform-specific paths)
- **sox** - For audio file conversion

Each tool detection includes user-friendly error messages and installation instructions.

### 2. **Build Output Paths**

Configured output locations for generated CD images:
```cmake
set(BUILD_ISO "${CMAKE_BINARY_DIR}/cd.iso")      # ISO image
set(BUILD_BIN "${CMAKE_BINARY_DIR}/cd.bin")      # BIN data file
set(BUILD_CUE "${CMAKE_BINARY_DIR}/cd.cue")      # CUE sheet
```

### 3. **IP.BIN Locator**

Smart detection of Saturn system boot sector:
- Checks custom `SRL_IPBIN` CMake variable
- Searches standard locations in project
- Falls back to SaturnRingLib resources
- Provides clear warnings if not found

### 4. **Automatic Script Generation**

Three bash scripts are automatically generated during CMake configuration:

#### **generate_iso.sh**
- Uses xorrisofs to create ISO 9660 image
- Includes IP.BIN as boot sector
- Incorporates game binary (0.bin) and assets
- Sets proper ISO metadata for Saturn

#### **generate_bin_cue.sh**
- Converts ISO to BIN (binary data) + CUE (track sheet) format
- Uses iso2raw with platform-specific error handling
- Validates output file integrity
- Suitable for emulators (Mednafen, Yabause, SSF, etc.)

#### **add_audio_tracks.sh**
Complete audio handling pipeline:
- **Auto-discovery**: Finds .mp3, .wav, .ogg, .flac, .aac, .m4a, .wma files in music directory
- **Tracklist support**: Optionally reads explicit list from `tracklist` file
- **Conversion**: Uses sox to normalize to 44.1kHz, 16-bit PCM
- **Sector alignment**: Pads audio to 2352-byte CD sector boundaries
- **CUE generation**: Creates track entries with proper MM:SS:FF indexing
- **Validation**: Checks sector alignment and file sizes before appending
- **PREGAP handling**: Adds 2-second gap before audio tracks (ECMA-130 spec compliance)

### 5. **CMake Custom Targets**

Four new build targets enable step-by-step or complete CD generation:

```bash
cmake --build build --target cd-iso      # Generate ISO image
cmake --build build --target cd-bin-cue  # Convert ISO to BIN/CUE
cmake --build build --target cd-audio    # Add audio tracks
cmake --build build --target cd-all      # Complete pipeline
```

**Dependency Chain:**
```
cd-iso → cd-bin-cue → cd-audio
  ↓         ↓            ↓
 .iso      .bin        .cue
           .cue      (complete)
```

### 6. **POST_BUILD Integration**

Added POST_BUILD commands to noiz2sa.elf target:
```cmake
# Make scripts executable after generation
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND chmod +x generate_*.sh
)
```

### 7. **User Feedback**

Enhanced CMake output with color-coded status messages:
- ✓ Green checkmarks for available tools/targets
- ✗ Red X marks for missing dependencies
- Installation instructions for missing tools
- Complete usage guide in console output

## Technical Implementation Details

### ISO Generation (xorrisofs)
```bash
xorrisofs -r -J -C 0,32 \
    -sysid="SEGA SATURN" \
    -boot_image isolinux bin=IP.BIN \
    -o cd.iso cd/data/
```

### BIN/CUE Conversion (iso2raw)
```bash
iso2raw cd.iso cd.bin
# Generates cd.bin (all sectors) and cd.cue (track info)
```

### Audio Processing (sox + sector alignment)
```bash
sox input.mp3 -r 44100 -b 16 -e signed output.wav
# Pad to 2352-byte boundary (2352 = CD sector size)
dd if=/dev/zero bs=1 count=PADDING >> output.wav
```

### CUE Sheet Format
```
TRACK 01 BINARY
INDEX 01 00:00:00

TRACK 02 AUDIO
PREGAP 00:02:00
INDEX 01 MM:SS:FF
```

## Audio Track Processing Workflow

1. **Discovery Phase**
   - Checks for `noiz2sa_share/music/tracklist` file
   - Falls back to auto-discovery of .mp3/.wav/.ogg/.flac/.aac/.m4a/.wma files
   - Processes tracks in sorted order

2. **Conversion Phase**
   - Each file converted to 44.1kHz, 16-bit signed PCM
   - Output filename: `basename.raw` in music directory
   - Cached: Skips reconversion if .raw file already exists

3. **Alignment Phase**
   - Calculates padding needed to reach 2352-byte multiple
   - Appends zero bytes if needed
   - Verifies alignment before appending to BIN

4. **Track Sheet Generation**
   - Calculates sector positions for each track
   - Converts sector numbers to MM:SS:FF (Minutes:Seconds:Frames)
   - Validates frame count (0-74 range per ECMA-130)
   - Adds PREGAP for audio tracks (150 frames = 2 seconds gap)

5. **Validation**
   - Checks all files are sector-aligned
   - Verifies CUE sheet syntax
   - Reports final CD image size in bytes and sectors

## File Structure After Build

```
build/
├── cd/
│   └── data/
│       ├── 0.bin              # Game binary
│       ├── ABS.TXT
│       ├── BIB.TXT
│       └── CPY.TXT
├── cd.iso                      # ISO image (from cd-iso target)
├── cd.bin                      # BIN image (from cd-bin-cue target)
├── cd.cue                      # CUE sheet (from cd-audio target)
├── generate_iso.sh             # ISO generation script
├── generate_bin_cue.sh         # BIN/CUE conversion script
└── add_audio_tracks.sh         # Audio addition script
```

## CMakeLists.txt Additions Location

All new code was inserted after the sound driver configuration section (line ~332) and before the "SRL Configuration Summary" section. The additions include:

1. Lines 333-365: Tool detection and path configuration
2. Lines 366-428: Script generation for ISO creation
3. Lines 430-506: Script generation for BIN/CUE conversion
4. Lines 508-638: Script generation for audio handling
5. Lines 640-738: Custom target definitions and user feedback

## Compatibility Notes

- **Python**: Scripts are bash, not Python (compatible with Linux/macOS/WSL)
- **Cross-platform**: Tools auto-detect Windows/macOS/Linux for platform-specific binaries
- **CMake version**: Requires CMake 3.5+ (already required by project)
- **Backward compatible**: Existing build process unaffected; targets are opt-in
- **Fallback support**: Graceful degradation if tools missing (clear error messages)

## Usage Examples

### Simple: Build and generate complete CD image
```bash
cd /saturn/noiz2sa-0.51a-saturn
cmake -B build
cmake --build build          # Build executable
cmake --build build --target cd-all  # Generate complete CD
```

### Advanced: Step-by-step with manual scripts
```bash
cmake -B build && cmake --build build
cd build
bash generate_iso.sh         # ISOs are ~700MB
bash generate_bin_cue.sh     # Creates playable CD format
bash add_audio_tracks.sh     # Appends audio tracks
```

### Custom audio directory
```bash
cmake -B build -DMUSIC_DIR=/path/to/audio
cmake --build build --target cd-audio
```

## Verification

To verify the generated CD images:
1. **ISO validity**: `iso-info -R cd.iso`
2. **BIN/CUE integrity**: `cuetools` package
3. **Audio tracks**: `ffprobe cd.bin` (shows all tracks concatenated)
4. **Emulator test**: Load `cd.cue` in Mednafen/Yabause

## Related Files

- **NEW**: [CD_GENERATION_GUIDE.md](CD_GENERATION_GUIDE.md) - User guide for CD generation
- **MODIFIED**: CMakeLists.txt - Enhanced with CD generation rules
- **EXISTING**: shared.mk - Original Makefile rules (now integrated into CMake)

## Error Handling

Scripts include comprehensive error checking:
- File existence validation
- Sector alignment verification
- Frame count bounds checking (0-74)
- Size sanity checks for BIN/CUE files
- Audio file validity confirmation

All errors are reported with descriptive messages and exit codes for integration with build systems.
