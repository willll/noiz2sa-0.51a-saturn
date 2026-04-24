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
set(SRL_MAX_CD_FILES "100" CACHE STRING "Maximum CD files" FORCE)
set(SRL_MAX_CD_RETRIES "5" CACHE STRING "Maximum CD retries" FORCE)

# Display configuration
option(SRL_HIGH_RES "Enable high resolution mode" OFF)
set(SRL_FRAMERATE "0" CACHE STRING "Target framerate (0 = auto)")
set(SRL_MODE "NTSC" CACHE STRING "Video mode: NTSC or PAL")

# Memory configuration
set(SRL_MAX_TEXTURES "75" CACHE STRING "Maximum textures")

# Debug configuration
option(DEBUG "Enable debug mode" OFF)
set(SRL_LOG_LEVEL "TRACE" CACHE STRING "Log level (0-4, empty = disabled)" FORCE)
set(SRL_LOG_OUTPUT "EMULATOR" CACHE STRING "Log output method")
set(SRL_DEBUG_MAX_PRINT_LENGTH "45" CACHE STRING "Maximum debug print length")
set(SRL_DEBUG_MAX_LOG_LENGTH "120" CACHE STRING "Maximum debug log length")

# Sound configuration
option(SRL_USE_SGL_SOUND_DRIVER "Use SGL sound driver" OFF)
option(SRL_ENABLE_FREQ_ANALYSIS "Enable frequency analysis" OFF)
set(MATH_PERFORMANCE_MODE "TURBO" CACHE STRING "Math precision: ACCURATE, FAST, or TURBO")
option(NOIZ2SA_ENABLE_SOUND "Enable in-game sound (PCM SFX + CDDA BGM)" ON)
option(NOIZ2SA_ENABLE_PCM_SFX "Enable PCM SFX loading through Ponesound backend" ON)

# SRL SGL sound driver path uses WAV playback. Ponesound path uses PCM playback.
# Keep these backend toggles mutually exclusive to avoid mixed configuration ambiguity.
if(SRL_USE_SGL_SOUND_DRIVER AND NOIZ2SA_ENABLE_PCM_SFX)
    message(FATAL_ERROR
        "Invalid sound configuration: SRL_USE_SGL_SOUND_DRIVER=ON and NOIZ2SA_ENABLE_PCM_SFX=ON are mutually exclusive. "
        "Use SGL driver for WAV SFX, or disable SGL and enable NOIZ2SA_ENABLE_PCM_SFX for Ponesound PCM SFX.")
endif()

# Gameplay performance toggles
option(NOIZ2SA_ENABLE_SMOKE "Enable smoke/afterimage decay effect" ON)
set(NOIZ2SA_SCREEN_DIVISOR "2" CACHE STRING "Screen render divisor (1=full, 2=half, 4=quarter)")
set_property(CACHE NOIZ2SA_SCREEN_DIVISOR PROPERTY STRINGS 1 2 4)
option(NOIZ2SA_DISABLE_DOUBLE_BUFFER "Disable frame synchronize/swap pacing" OFF)

# Background color after loading
set(NOIZ2SA_POST_LOAD_BG_COLOR "BLUE" CACHE STRING "Background color after loading completes")
set_property(CACHE NOIZ2SA_POST_LOAD_BG_COLOR PROPERTY STRINGS BLUE WHITE BLACK)

# SGL Work Area Settings
set(SGL_MAX_VERTICES "1800" CACHE STRING "Maximum vertices")
set(SGL_MAX_POLYGONS "1200" CACHE STRING "Maximum polygons")
set(SGL_MAX_EVENTS "64" CACHE STRING "Maximum events")
set(SGL_MAX_WORKS "256" CACHE STRING "Maximum work units")

# Memory allocator
set(SRL_MALLOC_METHOD "TLSF" CACHE STRING "Memory allocator method (TLSF or empty)")

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

# Gameplay colors
if(NOIZ2SA_POST_LOAD_BG_COLOR STREQUAL "BLUE")
    add_compile_definitions(
        NOIZ2SA_POST_LOAD_BG_R=20
        NOIZ2SA_POST_LOAD_BG_G=10
        NOIZ2SA_POST_LOAD_BG_B=50
    )
elseif(NOIZ2SA_POST_LOAD_BG_COLOR STREQUAL "WHITE")
    add_compile_definitions(
        NOIZ2SA_POST_LOAD_BG_R=255
        NOIZ2SA_POST_LOAD_BG_G=255
        NOIZ2SA_POST_LOAD_BG_B=255
    )
elseif(NOIZ2SA_POST_LOAD_BG_COLOR STREQUAL "BLACK")
    add_compile_definitions(
        NOIZ2SA_POST_LOAD_BG_R=0
        NOIZ2SA_POST_LOAD_BG_G=0
        NOIZ2SA_POST_LOAD_BG_B=0
    )
endif()

# Sound
if(SRL_USE_SGL_SOUND_DRIVER)
    add_compile_definitions(SRL_USE_SGL_SOUND_DRIVER=1)
    if(SRL_ENABLE_FREQ_ANALYSIS)
        add_compile_definitions(SRL_ENABLE_FREQ_ANALYSIS=1)
    endif()
endif()

if(NOIZ2SA_ENABLE_SOUND)
    add_compile_definitions(NOIZ2SA_ENABLE_SOUND=1)
else()
    add_compile_definitions(NOIZ2SA_ENABLE_SOUND=0)
endif()

if(NOIZ2SA_ENABLE_PCM_SFX)
    add_compile_definitions(NOIZ2SA_ENABLE_PCM_SFX=1)
else()
    add_compile_definitions(NOIZ2SA_ENABLE_PCM_SFX=0)
endif()

# Gameplay performance toggles
if(NOIZ2SA_ENABLE_SMOKE)
    add_compile_definitions(NOIZ2SA_ENABLE_SMOKE=1)
else()
    add_compile_definitions(NOIZ2SA_ENABLE_SMOKE=0)
endif()

add_compile_definitions(NOIZ2SA_SCREEN_DIVISOR=${NOIZ2SA_SCREEN_DIVISOR})

if(NOIZ2SA_DISABLE_DOUBLE_BUFFER)
    add_compile_definitions(NOIZ2SA_DISABLE_DOUBLE_BUFFER=1)
else()
    add_compile_definitions(NOIZ2SA_DISABLE_DOUBLE_BUFFER=0)
endif()

# SGL Work Area
add_compile_definitions(
    SGL_MAX_VERTICES=${SGL_MAX_VERTICES}
    SGL_MAX_POLYGONS=${SGL_MAX_POLYGONS}
    SGL_MAX_EVENTS=${SGL_MAX_EVENTS}
    SGL_MAX_WORKS=${SGL_MAX_WORKS}
)

# TLSF Allocator
# Note: TLSF source is added to SOURCES_SGL_SYSTEM list above conditionally

# ============================================================================

set(as_flags )
set(c_flags
    -W
    -m2
    -c
    -O3
    -fno-lto
    -ffat-lto-objects
    -funroll-loops
    -fomit-frame-pointer
    -Wno-strict-aliasing
    -std=c2x
)

set(cxx_flags
    -std=c++23
    -W
    -m2
    -c
    -O3
    -fno-lto
    -ffat-lto-objects
    -funroll-loops
    -fomit-frame-pointer
    -Wno-strict-aliasing
    -fpermissive
    -fno-exceptions
    -fno-rtti
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
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
    src/canvas.cpp
    src/canvas_palette.cpp
  src/letterrender.cpp
  src/gamepad.cpp
  src/rand.cpp
  src/attractmanager.cpp
  src/loading_screen.cpp
  src/soundmanager.cpp
  src/background.cpp
  src/foe.cc
  src/foecommand.cc
  src/barragemanager.cc
)

# SGL Preloader - MUST be linked first (before all other object files)
# Following shared.mk: PRELOADERSOURCES = $(SGLDIR)/SRC/preloader.cxx
set(SGL_PRELOADER_SOURCE "${SRL_SGL_DIR}/SRC/preloader.cxx")
if(NOT EXISTS "${SGL_PRELOADER_SOURCE}")
    message(FATAL_ERROR "SGL preloader.cxx not found at: ${SGL_PRELOADER_SOURCE}")
endif()

# System sources (following shared.mk SYSSOURCES pattern)
# These are added after user sources to match linking order
set(SOURCES_SGL_SYSTEM)
set(SGL_WORKAREA_SOURCE "${SRL_SGL_DIR}/SRC/workarea.c")
if(EXISTS "${SGL_WORKAREA_SOURCE}")
    list(APPEND SOURCES_SGL_SYSTEM "${SGL_WORKAREA_SOURCE}")
else()
    message(FATAL_ERROR "SGL workarea.c not found at: ${SGL_WORKAREA_SOURCE}")
endif()

# Add TLSF allocator source (conditional, following shared.mk)
if(SRL_MALLOC_METHOD STREQUAL "TLSF")
    set(TLSF_SOURCE "${SRL_MODULES_DIR}/tlsf/tlsf.c")
    if(EXISTS "${TLSF_SOURCE}")
        list(APPEND SOURCES_SGL_SYSTEM "${TLSF_SOURCE}")
        message(STATUS "Using TLSF memory allocator")
    else()
        message(WARNING "TLSF requested but source not found: ${TLSF_SOURCE}")
    endif()
endif()

set(CD_FILES
      0.bin
    )

# ============================================================================
# CD Build Configuration (must come before add_executable)
# ============================================================================

# CD_NAME is the base name for generated files
set(CD_NAME "noiz2sa")

# BUILD_DROP is where all build artifacts go (at project root, like shared.mk)
set(BUILD_DROP "${CMAKE_SOURCE_DIR}/BuildDrop")

# Music and assets directories
if(NOT DEFINED MUSIC_DIR)
    set(MUSIC_DIR "${CMAKE_SOURCE_DIR}/cd/music")
endif()

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/cd/data")

# Build output files (following shared.mk structure)
set(BUILD_ELF "${BUILD_DROP}/${CD_NAME}.elf")
set(BUILD_ISO "${BUILD_DROP}/${CD_NAME}.iso")
set(BUILD_BIN "${BUILD_DROP}/${CD_NAME}.bin")
set(BUILD_CUE "${BUILD_DROP}/${CD_NAME}.cue")
set(BUILD_MAP "${BUILD_DROP}/${CD_NAME}.map")
set(BUILD_IPBIN "${BUILD_DROP}/IP.BIN")

# Generate IP.BIN using IPMaker for this project.
set(ip_maker_verbose "")
if(CMAKE_VERBOSE_MAKEFILE)
    set(ip_maker_verbose "-v")
endif()

set(SATURN_IPMAKER_EXECUTABLE "$ENV{SATURN_IPMAKER}/IPMaker")
if(NOT EXISTS "${SATURN_IPMAKER_EXECUTABLE}")
    find_program(SATURN_IPMAKER_EXECUTABLE IPMaker)
endif()

if(NOT SATURN_IPMAKER_EXECUTABLE)
    message(FATAL_ERROR "IPMaker not found. Set SATURN_IPMAKER or add IPMaker to PATH.")
endif()

add_custom_command(
    OUTPUT "${BUILD_IPBIN}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_DROP}"
    COMMAND "${SATURN_IPMAKER_EXECUTABLE}"
    ARGS ${ip_maker_verbose} -o "${BUILD_IPBIN}" -t ${CD_NAME} -p 2
    COMMENT "Generating IP.BIN with IPMaker"
    VERBATIM
)

add_custom_target(generate_project_ip_bin DEPENDS "${BUILD_IPBIN}")

set(IPFILE "${BUILD_IPBIN}")

# Find xorrisofs for ISO creation (required for Saturn CD generation)
find_program(XORRISOFS_EXECUTABLE xorrisofs)

if(NOT XORRISOFS_EXECUTABLE)
    message(WARNING "xorrisofs not found in PATH")
    message(WARNING "ISO generation will not be available")
    message(WARNING "Install xorrisofs: apt-get install xorriso")
endif()

# Optional additional directory/files to include in ISO (matching shared.mk)
set(ENTRYPOINT "" CACHE STRING "Optional additional directory or files for ISO generation")

# Find iso2raw in SaturnRingLib tools (with fallback)
if(WIN32)
    set(ISO2RAW_EXECUTABLE "${CMAKE_SOURCE_DIR}/../SaturnRingLib/tools/bin/win/iso2raw/iso2raw.exe")
else()
    set(ISO2RAW_EXECUTABLE "${CMAKE_SOURCE_DIR}/../SaturnRingLib/tools/bin/lin/iso2raw/iso2raw")
    if(NOT EXISTS "${ISO2RAW_EXECUTABLE}")
        find_program(ISO2RAW_EXECUTABLE iso2raw)
    endif()
endif()

# Find sox for audio conversion
find_program(SOX_EXECUTABLE sox)

add_compile_definitions( MODEL_S
                         _SH )

# Build executable with direct source ordering for strict link order control
# Following shared.mk: PRELOADERSOURCES → OBJECTS → SYSOBJECTS → LIBS
add_executable(${CD_NAME}.elf
    "${SGL_PRELOADER_SOURCE}"
    ${SOURCES_NOIZ2SA}
    ${SOURCES_SGL_SYSTEM}
)

# Link libraries
target_link_libraries(${CD_NAME}.elf PUBLIC bulletml_binary)
srl_link_target(${CD_NAME}.elf)

# Clean BuildDrop before linking (redundant with clean-all, but ensures directory exists)
add_custom_command(TARGET ${CD_NAME}.elf PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_DROP}"
    COMMENT "Ensuring BuildDrop directory exists"
)

# Set output directory to BUILD_DROP and map file path
set_target_properties(${CD_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BUILD_DROP}"
)

target_include_directories(
    ${CD_NAME}.elf PUBLIC
    # SaturnRingLib headers (MUST come first to override system headers)
    ${SRL_INSTALL_ROOT}/saturnringlib
    ${CMAKE_SOURCE_DIR}/src/ponesound/INC
    ${CMAKE_SOURCE_DIR}/src/decompression/INC
    ${CMAKE_SOURCE_DIR}/src/smpc/INC
    src/SDL
    ${SRL_INSTALL_ROOT}/modules/sgl/INC
    ${SRL_INSTALL_ROOT}/modules/dummy
    ${SRL_INSTALL_ROOT}/modules/SaturnMathPP
    ${SRL_INSTALL_ROOT}/modules/tlsf
    # Project headers
    .
    src
    src/bulletml_binary
    )

target_compile_definitions(${CD_NAME}.elf PUBLIC
    ${BULLETML_INCLUDE_DIRS}
    )

target_compile_options(${CD_NAME}.elf PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
                                                 $<$<COMPILE_LANGUAGE:C>:${c_flags}> )

target_compile_definitions(${CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:C>:bool=int>
    $<$<COMPILE_LANGUAGE:C>:true=1>
    $<$<COMPILE_LANGUAGE:C>:false=0>
)

target_link_options(${CD_NAME}.elf PUBLIC
    "SHELL:-m2"
    "SHELL:-L${SRL_SGL_DIR}/LIB"
    "SHELL:-fno-lto"
    "SHELL:-Xlinker -T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-Xlinker -Map -Xlinker ${BUILD_MAP}"
    "SHELL:-Xlinker -e -Xlinker ___Start -nostartfiles"
)

# Match shared.mk GCC 14 behavior
if(CMAKE_C_COMPILER_VERSION MATCHES "^14\\.")
    target_link_options(${CD_NAME}.elf PUBLIC "SHELL:-specs=nosys.specs")
endif()

# Apply custom linker flags from SRL configuration
if(NOT SRL_CUSTOM_LDFLAGS STREQUAL "")
    target_link_options(${CD_NAME}.elf PUBLIC "SHELL:${SRL_CUSTOM_LDFLAGS}")
endif()

target_link_libraries (${CD_NAME}.elf PUBLIC bulletml_binary m )
srl_link_target(${CD_NAME}.elf)

# ============================================================================
# Post-Build Steps: Asset Directories, Binary Conversion, Sound Driver
# ============================================================================

# Custom target for deep clean of build artifacts
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning all build artifacts..."
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/CMakeFiles/${CD_NAME}.elf.dir"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BUILD_DROP}"
    COMMENT "Deep cleaning build artifacts and output directory"
    VERBATIM
)

# Create BUILD_DROP and asset directories at configure time
file(MAKE_DIRECTORY "${BUILD_DROP}")
file(MAKE_DIRECTORY "${ASSETS_DIR}")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cd/music")

# Create asset directories at build time (in case they're cleaned)
add_custom_command(TARGET ${CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/SOUNDS"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/cd/music"
    COMMENT "Ensuring asset directories exist"
)

# Convert SFX WAV assets into raw PCM files expected by Ponesound.
if(SOX_EXECUTABLE)
    add_custom_command(TARGET ${CD_NAME}.elf POST_BUILD
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/SHOT.WAV"   -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/SHOT.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/HIT.WAV"    -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/HIT.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/FOEDES.WAV" -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/FOEDES.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/BOSDES.WAV" -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/BOSDES.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/SHIPDS.WAV" -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/SHIPDS.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/BONUS.WAV"  -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/BONUS.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/EXTEND.WAV" -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/EXTEND.SGL.WAV"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/SHOT.WAV"   -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/SHOT.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/HIT.WAV"    -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/HIT.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/FOEDES.WAV" -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/FOEDES.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/BOSDES.WAV" -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/BOSDES.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/SHIPDS.WAV" -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/SHIPDS.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/BONUS.WAV"  -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/BONUS.PCM"
        COMMAND ${SOX_EXECUTABLE} "${ASSETS_DIR}/SOUNDS/EXTEND.WAV" -t raw -r 15360 -e signed-integer -b 8 -c 1 "${ASSETS_DIR}/SOUNDS/EXTEND.PCM"
        COMMENT "Converting SFX WAV files to SGL-compatible WAV and raw PCM"
        VERBATIM
    )
else()
    message(WARNING "sox not found: Ponesound SFX conversion (WAV->PCM) will be skipped")
endif()

# Create required text files (done at CMake config time)
file(WRITE "${ASSETS_DIR}/ABS.TXT" "NOT Abstracted by SEGA\n")
file(WRITE "${ASSETS_DIR}/BIB.TXT" "NOT Bibliographiced by SEGA\n")
file(WRITE "${ASSETS_DIR}/CPY.TXT" "")

# Convert ELF to binary (equivalent to shared.mk convert_binary target)
add_custom_command(TARGET ${CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${CD_NAME}.elf>" "${ASSETS_DIR}/0.bin"
    COMMENT "Converting ELF to binary: ${ASSETS_DIR}/0.bin"
    VERBATIM
)

# Copy sound driver files when SRL_USE_SGL_SOUND_DRIVER is enabled
if(SRL_USE_SGL_SOUND_DRIVER)
    set(SGL_DRV_DIR "${SRL_ROOT}/modules/sgl/DRV")
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
        set(SGL_DSP_FILE "${SRL_ROOT}/modules/sgl/DSP/3BANDANA.EXB")
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
# CD Image Generation Targets (following shared.mk build_bin_cue pattern)
# ============================================================================

# Create ISO using xorrisofs
if(XORRISOFS_EXECUTABLE)
    add_custom_target(create_iso
        DEPENDS ${CD_NAME}.elf generate_project_ip_bin
        COMMAND ${CMAKE_COMMAND} -E echo "Creating ISO image with xorrisofs..."
        COMMAND ${XORRISOFS_EXECUTABLE}
            --norock -quiet 
            -sysid "SEGA SATURN"
            -volid "${CD_NAME}"
            -volset "${CD_NAME}"
            -publisher "SEGA ENTERPRISES, LTD."
            -preparer "SEGA ENTERPRISES, LTD."
            -appid "${CD_NAME}"
            -abstract "NOT Abstracted by SEGA"
            -copyright ""
            -biblio ""
            -generic-boot "${IPFILE}"
            -full-iso9660-filenames
            -o "${BUILD_ISO}"
            "${ASSETS_DIR}" ${ENTRYPOINT}
        COMMENT "Building ISO image: ${BUILD_ISO}"
        VERBATIM
    )
else()
    add_custom_target(create_iso
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "ERROR: xorrisofs not found"
        COMMAND ${CMAKE_COMMAND} -E message "  Please install xorrisofs: apt-get install xorriso"
        COMMAND ${CMAKE_COMMAND} -E message "  Or run: /saturn/SaturnRingLib/setup_compiler.bat"
        COMMAND false
    )
endif()

# Create BIN/CUE from ISO using iso2raw
if(EXISTS "${ISO2RAW_EXECUTABLE}")
    add_custom_target(create_bin_cue
        DEPENDS create_iso
        COMMAND ${CMAKE_COMMAND} -E echo "Converting ISO to BIN/CUE using iso2raw..."
        COMMAND "${ISO2RAW_EXECUTABLE}" "${BUILD_ISO}"
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/create_cue_header.sh" "${BUILD_CUE}" "${CD_NAME}.bin"
        COMMENT "Creating BIN/CUE files"
    )
    
    # Add audio tracks to BIN/CUE (following shared.mk pattern)
    add_custom_target(add_audio_to_bin_cue
        DEPENDS create_bin_cue
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/add_audio_tracks.sh" "${BUILD_BIN}" "${BUILD_CUE}" "${MUSIC_DIR}" "${CD_NAME}"
        COMMENT "Adding audio tracks to BIN/CUE"
    )
else()
    message(WARNING "iso2raw not found at ${ISO2RAW_EXECUTABLE}")
    message(WARNING "BIN/CUE generation will not be available")
    message(WARNING "Please run setup_compiler.bat to install iso2raw")
    
    add_custom_target(create_bin_cue
        DEPENDS create_iso
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "ERROR: iso2raw not found"
        COMMAND ${CMAKE_COMMAND} -E message "  Expected at: ${ISO2RAW_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND} -E message "  Please run: /saturn/SaturnRingLib/setup_compiler.bat"
        COMMAND false
    )
    add_custom_target(add_audio_to_bin_cue
        DEPENDS create_bin_cue
        COMMAND ${CMAKE_COMMAND} -E message "iso2raw required for audio track support"
    )
endif()

# Main build target - creates everything
add_custom_target(build_bin_cue ALL
    DEPENDS add_audio_to_bin_cue
    COMMENT "CD image generation complete"
)

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
if(SRL_USE_SGL_SOUND_DRIVER)
    message(STATUS "Frequency analysis: ${SRL_ENABLE_FREQ_ANALYSIS}")
endif()
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

# ============================================================================
# SH2 Collision Unit Tests (CMake + CTest integration)
# ============================================================================

set(COLLISION_TEST_CD_NAME "noiz2sa_collision_ut")
set(COLLISION_TESTS_DIR "${CMAKE_SOURCE_DIR}/Tests")
set(COLLISION_TEST_BUILD_DROP "${COLLISION_TESTS_DIR}/BuildDrop")
set(COLLISION_TEST_ASSETS_DIR "${COLLISION_TESTS_DIR}/cd/data")
set(COLLISION_TEST_ELF "${COLLISION_TEST_BUILD_DROP}/${COLLISION_TEST_CD_NAME}.elf")
set(COLLISION_TEST_ISO "${COLLISION_TEST_BUILD_DROP}/${COLLISION_TEST_CD_NAME}.iso")
set(COLLISION_TEST_BIN "${COLLISION_TEST_BUILD_DROP}/${COLLISION_TEST_CD_NAME}.bin")
set(COLLISION_TEST_CUE "${COLLISION_TEST_BUILD_DROP}/${COLLISION_TEST_CD_NAME}.cue")
set(COLLISION_TEST_MAP "${COLLISION_TEST_BUILD_DROP}/${COLLISION_TEST_CD_NAME}.map")

file(MAKE_DIRECTORY "${COLLISION_TEST_BUILD_DROP}")
file(MAKE_DIRECTORY "${COLLISION_TEST_ASSETS_DIR}")
file(WRITE "${COLLISION_TEST_ASSETS_DIR}/ABS.TXT" "NOT Abstracted by SEGA\n")
file(WRITE "${COLLISION_TEST_ASSETS_DIR}/BIB.TXT" "NOT Bibliographiced by SEGA\n")
file(WRITE "${COLLISION_TEST_ASSETS_DIR}/CPY.TXT" "")

set(COLLISION_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/Tests/src/main.cxx
    ${CMAKE_SOURCE_DIR}/src/vector.cpp
)

add_executable(${COLLISION_TEST_CD_NAME}.elf
    "${SGL_PRELOADER_SOURCE}"
    ${COLLISION_TEST_SOURCES}
    ${SOURCES_SGL_SYSTEM}
)

set_target_properties(${COLLISION_TEST_CD_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${COLLISION_TEST_BUILD_DROP}"
)

target_include_directories(
    ${COLLISION_TEST_CD_NAME}.elf PUBLIC
    ${SRL_INSTALL_ROOT}/saturnringlib
    ${SRL_INSTALL_ROOT}/modules/sgl/INC
    ${SRL_INSTALL_ROOT}/modules/dummy
    ${SRL_INSTALL_ROOT}/modules/SaturnMathPP
    ${SRL_INSTALL_ROOT}/modules/tlsf
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Tests/src
)

target_compile_options(${COLLISION_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
    $<$<COMPILE_LANGUAGE:C>:${c_flags}>
)

target_compile_definitions(${COLLISION_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:C>:bool=int>
    $<$<COMPILE_LANGUAGE:C>:true=1>
    $<$<COMPILE_LANGUAGE:C>:false=0>
)

target_link_options(${COLLISION_TEST_CD_NAME}.elf PUBLIC
    "SHELL:-m2"
    "SHELL:-L${SRL_SGL_DIR}/LIB"
    "SHELL:-Xlinker -T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-Xlinker -Map -Xlinker ${COLLISION_TEST_MAP}"
    "SHELL:-Xlinker -e -Xlinker ___Start -nostartfiles"
)

if(CMAKE_C_COMPILER_VERSION MATCHES "^14\\.")
    target_link_options(${COLLISION_TEST_CD_NAME}.elf PUBLIC "SHELL:-specs=nosys.specs")
endif()

srl_link_target(${COLLISION_TEST_CD_NAME}.elf)

add_custom_command(TARGET ${COLLISION_TEST_CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${COLLISION_TEST_CD_NAME}.elf>" "${COLLISION_TEST_ASSETS_DIR}/0.bin"
    COMMENT "Converting collision test ELF to binary: ${COLLISION_TEST_ASSETS_DIR}/0.bin"
    VERBATIM
)

if(XORRISOFS_EXECUTABLE)
    add_custom_target(${COLLISION_TEST_CD_NAME}_iso
        DEPENDS ${COLLISION_TEST_CD_NAME}.elf
        COMMAND ${XORRISOFS_EXECUTABLE}
            --norock -quiet
            -sysid "SEGA SATURN"
            -volid "${COLLISION_TEST_CD_NAME}"
            -volset "${COLLISION_TEST_CD_NAME}"
            -publisher "SEGA ENTERPRISES, LTD."
            -preparer "SEGA ENTERPRISES, LTD."
            -appid "${COLLISION_TEST_CD_NAME}"
            -abstract "NOT Abstracted by SEGA"
            -copyright ""
            -biblio ""
            -generic-boot "${IPFILE}"
            -full-iso9660-filenames
            -o "${COLLISION_TEST_ISO}"
            "${COLLISION_TEST_ASSETS_DIR}"
        COMMENT "Building collision test ISO: ${COLLISION_TEST_ISO}"
        VERBATIM
    )
endif()

if(EXISTS "${ISO2RAW_EXECUTABLE}")
    add_custom_target(${COLLISION_TEST_CD_NAME}_bin_cue
        DEPENDS ${COLLISION_TEST_CD_NAME}_iso
        COMMAND "${ISO2RAW_EXECUTABLE}" "${COLLISION_TEST_ISO}"
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/create_cue_header.sh" "${COLLISION_TEST_CUE}" "${COLLISION_TEST_CD_NAME}.bin"
        COMMENT "Creating collision test BIN/CUE"
        VERBATIM
    )
endif()

set(BACKGROUND_TEST_CD_NAME "noiz2sa_background_ut")
set(BACKGROUND_TESTS_DIR "${CMAKE_SOURCE_DIR}/Tests")
set(BACKGROUND_TEST_BUILD_DROP "${BACKGROUND_TESTS_DIR}/BuildDrop")
set(BACKGROUND_TEST_ASSETS_DIR "${BACKGROUND_TESTS_DIR}/cd_background/data")
set(BACKGROUND_TEST_ELF "${BACKGROUND_TEST_BUILD_DROP}/${BACKGROUND_TEST_CD_NAME}.elf")
set(BACKGROUND_TEST_ISO "${BACKGROUND_TEST_BUILD_DROP}/${BACKGROUND_TEST_CD_NAME}.iso")
set(BACKGROUND_TEST_BIN "${BACKGROUND_TEST_BUILD_DROP}/${BACKGROUND_TEST_CD_NAME}.bin")
set(BACKGROUND_TEST_CUE "${BACKGROUND_TEST_BUILD_DROP}/${BACKGROUND_TEST_CD_NAME}.cue")
set(BACKGROUND_TEST_MAP "${BACKGROUND_TEST_BUILD_DROP}/${BACKGROUND_TEST_CD_NAME}.map")

file(MAKE_DIRECTORY "${BACKGROUND_TEST_BUILD_DROP}")
file(MAKE_DIRECTORY "${BACKGROUND_TEST_ASSETS_DIR}")
file(WRITE "${BACKGROUND_TEST_ASSETS_DIR}/ABS.TXT" "NOT Abstracted by SEGA\n")
file(WRITE "${BACKGROUND_TEST_ASSETS_DIR}/BIB.TXT" "NOT Bibliographiced by SEGA\n")
file(WRITE "${BACKGROUND_TEST_ASSETS_DIR}/CPY.TXT" "")

set(BACKGROUND_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/Tests/src/background_main.cxx
    ${CMAKE_SOURCE_DIR}/src/background.cpp
    ${CMAKE_SOURCE_DIR}/src/clrtbl.cpp
)

add_executable(${BACKGROUND_TEST_CD_NAME}.elf
    "${SGL_PRELOADER_SOURCE}"
    ${BACKGROUND_TEST_SOURCES}
    ${SOURCES_SGL_SYSTEM}
)

set_target_properties(${BACKGROUND_TEST_CD_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BACKGROUND_TEST_BUILD_DROP}"
)

target_include_directories(
    ${BACKGROUND_TEST_CD_NAME}.elf PUBLIC
    ${SRL_INSTALL_ROOT}/saturnringlib
    ${CMAKE_SOURCE_DIR}/src/SDL
    ${SRL_INSTALL_ROOT}/modules/sgl/INC
    ${SRL_INSTALL_ROOT}/modules/dummy
    ${SRL_INSTALL_ROOT}/modules/SaturnMathPP
    ${SRL_INSTALL_ROOT}/modules/tlsf
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Tests/src
)

target_compile_options(${BACKGROUND_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
    $<$<COMPILE_LANGUAGE:C>:${c_flags}>
)

target_compile_definitions(${BACKGROUND_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:C>:bool=int>
    $<$<COMPILE_LANGUAGE:C>:true=1>
    $<$<COMPILE_LANGUAGE:C>:false=0>
)

target_link_options(${BACKGROUND_TEST_CD_NAME}.elf PUBLIC
    "SHELL:-m2"
    "SHELL:-L${SRL_SGL_DIR}/LIB"
    "SHELL:-Xlinker -T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-Xlinker -Map -Xlinker ${BACKGROUND_TEST_MAP}"
    "SHELL:-Xlinker -e -Xlinker ___Start -nostartfiles"
)

if(CMAKE_C_COMPILER_VERSION MATCHES "^14\\.")
    target_link_options(${BACKGROUND_TEST_CD_NAME}.elf PUBLIC "SHELL:-specs=nosys.specs")
endif()

srl_link_target(${BACKGROUND_TEST_CD_NAME}.elf)

add_custom_command(TARGET ${BACKGROUND_TEST_CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${BACKGROUND_TEST_CD_NAME}.elf>" "${BACKGROUND_TEST_ASSETS_DIR}/0.bin"
    COMMENT "Converting background test ELF to binary: ${BACKGROUND_TEST_ASSETS_DIR}/0.bin"
    VERBATIM
)

if(XORRISOFS_EXECUTABLE)
    add_custom_target(${BACKGROUND_TEST_CD_NAME}_iso
        DEPENDS ${BACKGROUND_TEST_CD_NAME}.elf
        COMMAND ${XORRISOFS_EXECUTABLE}
            --norock -quiet
            -sysid "SEGA SATURN"
            -volid "${BACKGROUND_TEST_CD_NAME}"
            -volset "${BACKGROUND_TEST_CD_NAME}"
            -publisher "SEGA ENTERPRISES, LTD."
            -preparer "SEGA ENTERPRISES, LTD."
            -appid "${BACKGROUND_TEST_CD_NAME}"
            -abstract "NOT Abstracted by SEGA"
            -copyright ""
            -biblio ""
            -generic-boot "${IPFILE}"
            -full-iso9660-filenames
            -o "${BACKGROUND_TEST_ISO}"
            "${BACKGROUND_TEST_ASSETS_DIR}"
        COMMENT "Building background test ISO: ${BACKGROUND_TEST_ISO}"
        VERBATIM
    )
endif()

if(EXISTS "${ISO2RAW_EXECUTABLE}")
    add_custom_target(${BACKGROUND_TEST_CD_NAME}_bin_cue
        DEPENDS ${BACKGROUND_TEST_CD_NAME}_iso
        COMMAND "${ISO2RAW_EXECUTABLE}" "${BACKGROUND_TEST_ISO}"
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/create_cue_header.sh" "${BACKGROUND_TEST_CUE}" "${BACKGROUND_TEST_CD_NAME}.bin"
        COMMENT "Creating background test BIN/CUE"
        VERBATIM
    )
endif()

set(BULLETML_SH2_TEST_CD_NAME "noiz2sa_bulletml_ut")
set(BULLETML_SH2_TESTS_DIR "${CMAKE_SOURCE_DIR}/Tests")
set(BULLETML_SH2_TEST_BUILD_DROP "${BULLETML_SH2_TESTS_DIR}/BuildDrop")
set(BULLETML_SH2_TEST_ASSETS_DIR "${BULLETML_SH2_TESTS_DIR}/cd_bulletml/data")
set(BULLETML_SH2_TEST_ELF "${BULLETML_SH2_TEST_BUILD_DROP}/${BULLETML_SH2_TEST_CD_NAME}.elf")
set(BULLETML_SH2_TEST_ISO "${BULLETML_SH2_TEST_BUILD_DROP}/${BULLETML_SH2_TEST_CD_NAME}.iso")
set(BULLETML_SH2_TEST_BIN "${BULLETML_SH2_TEST_BUILD_DROP}/${BULLETML_SH2_TEST_CD_NAME}.bin")
set(BULLETML_SH2_TEST_CUE "${BULLETML_SH2_TEST_BUILD_DROP}/${BULLETML_SH2_TEST_CD_NAME}.cue")
set(BULLETML_SH2_TEST_MAP "${BULLETML_SH2_TEST_BUILD_DROP}/${BULLETML_SH2_TEST_CD_NAME}.map")

file(MAKE_DIRECTORY "${BULLETML_SH2_TEST_BUILD_DROP}")
file(MAKE_DIRECTORY "${BULLETML_SH2_TEST_ASSETS_DIR}")
file(WRITE "${BULLETML_SH2_TEST_ASSETS_DIR}/ABS.TXT" "NOT Abstracted by SEGA\n")
file(WRITE "${BULLETML_SH2_TEST_ASSETS_DIR}/BIB.TXT" "NOT Bibliographiced by SEGA\n")
file(WRITE "${BULLETML_SH2_TEST_ASSETS_DIR}/CPY.TXT" "")

set(BULLETML_SH2_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/Tests/src/bulletml_main.cxx
)

add_executable(${BULLETML_SH2_TEST_CD_NAME}.elf
    "${SGL_PRELOADER_SOURCE}"
    ${BULLETML_SH2_TEST_SOURCES}
    ${SOURCES_SGL_SYSTEM}
)

set_target_properties(${BULLETML_SH2_TEST_CD_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BULLETML_SH2_TEST_BUILD_DROP}"
)

target_include_directories(
    ${BULLETML_SH2_TEST_CD_NAME}.elf PUBLIC
    ${SRL_INSTALL_ROOT}/saturnringlib
    ${CMAKE_SOURCE_DIR}/src/bulletml_binary
    ${CMAKE_SOURCE_DIR}/src/SDL
    ${SRL_INSTALL_ROOT}/modules/sgl/INC
    ${SRL_INSTALL_ROOT}/modules/dummy
    ${SRL_INSTALL_ROOT}/modules/SaturnMathPP
    ${SRL_INSTALL_ROOT}/modules/tlsf
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Tests/src
)

target_compile_options(${BULLETML_SH2_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
    $<$<COMPILE_LANGUAGE:C>:${c_flags}>
)

target_compile_definitions(${BULLETML_SH2_TEST_CD_NAME}.elf PUBLIC
    $<$<COMPILE_LANGUAGE:C>:bool=int>
    $<$<COMPILE_LANGUAGE:C>:true=1>
    $<$<COMPILE_LANGUAGE:C>:false=0>
)

target_link_options(${BULLETML_SH2_TEST_CD_NAME}.elf PUBLIC
    "SHELL:-m2"
    "SHELL:-L${SRL_SGL_DIR}/LIB"
    "SHELL:-Xlinker -T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-Xlinker -Map -Xlinker ${BULLETML_SH2_TEST_MAP}"
    "SHELL:-Xlinker -e -Xlinker ___Start -nostartfiles"
)

if(CMAKE_C_COMPILER_VERSION MATCHES "^14\\.")
    target_link_options(${BULLETML_SH2_TEST_CD_NAME}.elf PUBLIC "SHELL:-specs=nosys.specs")
endif()

srl_link_target(${BULLETML_SH2_TEST_CD_NAME}.elf)

add_custom_command(TARGET ${BULLETML_SH2_TEST_CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${BULLETML_SH2_TEST_CD_NAME}.elf>" "${BULLETML_SH2_TEST_ASSETS_DIR}/0.bin"
    COMMENT "Converting bulletml test ELF to binary: ${BULLETML_SH2_TEST_ASSETS_DIR}/0.bin"
    VERBATIM
)

if(XORRISOFS_EXECUTABLE)
    add_custom_target(${BULLETML_SH2_TEST_CD_NAME}_iso
        DEPENDS ${BULLETML_SH2_TEST_CD_NAME}.elf
        COMMAND ${XORRISOFS_EXECUTABLE}
            --norock -quiet
            -sysid "SEGA SATURN"
            -volid "${BULLETML_SH2_TEST_CD_NAME}"
            -volset "${BULLETML_SH2_TEST_CD_NAME}"
            -publisher "SEGA ENTERPRISES, LTD."
            -preparer "SEGA ENTERPRISES, LTD."
            -appid "${BULLETML_SH2_TEST_CD_NAME}"
            -abstract "NOT Abstracted by SEGA"
            -copyright ""
            -biblio ""
            -generic-boot "${IPFILE}"
            -full-iso9660-filenames
            -o "${BULLETML_SH2_TEST_ISO}"
            "${BULLETML_SH2_TEST_ASSETS_DIR}"
        COMMENT "Building bulletml test ISO: ${BULLETML_SH2_TEST_ISO}"
        VERBATIM
    )
endif()

if(EXISTS "${ISO2RAW_EXECUTABLE}")
    add_custom_target(${BULLETML_SH2_TEST_CD_NAME}_bin_cue
        DEPENDS ${BULLETML_SH2_TEST_CD_NAME}_iso
        COMMAND "${ISO2RAW_EXECUTABLE}" "${BULLETML_SH2_TEST_ISO}"
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/create_cue_header.sh" "${BULLETML_SH2_TEST_CUE}" "${BULLETML_SH2_TEST_CD_NAME}.bin"
        COMMENT "Creating bulletml test BIN/CUE"
        VERBATIM
    )
endif()

enable_testing()

# ============================================================================
# BulletML Parity Test (compiled with native compiler, not Saturn toolchain)
# ============================================================================

set(BULLETML_TEST_NAME "bulletml_parity_ut")
set(BULLETML_TEST_SOURCE "${CMAKE_SOURCE_DIR}/Tests/bulletml/bulletml_parity_tests.cpp")
set(BULLETML_TEST_EXECUTABLE "${CMAKE_BINARY_DIR}/Tests/${BULLETML_TEST_NAME}")

# Build bulletml parity test with native compiler (not Saturn SH-ELF)
add_custom_command(
    OUTPUT "${BULLETML_TEST_EXECUTABLE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/Tests"
    COMMAND g++ -std=c++11 -Wall -Wextra -o "${BULLETML_TEST_EXECUTABLE}" "${BULLETML_TEST_SOURCE}"
    DEPENDS "${BULLETML_TEST_SOURCE}"
    COMMENT "Building bulletml parity test with native compiler"
    VERBATIM
)

add_custom_target(bulletml_parity_ut_build ALL
    DEPENDS "${BULLETML_TEST_EXECUTABLE}"
)

# ============================================================================
# CTest Registration
# ============================================================================

add_test(
    NAME sh2_collision_campaign
    COMMAND bash "${CMAKE_SOURCE_DIR}/Tests/test_campaign.sh" --emulator mednafen --strict
)
set_tests_properties(sh2_collision_campaign PROPERTIES TIMEOUT 700)

add_test(
    NAME sh2_background_animation_campaign
    COMMAND bash "${CMAKE_SOURCE_DIR}/Tests/test_background_campaign.sh" --emulator mednafen --strict
)
set_tests_properties(sh2_background_animation_campaign PROPERTIES TIMEOUT 700)

add_test(
    NAME bulletml_parity_test
    COMMAND "${BULLETML_TEST_EXECUTABLE}"
)
set_tests_properties(bulletml_parity_test PROPERTIES TIMEOUT 60)

add_test(
    NAME bulletml_all_xml_recursive_test
    COMMAND python3 "${CMAKE_SOURCE_DIR}/Tests/bulletml/test_all_xml_recursive.py"
)
set_tests_properties(bulletml_all_xml_recursive_test PROPERTIES TIMEOUT 120)
