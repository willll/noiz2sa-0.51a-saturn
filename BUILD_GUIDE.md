# Noiz2sa Saturn - Build Guide

This guide covers building the Saturn port of Noiz2sa using the CMake build system.

## Prerequisites

1. **Saturn Cross-Compilation Toolchain**
   - sh-elf-gcc/g++ (recommend version 14.3.0+)
   - newlib 4.4.0+
   - binutils configured for sh-elf target
   - Toolchain configuration file at `/opt/saturn/CMake/sega_saturn.cmake`

2. **Environment Variables**
   The following environment variables should be set (typically configured by the toolchain):
   - `SATURN_ROOT` - Saturn SDK root directory
   - `SATURN_COMMON` - Common Saturn build files
   - `SATURN_CMAKE` - CMake module path
   - `CC`, `CXX`, `AR`, `RANLIB`, `AS`, `OBJCOPY` - Compiler toolchain binaries

3. **SaturnRingLib**
   - Located in `SaturnRingLib/` subdirectory
   - Pre-built libraries must exist:
     - `SaturnRingLib/saturnringlib/libs/LIBCPK.A`
     - `SaturnRingLib/saturnringlib/libs/SEGA_SYS.A`
     - `SaturnRingLib/saturnringlib/libs/LIBCD.A`
     - `SaturnRingLib/saturnringlib/libs/LIBSGL.A`

3. **SDL Libraries**
   - SDL_Saturn (submodule in `SDL/`)
   - SDL_mixer_Saturn (submodule in `SDL_mixer/`)

4. **BulletML Library**
   - Source in `src/bulletml/`
   - Built automatically by CMake

## Quick Start

### Basic Build

The build system automatically uses the Saturn toolchain file at `/opt/saturn/CMake/sega_saturn.cmake`.

```bash
# Configure for Saturn target (toolchain auto-loaded)
cmake -B build

# Or explicitly specify the toolchain (optional)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/opt/saturn/CMake/sega_saturn.cmake

# Build the project
cmake --build build

# Output: build/noiz2sa.elf
```

**Note**: The toolchain file is automatically included if not already specified. It configures:
- Cross-compiler settings (sh-elf-gcc/g++)
- Saturn-specific paths and environment
- Build type flags (Debug/Release/etc.)
- CMAKE_OBJCOPY for binary conversion

### Common Build Configurations

#### Debug Build
```bash
cmake -B build -DDEBUG=1 -DSRL_LOG_LEVEL=4
cmake --build build
```

#### PAL Mode (50Hz)
```bash
cmake -B build -DSRL_MODE=PAL
cmake --build build
```

#### High Performance (60fps, optimizations)
```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSRL_FRAMERATE=2 \
    -DSRL_CUSTOM_CCFLAGS="-O3 -ffast-math -funroll-loops"
cmake --build build
```

#### TLSF Memory Allocator
```bash
cmake -B build -DSRL_MALLOC_METHOD=TLSF
cmake --build build
```

#### Custom Texture/Polygon Limits
```bash
cmake -B build \
    -DSRL_MAX_TEXTURES=4096 \
    -DSGL_MAX_VERTICES=4000 \
    -DSGL_MAX_POLYGONS=2000
cmake --build build
```

## Configuration Options

### Display Settings

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `SRL_MODE` | NTSC/PAL | NTSC | Video standard |
| `SRL_FRAMERATE` | 0/1/2 | 0 | 0=auto, 1=30fps, 2=60fps |
| `SRL_HIGH_RES` | ON/OFF | OFF | Enable high resolution mode |

### Memory Settings

| Option | Default | Description |
|--------|---------|-------------|
| `SRL_MAX_TEXTURES` | 100 | Maximum texture count |
| `SGL_MAX_VERTICES` | 2500 | Maximum vertices per frame |
| `SGL_MAX_POLYGONS` | 1700 | Maximum polygons per frame |
| `SGL_MAX_EVENTS` | 64 | Maximum SGL events |
| `SGL_MAX_WORKS` | 256 | Maximum SGL work units |
| `SRL_MALLOC_METHOD` | (empty) | Set to "TLSF" for TLSF allocator |

### CD Subsystem

| Option | Default | Description |
|--------|---------|-------------|
| `SRL_MAX_CD_BACKGROUND_JOBS` | 1 | Concurrent CD operations |
| `SRL_MAX_CD_FILES` | 255 | Maximum open CD files |
| `SRL_MAX_CD_RETRIES` | 5 | CD read retry attempts |

### Debug Settings

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `DEBUG` | ON/OFF | OFF | Enable debug mode |
| `SRL_LOG_LEVEL` | 0-4 or empty | (empty) | Logging verbosity |
| `SRL_LOG_OUTPUT` | (varies) | (empty) | Log output method |
| `SRL_DEBUG_MAX_LOG_LENGTH` | number | 80 | Max log message length |

### Sound Settings

| Option | Default | Description |
|--------|---------|-------------|
| `SRL_USE_SGL_SOUND_DRIVER` | OFF | Use SGL sound instead of SCSP |
| `SRL_ENABLE_FREQ_ANALYSIS` | OFF | Enable frequency analysis |

### Custom Flags

| Option | Type | Description |
|--------|------|-------------|
| `SRL_CUSTOM_CCFLAGS` | string | Additional compiler flags |
| `SRL_CUSTOM_LDFLAGS` | string | Additional linker flags |

## Build Process Details

### 1. Configuration Phase

CMake configures the project with your chosen options:

```bash
cmake -B build [OPTIONS]
```

This phase:
- Validates SaturnRingLib installation
- Checks for required libraries
- Sets up compiler/linker flags
- Generates build files
- Displays configuration summary

### 2. Compilation Phase

CMake builds the project:

```bash
cmake --build build
```

This phase:
- Compiles all source files
- Links against BulletML, SDL, SDL_mixer, SaturnRingLib
- Generates `noiz2sa.elf`

**Automatic Post-Build Steps:**
After successful compilation, the following happens automatically:
- Creates `build/cd/data/` and `build/cd/music/` directories
- Creates required metadata files (`ABS.TXT`, `BIB.TXT`, `CPY.TXT`)
- Converts `noiz2sa.elf` → `build/cd/data/0.bin` (binary format)
- Copies SGL sound driver files (if `SRL_USE_SGL_SOUND_DRIVER=ON`)
- Copies frequency analysis DSP (if `SRL_ENABLE_FREQ_ANALYSIS=ON`)

**Build Outputs:**
- `build/noiz2sa.elf` - Main executable (ELF format)
- `build/cd/data/0.bin` - Binary format for Saturn CD
- `build/cd/data/` - Asset directory with all required files
- `build/cd/music/` - Music asset directory
- `build/noiz2sa.map` - Linker map file

### 3. Post-Build (Optional)

If `postbuild.cmake` exists in project root, it runs automatically:

```cmake
# Example postbuild.cmake
message(STATUS "Creating binary...")
execute_process(
    COMMAND sh-elf-objcopy -O binary noiz2sa.elf noiz2sa.bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

**ISO Image Creation:**

To create a Saturn ISO image, copy the example script:
```bash
cp postbuild.cmake.example postbuild.cmake
```

Then rebuild:
```bash
cmake --build build
```

This will create:
- `build/noiz2sa.iso` - Saturn CD-ROM image
- `build/noiz2sa.cue` - CUE sheet for burning/emulation

**Requirements for ISO creation:**
- `xorrisofs` or `mkisofs` installed on your system
- `IP.BIN` file in `SaturnRingLib/modules/sgl/`

**Testing the ISO:**
- Load in Mednafen: `mednafen build/noiz2sa.cue`
- Load in SSF: Open `build/noiz2sa.iso`
- Load in Yabause/Kronos: Open `build/noiz2sa.cue`
- Burn to CD-R: Use the `.cue` file with burning software

## Source Organization

### Game Sources (`src/`)

All game logic and rendering code:
- `noiz2sa.cpp` - Main game loop
- `ship.cpp` - Player ship
- `shot.cpp` - Projectile system
- `foe.cc` - Enemy entities
- `barragemanager.cc` - Bullet pattern manager
- `screen.cpp` - Display rendering
- `background.cpp` - Background effects
- `soundmanager.cpp` - Audio system
- And more...

### BulletML (`src/bulletml/`)

Bullet pattern library and binary format support:
- Original XML parser
- Binary format parser (`bulletml_binary/`)
- Python converter (`bulletml_binary/bulletml_converter.py`)

### SaturnRingLib Integration

Automatically included sources:
- `SaturnRingLib/modules/sgl/SRC/workarea.c` - SGL workspace
- `SaturnRingLib/modules/sgl/SRC/preloader.cxx` - C++ initialization
- `SaturnRingLib/modules/tlsf/tlsf.c` - Optional TLSF allocator

## Troubleshooting

### CMake Can't Find SaturnRingLib

**Error**: `SaturnRingLib not found at: ...`

**Solution**: Ensure SaturnRingLib is properly initialized:
```bash
git submodule update --init --recursive
```

### Missing Libraries

**Error**: `Could not find LIBSGL.A`

**Solution**: Build SaturnRingLib libraries first:
```bash
cd SaturnRingLib/saturnringlib
make
```

### Linker Errors (undefined references)

**Error**: `undefined reference to '_exit'`, `_sbrk`, etc.

**Solution**: These are Saturn-specific syscalls. Ensure your toolchain includes:
- Proper `crt0.s` startup file
- `Saturn.lnk` linker script
- Newlib syscall stubs

### Configuration Summary Not Showing

**Issue**: Build works but no "SaturnRingLib Configuration" output

**Solution**: This is normal - the summary only appears during CMake configuration:
```bash
cmake -B build  # Summary appears here
```

## Advanced Usage

### Changing Options Without Reconfiguring

```bash
# Use ccmake for interactive configuration
ccmake build

# Or cmake-gui
cmake-gui build
```

### Parallel Builds

```bash
# Use all CPU cores
cmake --build build -j$(nproc)
```

### Verbose Build Output

```bash
cmake --build build --verbose
```

### Clean Build

```bash
rm -rf build
cmake -B build
cmake --build build
```

### Build with Specific Generator

```bash
# Use Ninja instead of Make
cmake -B build -G Ninja
cmake --build build
```

## Related Documentation

- [SRL_CMAKE_MIGRATION.md](SRL_CMAKE_MIGRATION.md) - CMake migration details
- [src/bulletml_binary/README.md](src/bulletml_binary/README.md) - Binary BulletML format
- [SaturnRingLib/readme.md](SaturnRingLib/readme.md) - SaturnRingLib documentation
- [README.md](README.md) - Project overview

## Support

For build issues specific to:
- **SaturnRingLib**: See `SaturnRingLib/Documentation/`
- **BulletML**: See `src/bulletml/`
- **Saturn toolchain**: Consult your toolchain documentation
