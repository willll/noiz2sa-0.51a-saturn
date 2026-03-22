# noiz2sa-0.51a-saturn

Sega Saturn-focused port and modernization of Kenta Cho's abstract shoot-em-up **Noiz2sa**.

## Project Description

This repository contains the Saturn build of Noiz2sa with platform integrations for:

- SDL and SDL_mixer Saturn submodules
- SaturnRingLib integration
- BulletML-based barrage pattern system
- CMake-based cross-build flow for Saturn toolchains

Recent maintenance in this fork includes:

- migration of the top-level game sources in `src/` from C to C++
- a documented BulletML binary format (`.blb`)
- XML-to-binary conversion tooling in `tools/`
- Complete SaturnRingLib CMake integration with shared.mk feature parity

## Building

The build system uses `/opt/saturn/CMake/sega_saturn.cmake` for toolchain configuration.

### Quick Start

```bash
# Configure (toolchain auto-loaded)
cmake -B build

# Build (automatically creates .bin file and asset directories)
cmake --build build
```

**Build Outputs:**
- `build/noiz2sa.elf` - Main executable
- `build/cd/data/0.bin` - Saturn binary format (auto-generated)
- `build/cd/data/` - Asset directory with required metadata files
- `build/cd/music/` - Music asset directory

### Create ISO Image

```bash
# Enable ISO creation
cp postbuild.cmake.example postbuild.cmake

# Rebuild
cmake --build build

# Outputs: build/noiz2sa.iso and build/noiz2sa.cue
```

### Documentation

- [BUILD_GUIDE.md](BUILD_GUIDE.md) - Complete build instructions and configuration options
- [BUILD_SYSTEM_SUMMARY.md](../BUILD_SYSTEM_SUMMARY.md) - SaturnRingLib and build system integration summary
- [BINARY_PARSER_README.md](BINARY_PARSER_README.md) - BulletML binary parser integration guide
- [BINARY_FORMAT.md](BINARY_FORMAT.md) - BulletML binary format specification

## Upstream / Legacy Readme

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
