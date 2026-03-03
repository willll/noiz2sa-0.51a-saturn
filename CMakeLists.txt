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
