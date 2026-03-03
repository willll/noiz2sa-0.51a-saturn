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
# Note: TLSF source is added to SOURCES_SGL_SYSTEM list above conditionally

# ============================================================================

set(as_flags )
set(cxx_flags
    -std=c++23
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
  src/syscalls.c
  )

# SGL Preloader - MUST be linked first (before all other object files)
# Following shared.mk: PRELOADERSOURCES = $(SGLDIR)/SRC/preloader.cxx
set(SGL_PRELOADER_SOURCE "${SRL_SGL_DIR}/SRC/preloader.cxx")
if(NOT EXISTS "${SGL_PRELOADER_SOURCE}")
    message(WARNING "SGL preloader.cxx not found at: ${SGL_PRELOADER_SOURCE}")
endif()

# System sources (following shared.mk SYSSOURCES pattern)
# These are added after user sources to match linking order
set(SOURCES_SGL_SYSTEM)
set(SGL_WORKAREA_SOURCE "${SRL_SGL_DIR}/SRC/workarea.c")
if(EXISTS "${SGL_WORKAREA_SOURCE}")
    list(APPEND SOURCES_SGL_SYSTEM "${SGL_WORKAREA_SOURCE}")
else()
    message(WARNING "SGL workarea.c not found at: ${SGL_WORKAREA_SOURCE}")
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

# Locate IP.BIN (system boot sector)
if(SRL_IPBIN)
    set(IPFILE "${SRL_IPBIN}")
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/cd/data/IP.BIN")
        set(IPFILE "${CMAKE_SOURCE_DIR}/cd/data/IP.BIN")
    elseif(EXISTS "${SRL_SGL_DIR}/IP.BIN")
        set(IPFILE "${SRL_SGL_DIR}/IP.BIN")
    elseif(EXISTS "${SRL_INSTALL_ROOT}/resources/IP.BIN")
        set(IPFILE "${SRL_INSTALL_ROOT}/resources/IP.BIN")
    else()
        set(IPFILE "${SRL_SGL_DIR}/../../resources/IP.BIN")
    endif()
endif()

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

# Clean BuildDrop before linking
add_custom_command(TARGET ${CD_NAME}.elf PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BUILD_DROP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_DROP}"
    COMMENT "Cleaning BuildDrop directory"
)

# Set output directory to BUILD_DROP and map file path
set_target_properties(${CD_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BUILD_DROP}"
)

target_include_directories(
    ${CD_NAME}.elf PUBLIC
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
    "SHELL:-Xlinker -Map -Xlinker ${BUILD_MAP}"
    "SHELL:-Xlinker --strip-debug"
    "SHELL:-Xlinker -fno-lto"
    "SHELL:-m2 -nostartfiles"
    "SHELL:-T${SRL_SGL_DIR}/sgl.linker"
    "SHELL:-lc -lgcc -lm"
)

# Apply custom linker flags from SRL configuration
if(NOT SRL_CUSTOM_LDFLAGS STREQUAL "")
    target_link_options(${CD_NAME}.elf PUBLIC "SHELL:${SRL_CUSTOM_LDFLAGS}")
endif()

target_link_libraries (${CD_NAME}.elf PUBLIC bulletml_binary )
srl_link_target(${CD_NAME}.elf)

# ============================================================================
# Post-Build Steps: Asset Directories, Binary Conversion, Sound Driver
# ============================================================================

# Create BUILD_DROP and asset directories at configure time
file(MAKE_DIRECTORY "${BUILD_DROP}")
file(MAKE_DIRECTORY "${ASSETS_DIR}")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cd/music")

# Create asset directories at build time (in case they're cleaned)
add_custom_command(TARGET ${CD_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/cd/music"
    COMMENT "Ensuring asset directories exist"
)

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
# CD Image Generation Targets (following shared.mk build_bin_cue pattern)
# ============================================================================

# Create ISO using xorrisofs
if(XORRISOFS_EXECUTABLE)
    add_custom_target(create_iso
        DEPENDS ${CD_NAME}.elf
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
