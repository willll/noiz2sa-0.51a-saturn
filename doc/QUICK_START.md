# Quick Start: Automatic CD Generation

## Default Build (Everything Automatic!)

```bash
cd /saturn/noiz2sa-0.51a-saturn
cmake -B build
cmake --build build
```

**What this does:**
1. ✅ Compiles game to `build/noiz2sa.elf`
2. ✅ Extracts binary to `build/cd/data/0.bin`
3. ✅ **Automatically generates** `build/cd.iso` (if xorrisofs available)
4. ✅ **Automatically processes audio** and adds to CD image (if sox available)

## Output Files

After the build completes:
```
build/
├── noiz2sa.elf              # Compiled game
├── cd.iso                   # ISO image (always generated if xorrisofs available)
├── cd.bin                   # CD binary data (requires iso2raw - separate install)
├── cd.cue                   # CUE sheet with track info
└── cd/data/
    ├── 0.bin                # Game binary
    ├── ABS.TXT
    ├── BIB.TXT
    └── CPY.TXT
```

## Next Steps

### Option 1: Use ISO Immediately
The `build/cd.iso` file is ready to use with emulators:
```bash
mednafen build/cd.iso
```

### Option 2: Get BIN/CUE Format (Requires iso2raw)
```bash
# Install iso2raw (one-time setup)
git clone https://github.com/gianlucarenzi/iso2raw
cd iso2raw && make

# Then rebuild to generate BIN/CUE
cd /saturn/noiz2sa-0.51a-saturn
cmake --build build
```

After iso2raw is installed, `cmake --build build` will automatically generate:
- `build/cd.bin` - Complete CD image
- `build/cd.cue` - Track information

### Option 3: Add Audio Files
Place audio files in `noiz2sa_share/music/`:
```bash
mkdir -p noiz2sa_share/music
cp bgm_*.mp3 noiz2sa_share/music/
cmake --build build    # Builds and automatically adds audio
```

Supported formats: MP3, WAV, OGG, FLAC, AAC, M4A, WMA

## Manual Regeneration (Without Full Rebuild)

If you change audio files or assets:
```bash
# Re-generate ISO only
cmake --build build --target cd-iso

# Re-generate BIN/CUE only
cmake --build build --target cd-bin-cue

# Re-add audio tracks only
cmake --build build --target cd-audio
```

## Build Status Messages

During `cmake -B build`, you'll see:
```
-- ========== CD Image Generation ==========
-- ✓ ISO will be generated automatically
-- ✗ BIN/CUE requires iso2raw (build from: ...)
-- ⚠ Audio tracks can be added (run after ISO/BIN/CUE generation)
```

- ✓ = Automatic during default build
- ✗ = Not available (tool not installed)
- ⚠ = Available if dependencies are ready

## Troubleshooting

### "ISO will fail without IP.BIN"
This is just a warning about the system boot sector. It's provided by SGL library and the build handles it automatically.

### ISO Not Generated
- Verify xorrisofs is installed: `which xorrisofs`
- Install: `apt-get install xorriso` (Linux) or `brew install xorriso` (macOS)
- Reconfigure: `cmake -B build` (cache may need refresh)

### Audio Not Converting
- Install sox: `apt-get install sox libsox-fmt-all`
- Verify audio files exist: `ls noiz2sa_share/music/`
- Try manual regeneration: `cmake --build build --target cd-audio`

### Binary Too Large / Sector Alignment Errors
Check build log for detailed audio conversion messages. Usually requires sox to be properly installed.

## For Detailed Information

- **CD_GENERATION_GUIDE.md** - Complete reference
- **CMAKE_ENHANCEMENTS.md** - Technical details
- **BUILD_SYSTEM_SUMMARY.md** - Full summary of changes
