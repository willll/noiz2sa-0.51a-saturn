# noiz2sa-saturn — Agent Instructions

Sega Saturn port of Kenta Cho's bullet-hell shooter, cross-compiled for SH-2 using CMake + SaturnRingLib.

## Build

```bash
cmake -B build          # Configure (requires /opt/saturn toolchain)
cmake --build build     # Compile → build/noiz2sa.elf + CD image
```

Full options documented in [BUILD_GUIDE.md](BUILD_GUIDE.md). Key CMake flags:

| Flag | Default | Notes |
|------|---------|-------|
| `SRL_MODE` | `NTSC` | `PAL` for PAL consoles |
| `NOIZ2SA_SCREEN_DIVISOR` | `1` | `2`/`4` for lower resolution |
| `NOIZ2SA_ENABLE_SMOKE` | OFF | Smoke/afterimage effects |
| `DEBUG` | OFF | Debug logging |

## Testing

```bash
ctest                          # All tests (from build/)
ctest -V                       # Verbose
```

- **Collision/vector tests** – run under Mednafen Saturn emulator (SH-2 emulated). Requires `mednafen` on PATH.
- **BulletML parity tests** – compiled and run natively (no emulator needed).

See [Tests/BULLETML_PARITY_TEST_README.md](Tests/BULLETML_PARITY_TEST_README.md) for test details.

## Architecture

```
noiz2sa.cpp      Main loop, initialization
screen.cpp       SDL rendering, screen management
canvas.cpp       SRL::Bitmap::IBitmap abstraction
barragemanager   Enemy wave orchestration
foe / foecommand Enemy entities + BulletML execution
  bulletml_binary/   Binary .blb parser
ship / shot      Player entity + bullets
soundmanager     CDDA BGM + PCM SFX (ponesound/)
```

Detailed drawing pipeline: [doc/DRAWING_SYSTEM.md](doc/DRAWING_SYSTEM.md).

## Conventions

- **C++23** required (`-std=c++23`; `.cc` files are legacy, being migrated to `.cpp`)
- **Naming**: Classes `PascalCase`, structs `snake_case`, functions `camelCase`, globals `g_` prefix or bare, macros `SCREAMING_SNAKE_CASE`
- **Math**: Use `Fxp` (BulletML fixed-point) for bullet/position calculations — no floating-point on Saturn
- **SRL namespaces**: Import via `using namespace SRL::Input/Logger/Types` at file scope
- **Global state**: Extensive use of `extern` globals declared in `noiz2sa.h` — check there before adding new state
- **Memory**: Manual `new`/`delete`; optional TLSF allocator (`SRL_MALLOC_METHOD=TLSF`)

## BulletML Binary Format (.blb)

Enemy patterns live in `noiz2sa_share/{boss,middle,zako}/` as `.blb` files (binary BulletML).

Convert XML ↔ binary:
```bash
python3 tools/bulletml_converter.py input.xml output.blb
python3 tools/bulletml_converter.py noiz2sa_share/boss/ out/   # batch
```

Format spec: [doc/BINARY_FORMAT.md](doc/BINARY_FORMAT.md).

## Emulator Testing

```bash
bash tools/run_mednafen.sh    # Launch Mednafen with BuildDrop/noiz2sa.cue
bash tools/run_kronos.sh      # Launch Kronos emulator
```

Build outputs a `.cue` file automatically if `xorrisofs` and `iso2raw` are available.

## Performance Analysis

Tests emit `[PERF_US]` / `[BLIT_US]` markers. Analyze with:
```bash
python3 tools/perf_drop_report.py Tests/uts.log --fps-threshold 20 --top 10
```

## Key References

- [README.md](README.md) — project overview, upstream history
- [doc/DRAWING_SYSTEM.md](doc/DRAWING_SYSTEM.md) — rendering pipeline detail
- [doc/BINARY_FORMAT.md](doc/BINARY_FORMAT.md) — .blb format spec
- [BUILD_GUIDE.md](BUILD_GUIDE.md) — all CMake options and prerequisites
- [tools/README.md](tools/README.md) — converter, performance scripts, emulator wrappers
- [CD_GENERATION_GUIDE.md](CD_GENERATION_GUIDE.md) — producing a runnable CD image
