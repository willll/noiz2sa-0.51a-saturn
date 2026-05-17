# noiz2sa-0.51a-saturn

Sega Saturn-focused port and modernization of Kenta Cho's abstract shoot-em-up **Noiz2sa**.

## Project Description

This repository contains the Saturn build of Noiz2sa with platform integrations for:

- SDL and SDL_mixer Saturn integration
- SaturnRingLib integration
- BulletML-based barrage pattern system
- CMake-based cross-build flow for Saturn toolchains

Recent maintenance in this fork includes:

- migration of the top-level game sources in `src/` from C to C++
- a documented BulletML binary format (`.blb`)
- XML-to-binary conversion tooling in `tools/`
- Complete SaturnRingLib CMake integration with shared.mk feature parity

The Saturn title screen uses sprite-based title glyphs loaded from the original title image assets (`TITLE*.TGA`), following the layout conventions used by the Windows 0.52 release and the GP32 codebase.

### Historical Context

Noiz2sa is one of Kenta Cho's early ABA Games freeware shooters, first released for Windows in 2002 and listed on the official ABA Games page as version 0.52. Contemporary descriptions characterize it as an abstract top-down shooter focused on dense projectile patterns, precision movement with a slowdown key, and score optimization through chained green-star collection rather than weapon-upgrade progression. Community-maintained game catalogs also record later ports to additional platforms.

Sources:

- [Official Noiz2sa page (ABA Games)](https://www.asahi-net.or.jp/~cs8k-cyu/windows/noiz2sa_e.html)
- [ABA Games catalog](https://www.asahi-net.or.jp/~cs8k-cyu/)
- [MobyGames: Noiz2sa](https://www.mobygames.com/game/noiz2sa)

### Sega Saturn Controls

- D-Pad: Move ship (in game), navigate stage select (title)
- A or B: Fire (in game), confirm/start stage (title)
- L or R: Reserved (slowdown currently not implemented)
- START: Pause / resume during gameplay

Notes:

- Pause is toggled with START only while in-game.
- The legacy PC controls listed later in this README are kept for historical reference.

## Building

The build system uses `/opt/saturn/CMake/sega_saturn.cmake` for toolchain configuration.

### Quick Start

Prerequisites on PATH for the default full CD image flow:

- `IPMaker` (from Saturn toolchain)
- `xorrisofs` (package: `xorriso`)
- `iso2raw` (from SaturnRingLib tool setup)

```bash
# Configure (toolchain auto-loaded)
cmake -B build

# Build (automatically creates .bin file and asset directories)
cmake --build build
```

**Build Outputs:**
- `BuildDrop/noiz2sa.elf` - Main executable
- `BuildDrop/noiz2sa.bin` - Saturn binary format (CD image)
- `cd/data/IP.BIN` - CD metadata and system information
- `BuildDrop/noiz2sa.cue` - CUE sheet for CD emulators

### Create ISO Image

ISO images are generated automatically during the build if `xorrisofs` is available:

```bash
# Ensure you have xorrisofs installed
apt-get install xorriso  # (Linux)
brew install xorriso     # (macOS)

# Build normally - ISO generation happens automatically
cmake -B build
cmake --build build

# Outputs: BuildDrop/noiz2sa.iso, BuildDrop/noiz2sa.cue, etc.
```

See [doc/CD_GENERATION_GUIDE.md](doc/CD_GENERATION_GUIDE.md) for complete details.

### Documentation

- [doc/BUILD_GUIDE.md](doc/BUILD_GUIDE.md) - Complete build instructions and configuration options
- [doc/HW_DEBUG_GUIDE.md](doc/HW_DEBUG_GUIDE.md) - Hardware debug mode for real Saturn testing with development cartridges
- [doc/BUILD_SYSTEM_SUMMARY.md](doc/BUILD_SYSTEM_SUMMARY.md) - SaturnRingLib and build system integration summary
- [doc/BINARY_PARSER_README.md](doc/BINARY_PARSER_README.md) - BulletML binary parser integration guide
- [doc/BINARY_FORMAT.md](doc/BINARY_FORMAT.md) - BulletML binary format specification

## Testing

This project includes both host-native and Saturn SH2/emulator-backed tests.

### Run All Registered CTest Suites

```bash
cmake -B build
cmake --build build
cd build
ctest -V
```

### Run Specific CTest Suites

```bash
# SH2 collision campaign (emulator-backed)
ctest -R sh2_collision_campaign -V

# SH2 background animation campaign (emulator-backed)
ctest -R sh2_background_animation_campaign -V

# SH2 factory campaign (emulator-backed)
ctest -R sh2_factory_campaign -V

# Native BulletML parity test
ctest -R bulletml_parity_test -V

# Native BulletML latch recovery test
ctest -R bulletml_latch_recovery_test -V

# Native hi-score persistence test
ctest -R hiscore_persistence_test -V

# Recursive XML parser test (python)
ctest -R bulletml_all_xml_recursive_test -V
```

### Run Campaign Scripts Directly

```bash
# Collision campaign
bash Tests/test_campaign.sh --emulator mednafen --strict

# Background campaign
bash Tests/test_background_campaign.sh --emulator mednafen --strict

# BulletML SH2 campaign
bash Tests/test_bulletml_campaign.sh --emulator mednafen --strict

# Factory SH2 campaign
bash Tests/test_factory_campaign.sh --emulator mednafen --strict
```

Supported emulator argument values for campaign scripts:

- `mednafen` (default)
- `kronos`
- `USBGamers`

Notes:

- Emulator-backed campaign scripts reconfigure CMake with `SRL_LOG_OUTPUT=EMULATOR` before building so runtime traces and UT completion markers are visible in the emulator log.
- `ctest -V` does not currently register a dedicated SH2 BulletML campaign entry. Use `Tests/test_bulletml_campaign.sh` directly.
- The SH2 BulletML campaign is currently built with `BULLETML_SKIP_CD_TESTS` (CD-file-dependent manifest parity test is skipped in campaign runs).
- For real hardware (`USBGamers`), the most reliable sequence is power-cycle, then `usbreset "FT245R USB FIFO"`, then a short settle delay before campaign upload.

Additional test implementation details and historical status are documented in:

- [Tests/BULLETML_PARITY_TEST_README.md](Tests/BULLETML_PARITY_TEST_README.md)

## Hardware Testing

For testing on real Sega Saturn hardware with development cartridges (USBGamers) and optional remote power control:

```bash
# Build in HW_DEBUG mode (no CD assets, embedded patterns)
cmake -B build_hw_debug -DHW_DEBUG=ON
cmake --build build_hw_debug

# Upload to Saturn hardware
./tools/run_on_saturn.bat
```

See [doc/HW_DEBUG_GUIDE.md](doc/HW_DEBUG_GUIDE.md) for complete instructions, including optional ESP-SaturnPSU_Control device integration for automated hardware control via REST API.

## Development Environment

Recommended external tooling used in this project workflow:

- [Saturn-SDK-GCC-SH2](https://github.com/willll/Saturn-SDK-GCC-SH2): GCC SH-2 toolchain for Sega Saturn development.
- [saturn-docker](https://github.com/willll/saturn-docker): Containerized build environment for reproducible Saturn builds.
- [ftx](https://github.com/willll/ftx): Development cartridge interface/tooling for upload and debug workflows.
- [ESP-SaturnPSU_Control](https://github.com/willll/ESP-SaturnPSU_Control): Hardware power-control integration used for real-Saturn automated testing.

- [Jump to Upstream / Legacy Readme](#sym:Upstream)

<a id="sym:Upstream"></a>
## Upstream / Legacy Readme

Upstream (original author site): http://www.asahi-net.or.jp/~cs8k-cyu/

The original README content from the classic PC release is preserved below for historical reference.

Noiz2sa  readme_e.txt
for Windows98/2000/XP
ver. 0.60
(C) Kenta Cho

Abstract shootem up game, 'Noiz2sa'.


- How to install.

Unpack noiz2sa0_51.zip, and execute 'noiz2sa.exe'.


- How to play.

Select the stage by a keyboard or a joystick.

 - Movement  Arrow key / Joystick
 - Fire      [Z]       / Trigger 1, Trigger 4
 - Slowdown  [X]       / Trigger 2, Trigger 3
 - Pause     [P]

Press a fire key to start the game.

Control your ship and avoid the barrage.
A ship is not destroyed even if it contacts an enemy main body.
A ship becomes slow while holding the slowdown key.

A green star is the bonus item.
A score of the item(displayed at the left-up corner) increases 
if you get items continuously.

When all ships are destroyed, the game is over.
The ship extends 200,000 and every 500,000 points.

These command line options are available:
 -nosound       Stop the sound.
 -window        Launch the game in the window, not use the full-screen.
 -reverse       Reverse the fire key and the slowdown key.
 -brightness n  Set the brightness of the sceen(n=0-256).
 -accframe      Use the alternative framerate management algorithm.
                (If you have a problem with framerate, try this option.)

- Add your original barrage patterns.

You can add your own barrage patterns to Noiz2sa.
In the 'noiz2sa' directory, there are 3 directories named
'zako', 'middle' and 'boss'.
In these directories, the barrage pattern files are placed.

The barrage pattern files are written by BulletML.
About BulletML, see the page:

BulletML
http://www.asahi-net.or.jp/~cs8k-cyu/bulletml/index_e.html

A 'zako' directory is for the small enemies.
A 'middle' directory is for the middle class enemies.
A 'boss' directory is for the boss type enemies.

You should adjust the difficulty of the barrage
by using a $rank variable properly.
A $rank variable is used to control the difficulty
of each scene in Noiz2sa.


- Comments

If you have any comments, please mail to cs8k-cyu@asahi-net.or.jp.


- Acknowledgement

libBulletML is used to parse BulletML files.
 libBulletML
 http://user.ecc.u-tokyo.ac.jp/~s31552/wp/libbulletml/
 
Simple DirectMedia Layer is used for the display handling. 
 Simple DirectMedia Layer
 http://www.libsdl.org/

SDL_mixer and Ogg Vorbis CODEC to play BGM/SE. 
 SDL_mixer 1.2
 http://www.libsdl.org/projects/SDL_mixer/
 Vorbis.com
 http://www.vorbis.com/


- History

2003  8/10  ver. 0.51
            Update libBulletML.
2003  2/12  ver. 0.5
            Add the accframe option.
            Add new barrages.
2003  1/ 3  ver. 0.42
            Adjust barrages.
2003  1/ 3  ver. 0.41
            Adjust barrages.
2002 12/31  ver. 0.4
            Add an endless insane mode.
            Add new barrages.
2002 11/23  ver. 0.32
            Adjust an invincible time.
2002 11/ 9  ver. 0.31
            Adjust the limits of movement of the ship.
            Add the brightness option.
2002 11/ 3  ver. 0.3


-- License

License
-------

Copyright 2002 Kenta Cho. All rights reserved. 

Redistribution and use in source and binary forms, 
with or without modification, are permitted provided that 
the following conditions are met: 

 1. Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer. 

 2. Redistributions in binary form must reproduce the above copyright notice, 
    this list of conditions and the following disclaimer in the documentation 
    and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
