cmake_minimum_required(VERSION 3.5)

# ============================================================================
# Saturn Toolchain Configuration
# ============================================================================
# Use the standard Saturn toolchain file
if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{CMAKE_MODULE_PATH})
        list(PREPEND CMAKE_MODULE_PATH $ENV{CMAKE_MODULE_PATH})
    endif()
    find_file(SATURN_TOOLCHAIN_FILE
        NAMES sega_saturn.cmake
        PATHS ${CMAKE_MODULE_PATH}
        NO_DEFAULT_PATH)
    if(NOT SATURN_TOOLCHAIN_FILE)
        set(SATURN_TOOLCHAIN_FILE "/opt/saturn/CMake/sega_saturn.cmake")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "${SATURN_TOOLCHAIN_FILE}")
    include("${CMAKE_TOOLCHAIN_FILE}")
endif()

set(CMAKE_VERBOSE_MAKEFILE OFF)

# Project-specific library flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

# SEGALIB and SEGASGL are set by sega_saturn.cmake, but available here
# SATURN_COMMON and SATURN_CMAKE are also set by the toolchain file

# Project definition
project( noiz2sa VERSION 1.0
                  DESCRIPTION "Sega Saturn noiz2sa"
                  LANGUAGES C CXX ASM)

# Require C++23 for project compatibility
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SaturnRingLib include path configuration
# Use environment variable if available, otherwise use relative path
if(DEFINED ENV{SRL_INSTALL_ROOT})
    set(SRL_INSTALL_ROOT $ENV{SRL_INSTALL_ROOT})
else()
    set(SRL_INSTALL_ROOT "${CMAKE_SOURCE_DIR}/SaturnRingLib")
endif()

set(SRL_ROOT "${SRL_INSTALL_ROOT}" CACHE PATH "Path to SaturnRingLib root" FORCE)
include("/saturn/noiz2sa-0.51a-saturn/cmake/SRL.cmake")

# Add subdirectories after SRL setup so Saturn include paths are available
add_subdirectory(src/bulletml_binary)

# ============================================================================
# SaturnRingLib Configuration Options (from shared.mk)
# ============================================================================

# CD subsystem configuration
set(SRL_MAX_CD_BACKGROUND_JOBS "1" CACHE STRING "Maximum CD background jobs" FORCE)
set(SRL_MAX_CD_FILES "255" CACHE STRING "Maximum CD files" FORCE)
set(SRL_MAX_CD_RETRIES "5" CACHE STRING "Maximum CD retries" FORCE)

# Display configuration
option(SRL_HIGH_RES "Enable high resolution mode" OFF)
set(SRL_FRAMERATE "0" CACHE STRING "Target framerate (0 = auto)")
set(SRL_MODE "NTSC" CACHE STRING "Video mode: NTSC or PAL")

# Memory configuration
set(SRL_MAX_TEXTURES "100" CACHE STRING "Maximum textures")

# Debug configuration
option(DEBUG "Enable debug mode" OFF)
set(SRL_LOG_LEVEL "" CACHE STRING "Log level (0-4, empty = disabled)")
set(SRL_LOG_OUTPUT "" CACHE STRING "Log output method")
set(SRL_DEBUG_MAX_PRINT_LENGTH "45" CACHE STRING "Maximum debug print length")
set(SRL_DEBUG_MAX_LOG_LENGTH "80" CACHE STRING "Maximum debug log length")

# Sound configuration
option(SRL_USE_SGL_SOUND_DRIVER "Use SGL sound driver" OFF)
option(SRL_ENABLE_FREQ_ANALYSIS "Enable frequency analysis" OFF)

# SGL Work Area Settings
set(SGL_MAX_VERTICES "2500" CACHE STRING "Maximum vertices")
set(SGL_MAX_POLYGONS "1700" CACHE STRING "Maximum polygons")
set(SGL_MAX_EVENTS "64" CACHE STRING "Maximum events")
set(SGL_MAX_WORKS "256" CACHE STRING "Maximum work units")

# Memory allocator
set(SRL_MALLOC_METHOD "" CACHE STRING "Memory allocator method (TLSF or empty)")

# Custom flags
set(SRL_CUSTOM_CCFLAGS "" CACHE STRING "Custom C/C++ compiler flags")
set(SRL_CUSTOM_LDFLAGS "" CACHE STRING "Custom linker flags")

# Custom IP.BIN
set(SRL_IPBIN "" CACHE STRING "Custom IP.BIN path")

# ============================================================================
# Apply SRL Configuration as Compile Definitions
# ============================================================================

# CD subsystem
add_compile_definitions(
    SRL_MAX_CD_BACKGROUND_JOBS=${SRL_MAX_CD_BACKGROUND_JOBS}
    SRL_MAX_CD_FILES=${SRL_MAX_CD_FILES}
    SRL_MAX_CD_RETRIES=${SRL_MAX_CD_RETRIES}
)

# Display
if(SRL_HIGH_RES)
    add_compile_definitions(SRL_HIGH_RES)
endif()

add_compile_definitions(SRL_FRAMERATE=${SRL_FRAMERATE})

# Video mode
if(SRL_MODE STREQUAL "PAL")
    add_compile_definitions(SRL_MODE_PAL)
else()
    add_compile_definitions(SRL_MODE_NTSC)
endif()

# Memory
add_compile_definitions(SRL_MAX_TEXTURES=${SRL_MAX_TEXTURES})

# Debug
if(DEBUG)
    add_compile_definitions(DEBUG)
endif()

if(NOT SRL_LOG_LEVEL STREQUAL "")
    add_compile_definitions(SRL_LOG_LEVEL=${SRL_LOG_LEVEL})
endif()

if(NOT SRL_LOG_OUTPUT STREQUAL "")
    add_compile_definitions(SRL_LOG_OUTPUT=${SRL_LOG_OUTPUT})
endif()

add_compile_definitions(
    SRL_DEBUG_MAX_PRINT_LENGTH=${SRL_DEBUG_MAX_PRINT_LENGTH}
    SRL_DEBUG_MAX_LOG_LENGTH=${SRL_DEBUG_MAX_LOG_LENGTH}
)

# Sound
if(SRL_USE_SGL_SOUND_DRIVER)
    add_compile_definitions(SRL_USE_SGL_SOUND_DRIVER=1)
    if(SRL_ENABLE_FREQ_ANALYSIS)
        add_compile_definitions(SRL_ENABLE_FREQ_ANALYSIS=1)
    endif()
endif()

# SGL Work Area
add_compile_definitions(
    SGL_MAX_VERTICES=${SGL_MAX_VERTICES}
    SGL_MAX_POLYGONS=${SGL_MAX_POLYGONS}
    SGL_MAX_EVENTS=${SGL_MAX_EVENTS}
    SGL_MAX_WORKS=${SGL_MAX_WORKS}
)

# TLSF Allocator
if(SRL_MALLOC_METHOD STREQUAL "TLSF")
    set(TLSF_SOURCE "${SRL_MODULES_DIR}/tlsf/tlsf.c")
    if(EXISTS "${TLSF_SOURCE}")
        list(APPEND SOURCES_NOIZ2SA "${TLSF_SOURCE}")
        message(STATUS "Using TLSF memory allocator")
    else()
        message(WARNING "TLSF requested but source not found: ${TLSF_SOURCE}")
    endif()
endif()

# ============================================================================

set(as_flags )
set(cxx_flags
    -O2 -g3 -m2 -c
    -fpermissive
    -Wno-conversion-null
    -Wno-strict-aliasing
    -fno-lto -Wshadow -Wall -Wswitch -Wpadded
    -fno-common -Wextra -Wno-narrowing -fno-web
    -fno-builtin -funit-at-a-time -Wwrite-strings
    -mno-fsrra
    -Wno-padded
    -Wno-unused-variable
    -Wno-unused-parameter
    -maccumulate-outgoing-args
    -fomit-frame-pointer
    -fno-rtti
    -fno-exceptions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -Wdeprecated-declarations   # FIXME!
    -fno-permissive  # FIXME!
    )

# Apply custom compiler flags from SRL configuration
if(NOT SRL_CUSTOM_CCFLAGS STREQUAL "")
    separate_arguments(SRL_CUSTOM_CCFLAGS_LIST UNIX_COMMAND "${SRL_CUSTOM_CCFLAGS}")
    list(APPEND cxx_flags ${SRL_CUSTOM_CCFLAGS_LIST})
endif()

set(SOURCES_NOIZ2SA
  src/noiz2sa.cpp
  src/degutil.cpp
  src/vector.cpp
  src/screen.cpp
  src/shot.cpp
  src/ship.cpp
  src/bonus.cpp
  src/frag.cpp
  src/clrtbl.cpp
  src/letterrender.cpp
  src/gamepad.cpp
  src/rand.cpp
  src/attractmanager.cpp
  src/soundmanager.cpp
  src/background.cpp
  src/foe.cc
  src/foecommand.cc
  src/barragemanager.cc
  src/SDL/SDL.cpp
  src/SDL/stdio_wrapper.cpp
  src/SDL/dirent_wrapper.cpp
  )

# Add SGL workarea source (from SaturnRingLib's shared.mk SYSSOURCES)
set(SGL_WORKAREA_SOURCE "${SRL_SGL_DIR}/SRC/workarea.c")
if(EXISTS "${SGL_WORKAREA_SOURCE}")
    list(APPEND SOURCES_NOIZ2SA "${SGL_WORKAREA_SOURCE}")
else()
    message(WARNING "SGL workarea.c not found at: ${SGL_WORKAREA_SOURCE}")
endif()

# Add SGL preloader source (C++ preloader for SGL initialization)
set(SGL_PRELOADER_SOURCE "${SRL_SGL_DIR}/SRC/preloader.cxx")
if(EXISTS "${SGL_PRELOADER_SOURCE}")
    list(APPEND SOURCES_NOIZ2SA "${SGL_PRELOADER_SOURCE}")
else()
    message(WARNING "SGL preloader.cxx not found at: ${SGL_PRELOADER_SOURCE}")
endif()

set(CD_FILES
      0.bin
    )

add_compile_definitions( MODEL_S
                         _SH )

add_executable( noiz2sa.elf ${SOURCES_NOIZ2SA} )

target_include_directories(
    noiz2sa.elf PUBLIC
    # SaturnRingLib headers (MUST come first to override system headers)
    ${SRL_INSTALL_ROOT}/saturnringlib
    src/SDL
    ${SRL_INSTALL_ROOT}/modules/sgl/INC
    ${SRL_INSTALL_ROOT}/modules/dummy
    ${SRL_INSTALL_ROOT}/modules/SaturnMathPP
    ${SRL_INSTALL_ROOT}/modules/tlsf
    # Project headers
    .
    src
    src/bulletml
    ${BULLETML_INCLUDE_DIRS}
    )

target_compile_options(noiz2sa.elf PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
                                                 $<$<COMPILE_LANGUAGE:C>:${c_flags}> )

target_compile_definitions(noiz2sa.elf PUBLIC
    $<$<COMPILE_LANGUAGE:C>:bool=int>
    $<$<COMPILE_LANGUAGE:C>:true=1>
    $<$<COMPILE_LANGUAGE:C>:false=0>
)

target_link_options(noiz2sa.elf PUBLIC
    "SHELL:-Xlinker -Map -Xlinker noiz2sa.map"
    "SHELL:-Xlinker --strip-debug"
    "SHELL:-Xlinker -fno-lto"
    "SHELL:-m2 -nostartfiles"
    "SHELL:-T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-lc -lgcc -lm"
)

# Apply custom linker flags from SRL configuration
if(NOT SRL_CUSTOM_LDFLAGS STREQUAL "")
    target_link_options(noiz2sa.elf PUBLIC "SHELL:${SRL_CUSTOM_LDFLAGS}")
endif()

target_link_libraries (noiz2sa.elf PUBLIC bulletml_binary )
srl_link_target(noiz2sa.elf)

# ============================================================================
# Post-Build Steps: Asset Directories, Binary Conversion, Sound Driver
# ============================================================================

# Create asset directories
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/cd/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/cd/music"
    COMMENT "Creating asset directories"
)

# Create required text files if they don't exist
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "NOT Abstracted by SEGA" > "${CMAKE_BINARY_DIR}/cd/data/ABS.TXT"
    COMMAND ${CMAKE_COMMAND} -E echo "NOT Bibliographiced by SEGA" > "${CMAKE_BINARY_DIR}/cd/data/BIB.TXT"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/cd/data/CPY.TXT"
    COMMENT "Creating required metadata files"
)

# Convert ELF to binary (equivalent to shared.mk convert_binary target)
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:noiz2sa.elf>" "${CMAKE_BINARY_DIR}/cd/data/0.bin"
    COMMENT "Converting ELF to binary: cd/data/0.bin"
    VERBATIM
)

# Copy sound driver files when SRL_USE_SGL_SOUND_DRIVER is enabled
if(SRL_USE_SGL_SOUND_DRIVER)
    set(SGL_DRV_DIR "${CMAKE_SOURCE_DIR}/SaturnRingLib/modules/sgl/DRV")
    if(EXISTS "${SGL_DRV_DIR}")
        add_custom_command(TARGET noiz2sa.elf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SGL_DRV_DIR}" "${CMAKE_BINARY_DIR}/cd/data"
            COMMENT "Copying SGL sound driver files to cd/data"
        )
    else()
        message(WARNING "SGL_USE_SGL_SOUND_DRIVER enabled but DRV directory not found: ${SGL_DRV_DIR}")
    endif()
    
    # Copy frequency analysis DSP file if enabled
    if(SRL_ENABLE_FREQ_ANALYSIS)
        set(SGL_DSP_FILE "${CMAKE_SOURCE_DIR}/SaturnRingLib/modules/sgl/DSP/3BANDANA.EXB")
        if(EXISTS "${SGL_DSP_FILE}")
            add_custom_command(TARGET noiz2sa.elf POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${SGL_DSP_FILE}" "${CMAKE_BINARY_DIR}/cd/data/3BANDANA.EXB"
                COMMENT "Copying frequency analysis DSP file"
            )
        else()
            message(WARNING "SRL_ENABLE_FREQ_ANALYSIS enabled but 3BANDANA.EXB not found: ${SGL_DSP_FILE}")
        endif()
    endif()
endif()

# ============================================================================
# ISO, BIN/CUE, and Audio Generation (from shared.mk build_bin_cue target)
# ============================================================================

# Set up build output paths
set(BUILD_ISO "${CMAKE_BINARY_DIR}/cd.iso")
set(BUILD_BIN "${CMAKE_BINARY_DIR}/cd.bin")
set(BUILD_CUE "${CMAKE_BINARY_DIR}/cd.cue")

# Music directory can be customized via CMake variable
if(NOT DEFINED MUSIC_DIR)
    # Try to find it relative to project or use default location
    if(EXISTS "${CMAKE_SOURCE_DIR}/noiz2sa_share/music")
        set(MUSIC_DIR "${CMAKE_SOURCE_DIR}/noiz2sa_share/music")
    else()
        set(MUSIC_DIR "${CMAKE_SOURCE_DIR}/noiz2sa_share/ogg")
    endif()
endif()

set(MUSIC_TRACKLIST "${MUSIC_DIR}/tracklist")
set(ASSETS_DIR "${CMAKE_BINARY_DIR}/cd/data")

# Locate or generate IP.BIN (system boot sector)
if(SRL_IPBIN)
    set(IPFILE "${SRL_IPBIN}")
else()
    # Look for IP.BIN in standard Saturn locations
    if(EXISTS "${CMAKE_SOURCE_DIR}/cd/data/IP.BIN")
        set(IPFILE "${CMAKE_SOURCE_DIR}/cd/data/IP.BIN")
    elseif(EXISTS "${SRL_INSTALL_ROOT}/resources/IP.BIN")
        set(IPFILE "${SRL_INSTALL_ROOT}/resources/IP.BIN")
    else()
        # Use default SGL IP.BIN from SaturnRingLib
        set(IPFILE "${SRL_SGL_DIR}/../../resources/IP.BIN")
    endif()
endif()

if(NOT EXISTS "${IPFILE}")
    message(WARNING "IP.BIN not found at: ${IPFILE}")
    message(WARNING "ISO generation will fail without IP.BIN")
endif()

# Find xorrisofs for ISO creation
find_program(XORRISOFS_EXECUTABLE xorrisofs)
if(NOT XORRISOFS_EXECUTABLE)
    message(WARNING "xorrisofs not found - ISO generation will not work")
    message(STATUS "Install via: apt-get install xorriso")
endif()

# Find iso2raw for BIN/CUE conversion
if(WIN32)
    find_program(ISO2RAW_EXECUTABLE iso2raw.exe)
    if(NOT ISO2RAW_EXECUTABLE)
        # Try alternate Windows location
        find_program(ISO2RAW_EXECUTABLE iso2raw.exe 
                     PATHS "C:/Program Files/iso2raw" "C:/Program Files (x86)/iso2raw")
    endif()
else()
    find_program(ISO2RAW_EXECUTABLE iso2raw)
    if(NOT ISO2RAW_EXECUTABLE)
        # Try common Unix locations
        find_program(ISO2RAW_EXECUTABLE iso2raw
                     PATHS /usr/local/bin /opt/iso2raw)
    endif()
endif()

if(NOT ISO2RAW_EXECUTABLE)
    message(WARNING "iso2raw not found - BIN/CUE conversion will not work")
    message(STATUS "For Linux: build from source or use precompiled binary")
    message(STATUS "For macOS: install from Homebrew or build from source")
endif()

# Find sox for audio conversion
find_program(SOX_EXECUTABLE sox)
if(NOT SOX_EXECUTABLE)
    message(WARNING "sox not found - audio conversion will not work")
    message(STATUS "Install via: apt-get install sox libsox-fmt-all")
endif()

# ============================================================================
# ISO Generation Target
# ============================================================================
set(ISO_GENERATION_SCRIPT "${CMAKE_BINARY_DIR}/generate_iso.sh")
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Generating ISO with xorrisofs..."
    COMMENT "Creating ISO image (cd.iso)"
)

# Generate ISO creation script
file(GENERATE OUTPUT "${ISO_GENERATION_SCRIPT}"
    CONTENT "#!/bin/bash\n
set -e\n
IPFILE=\"${IPFILE}\"\n
ASSETS_DIR=\"${ASSETS_DIR}\"\n
BUILD_ISO=\"${BUILD_ISO}\"\n
XORRISOFS=\"${XORRISOFS_EXECUTABLE}\"\n
\n
if [ ! -f \"\$IPFILE\" ]; then\n
    echo \"ERROR: IP.BIN not found at: \$IPFILE\"\n
    exit 1\n
fi\n
\n
if [ ! -d \"\$ASSETS_DIR\" ]; then\n
    echo \"ERROR: Assets directory not found: \$ASSETS_DIR\"\n
    exit 1\n
fi\n
\n
echo \"Creating ISO image...\"\n
\"\$XORRISOFS\" -r -J -C 0,32 \\\n
    -sysid=\"SEGA SATURN\" \\\n
    -A \"SEGA SATURN\" \\\n
    -publisher-id=\"libburnia\" \\\n
    -application-id=\"xorrisofs\" \\\n
    \"-boot_image\" \"isolinux\" \"bin=\$IPFILE\" \\\n
    -o \"\$BUILD_ISO\" \n
    \"\$ASSETS_DIR\"\n
\n
if [ ! -f \"\$BUILD_ISO\" ]; then\n
    echo \"ERROR: Failed to create ISO\"\n
    exit 1\n
fi\n
\n
echo \"ISO created successfully: \$BUILD_ISO\"\n
"
)

add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "ISO generation configured (run manually or with custom build target)"
    COMMENT "ISO generation script ready"
)

# ============================================================================
# BIN/CUE Generation Target
# ============================================================================
set(BIN_CUE_GENERATION_SCRIPT "${CMAKE_BINARY_DIR}/generate_bin_cue.sh")
file(GENERATE OUTPUT "${BIN_CUE_GENERATION_SCRIPT}"
    CONTENT "#!/bin/bash\n
set -e\n
BUILD_ISO=\"${BUILD_ISO}\"\n
BUILD_BIN=\"${BUILD_BIN}\"\n
BUILD_CUE=\"${BUILD_CUE}\"\n
ISO2RAW=\"${ISO2RAW_EXECUTABLE}\"\n
\n
if [ -z \"\$ISO2RAW\" ] || [ ! -f \"\$ISO2RAW\" ]; then\n
    echo \"ERROR: iso2raw not found\"\n
    exit 1\n
fi\n
\n
if [ ! -f \"\$BUILD_ISO\" ]; then\n
    echo \"ERROR: ISO file not found: \$BUILD_ISO\"\n
    exit 1\n
fi\n
\n
echo \"Converting ISO to BIN/CUE...\"\n
\"\$ISO2RAW\" \"\$BUILD_ISO\" \"\$BUILD_BIN\"\n
\n
if [ ! -f \"\$BUILD_BIN\" ] || [ ! -f \"\$BUILD_CUE\" ]; then\n
    echo \"ERROR: Failed to create BIN/CUE files\"\n
    exit 1\n
fi\n
\n
echo \"BIN/CUE created successfully\"\n
"
)

add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "BIN/CUE generation configured (requires ISO first)"
    COMMENT "BIN/CUE generation script ready"
)

# ============================================================================
# Audio File Handling and Track Appending
# ============================================================================
set(AUDIO_GENERATION_SCRIPT "${CMAKE_BINARY_DIR}/add_audio_tracks.sh")
file(GENERATE OUTPUT "${AUDIO_GENERATION_SCRIPT}"
    CONTENT "#!/bin/bash\n
# Audio file discovery, conversion, and appending to CD image\n
set -e\n
\n
MUSIC_DIR=\"${MUSIC_DIR}\"\n
MUSIC_TRACKLIST=\"${MUSIC_TRACKLIST}\"\n
BUILD_BIN=\"${BUILD_BIN}\"\n
BUILD_CUE=\"${BUILD_CUE}\"\n
SOX=\"${SOX_EXECUTABLE}\"\n
\n
# Check if BIN/CUE files exist\n
if [ ! -f \"\$BUILD_BIN\" ] || [ ! -f \"\$BUILD_CUE\" ]; then\n
    echo \"ERROR: BIN/CUE files not found. Generate them first.\"\n
    exit 1\n
fi\n
\n
# Initialize audio file list\n
track=2\n
sectors=\$((\$(stat -c%s \"\$BUILD_BIN\") / 2352))\n
total_size=\$(stat -c%s \"\$BUILD_BIN\")\n
\n
# Audio file discovery: use tracklist if available, otherwise auto-discover\n
if [ -f \"\$MUSIC_TRACKLIST\" ]; then\n
    echo \"Using audio tracks from tracklist: \$MUSIC_TRACKLIST\"\n
    cp \"\$MUSIC_TRACKLIST\" .audio_files_temp\n
else\n
    echo \"Auto-discovering audio files in: \$MUSIC_DIR\"\n
    find \"\$MUSIC_DIR\" -type f \\\\( -iname '*.mp3' -o -iname '*.wav' -o -iname '*.ogg' -o -iname '*.flac' -o -iname '*.aac' -o -iname '*.m4a' -o -iname '*.wma' \\\\) | sort > .audio_files_temp || true\n
fi\n
\n
# Process each audio file\n
if [ ! -f .audio_files_temp ] || [ ! -s .audio_files_temp ]; then\n
    echo \"No audio files found\"\n
    touch .audio_files_temp\n
fi\n
\n
while IFS= read -r i; do\n
    [ -z \"\$i\" ] && continue\n
    # Remove quotes if present\n
    i=\${i#\\\\\"}\n
    i=\${i%\\\\\"}\n
    [ ! -f \"\$i\" ] && continue\n
    \n
    rawfile=\"\$MUSIC_DIR/\$(basename \"\$i\" .\${i##*.}).raw\"\n
    echo \"Converting audio track: \$i\"\n
    echo \"  Output: \$rawfile\"\n
    \n
    # Convert to 44.1kHz, 16-bit signed PCM using sox\n
    if [ -f \"\$rawfile\" ]; then\n
        echo \"  (already converted)\"\n
    else\n
        \"\$SOX\" \"\$i\" -r 44100 -b 16 -e signed \"\$rawfile\"\n
    fi\n
    \n
    # Sector-align the raw audio file to 2352 bytes (CD sector size)\n
    current_size=\$(stat -c%s \"\$rawfile\")\n
    remainder=\$((current_size % 2352))\n
    if [ \$remainder -ne 0 ]; then\n
        padding=\$((2352 - remainder))\n
        echo \"  Padding with \$padding bytes to sector-align\"\n
        dd if=/dev/zero bs=1 count=\$padding >> \"\$rawfile\" 2>/dev/null\n
    fi\n
    \n
    echo \"Track \$track: starts at sector \$sectors\"\n
    echo '  TRACK' \$(printf \"%02d\" \$track) 'AUDIO' >> \"\$BUILD_CUE\"\n
    \n
    # Add gap for audio tracks following data track (ECMA-130 spec)\n
    if [ \$track -eq 2 ]; then\n
        echo '    PREGAP   00:02:00' >> \"\$BUILD_CUE\"\n
    fi\n
    \n
    # Calculate MSF (Minutes:Seconds:Frames) format for CUE sheet\n
    index_sectors=\$sectors\n
    minutes=\$((index_sectors / (60 * 75)))\n
    remaining=\$((index_sectors % (60 * 75)))\n
    seconds=\$((remaining / 75))\n
    frames=\$((remaining % 75))\n
    msf=\$(printf \"%02d:%02d:%02d\" \$minutes \$seconds \$frames)\n
    \n
    echo \"  INDEX calculation: sector \$index_sectors = \$msf\"\n
    \n
    if [ \$frames -ge 75 ]; then\n
        echo \"  ERROR: Invalid frame count \$frames (must be 0-74)\"\n
        echo \"  This indicates sector misalignment in the audio file\"\n
        exit 1\n
    fi\n
    \n
    echo '    INDEX 01' \$msf >> \"\$BUILD_CUE\"\n
    \n
    # Verify sector alignment and append to BIN\n
    size=\$(stat -c%s \"\$rawfile\")\n
    if [ \$((size % 2352)) -ne 0 ]; then\n
        echo \"  ERROR: File \$rawfile is not sector-aligned (\$size bytes)\"\n
        echo \"  File size must be a multiple of 2352 bytes\"\n
        exit 1\n
    fi\n
    \n
    sectors_in_file=\$((size / 2352))\n
    echo \"  Adding \$rawfile: \$size bytes (\$sectors_in_file sectors)\"\n
    cat \"\$rawfile\" >> \"\$BUILD_BIN\"\n
    total_size=\$((total_size + size))\n
    echo \"  New total: \$total_size bytes (\$((total_size / 2352)) sectors)\"\n
    \n
    track=\$((track + 1))\n
    sectors=\$((sectors + sectors_in_file))\n
done < .audio_files_temp\n
\n
rm -f .audio_files_temp\n
\n
echo \"Audio tracks added successfully\"\n
echo \"Final CD image: \$BUILD_BIN (\$total_size bytes)\"\n
"
)

add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Audio track handling configured"
    COMMENT "Audio conversion and appending script ready"
)

# ============================================================================
# Custom Target for Complete CD Generation (ISO -> BIN/CUE -> Audio)
# ============================================================================

# Helper script to make all generation scripts executable
add_custom_command(TARGET noiz2sa.elf POST_BUILD
    COMMAND chmod +x "${ISO_GENERATION_SCRIPT}" || true
    COMMAND chmod +x "${BIN_CUE_GENERATION_SCRIPT}" || true
    COMMAND chmod +x "${AUDIO_GENERATION_SCRIPT}" || true
    COMMENT "Making CD generation scripts executable"
)

# ============================================================================
# Automatic CD Image Generation as POST_BUILD Steps (if tools available)
# ============================================================================

# Generate ISO automatically if xorrisofs is available
if(XORRISOFS_EXECUTABLE)
    add_custom_command(TARGET noiz2sa.elf POST_BUILD
        COMMAND bash "${ISO_GENERATION_SCRIPT}"
        COMMENT "Generating ISO image (cd.iso)"
        VERBATIM
    )
    set(CD_ISO_AVAILABLE TRUE)
    set(ISO_BUILD_MSG "✓ ISO will be generated automatically")
else()
    set(CD_ISO_AVAILABLE FALSE)
    set(ISO_BUILD_MSG "✗ ISO generation requires xorrisofs (apt-get install xorriso)")
endif()

# Generate BIN/CUE automatically if iso2raw is available
if(ISO2RAW_EXECUTABLE AND CD_ISO_AVAILABLE)
    add_custom_command(TARGET noiz2sa.elf POST_BUILD
        COMMAND bash "${BIN_CUE_GENERATION_SCRIPT}"
        COMMENT "Converting ISO to BIN/CUE format (cd.bin, cd.cue)"
        VERBATIM
    )
    set(CD_BIN_CUE_AVAILABLE TRUE)
    set(BIN_CUE_BUILD_MSG "✓ BIN/CUE will be generated automatically")
elseif(ISO2RAW_EXECUTABLE)
    set(CD_BIN_CUE_AVAILABLE TRUE)
    set(BIN_CUE_BUILD_MSG "⚠ BIN/CUE can be generated (requires cd-iso first)")
else()
    set(CD_BIN_CUE_AVAILABLE FALSE)
    set(BIN_CUE_BUILD_MSG "✗ BIN/CUE requires iso2raw (build from: https://github.com/gianlucarenzi/iso2raw)")
endif()

# Add audio tracks automatically if sox is available
if(SOX_EXECUTABLE AND CD_BIN_CUE_AVAILABLE AND CD_ISO_AVAILABLE)
    add_custom_command(TARGET noiz2sa.elf POST_BUILD
        COMMAND bash "${AUDIO_GENERATION_SCRIPT}"
        COMMENT "Adding audio tracks to CD image"
        VERBATIM
    )
    set(CD_AUDIO_AVAILABLE TRUE)
    set(AUDIO_BUILD_MSG "✓ Audio tracks will be added automatically")
elseif(SOX_EXECUTABLE)
    set(CD_AUDIO_AVAILABLE TRUE)
    set(AUDIO_BUILD_MSG "⚠ Audio tracks can be added (run after ISO/BIN/CUE generation)")
else()
    set(CD_AUDIO_AVAILABLE FALSE)
    set(AUDIO_BUILD_MSG "✗ Audio requires sox (apt-get install sox libsox-fmt-all)")
endif()

# ============================================================================
# Optional Custom Targets for Manual Control
# ============================================================================

# Manual ISO generation target (for re-generation without rebuilding everything)
add_custom_target(cd-iso
    COMMAND bash "${ISO_GENERATION_SCRIPT}"
    COMMENT "Manually generating ISO image (cd.iso)"
    USES_TERMINAL
)

# Manual BIN/CUE generation target
add_custom_target(cd-bin-cue
    COMMAND bash "${BIN_CUE_GENERATION_SCRIPT}"
    COMMENT "Manually converting ISO to BIN/CUE format"
    USES_TERMINAL
)

# Manual audio addition target
add_custom_target(cd-audio
    COMMAND bash "${AUDIO_GENERATION_SCRIPT}"
    COMMENT "Manually adding audio tracks to CD image"
    USES_TERMINAL
)

# Combined target for complete CD generation
add_custom_target(cd-all
    COMMAND ${CMAKE_COMMAND} -E echo "Complete CD generation (cd.iso, cd.bin, cd.cue with audio)"
    COMMENT "Note: Complete CD already built during default build if tools available"
)

set(CD_ALL_AVAILABLE TRUE)

message(STATUS "")
message(STATUS "========== CD Image Generation ==========")
message(STATUS "${ISO_BUILD_MSG}")
message(STATUS "${BIN_CUE_BUILD_MSG}")
message(STATUS "${AUDIO_BUILD_MSG}")
if(NOT CD_ISO_AVAILABLE OR NOT CD_BIN_CUE_AVAILABLE OR NOT CD_AUDIO_AVAILABLE)
    message(STATUS "")
    message(STATUS "To enable full automatic CD generation, install:")
    if(NOT CD_ISO_AVAILABLE)
        message(STATUS "  - xorrisofs: apt-get install xorriso")
    endif()
    if(NOT CD_BIN_CUE_AVAILABLE AND NOT ISO2RAW_EXECUTABLE)
        message(STATUS "  - iso2raw: https://github.com/gianlucarenzi/iso2raw")
    endif()
    if(NOT CD_AUDIO_AVAILABLE AND NOT SOX_EXECUTABLE)
        message(STATUS "  - sox: apt-get install sox libsox-fmt-all")
    endif()
endif()
message(STATUS "")
message(STATUS "Manual targets (for re-generation without rebuild):")
message(STATUS "  cmake --build . --target cd-iso")
message(STATUS "  cmake --build . --target cd-bin-cue")
message(STATUS "  cmake --build . --target cd-audio")
message(STATUS "=========================================")
message(STATUS "")

# ============================================================================
# SRL Configuration Summary (similar to shared.mk info output)
# ============================================================================
message(STATUS "===== SaturnRingLib Configuration =====")
message(STATUS "Maximum textures: ${SRL_MAX_TEXTURES}")
message(STATUS "Maximum vertices: ${SGL_MAX_VERTICES}")
message(STATUS "Maximum polygons: ${SGL_MAX_POLYGONS}")
message(STATUS "Maximum events: ${SGL_MAX_EVENTS}")
message(STATUS "Maximum work units: ${SGL_MAX_WORKS}")
message(STATUS "Video mode: ${SRL_MODE}")
message(STATUS "Framerate: ${SRL_FRAMERATE}")
if(NOT SRL_LOG_LEVEL STREQUAL "")
    message(STATUS "Log level: ${SRL_LOG_LEVEL}")
else()
    message(STATUS "Log level: NONE")
endif()
if(NOT SRL_LOG_OUTPUT STREQUAL "")
    message(STATUS "Log output method: ${SRL_LOG_OUTPUT}")
else()
    message(STATUS "Log output method: NONE")
endif()
message(STATUS "Maximum log length: ${SRL_DEBUG_MAX_LOG_LENGTH}")
message(STATUS "SGL sound driver: ${SRL_USE_SGL_SOUND_DRIVER}")
if(SRL_MALLOC_METHOD STREQUAL "TLSF")
    message(STATUS "Memory allocator: TLSF")
else()
    message(STATUS "Memory allocator: Default")
endif()
message(STATUS "=========================================")

# ============================================================================
# Pre/Post-build Script Support (equivalent to shared.mk -include hooks)
# ============================================================================
# Pre-build hook: execute before compilation
if(EXISTS "${CMAKE_SOURCE_DIR}/prebuild.cmake")
    message(STATUS "Including pre-build script: prebuild.cmake")
    include("${CMAKE_SOURCE_DIR}/prebuild.cmake")
endif()

# Post-build hook: execute after linking
if(EXISTS "${CMAKE_SOURCE_DIR}/postbuild.cmake")
    message(STATUS "Including post-build script: postbuild.cmake")
    add_custom_command(TARGET noiz2sa.elf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/postbuild.cmake"
        COMMENT "Running post-build script"
    )
endif()

#target_link_libraries(noiz2sa.elf PUBLIC ${SEGASGL}/lib/libsgl.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_scl.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_mth.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_spr.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_cdc.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_per.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_int.a )
