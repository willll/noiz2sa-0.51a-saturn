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
