# SaturnRingLib CMake integration for noiz2sa
#
# Usage:
#   include(${CMAKE_SOURCE_DIR}/SRL.cmake)
#   srl_link_target(<your-target>)
#
# Provided target:
#   SRL::Core (INTERFACE IMPORTED)

include_guard(GLOBAL)

option(SRL_REQUIRED "Fail configure if SaturnRingLib is missing" ON)
option(SRL_USE_SGL_SOUND_DRIVER "Link LIBSND.A from SGL" OFF)

set(SRL_ROOT "${CMAKE_SOURCE_DIR}/SaturnRingLib" CACHE PATH "Path to SaturnRingLib root")
set(SRL_MODULES_DIR "${SRL_ROOT}/modules" CACHE PATH "Path to SaturnRingLib modules")
set(SRL_SGL_DIR "${SRL_MODULES_DIR}/sgl" CACHE PATH "Path to SGL module")
set(SRL_SGL_INCLUDE_DIR "${SRL_SGL_DIR}/INC" CACHE PATH "Path to SGL include dir")
set(SRL_SGL_LIB_DIR "${SRL_SGL_DIR}/LIB" CACHE PATH "Path to SGL static libs")

set(_srl_missing FALSE)
foreach(_p
    "${SRL_ROOT}"
    "${SRL_ROOT}/saturnringlib"
    "${SRL_SGL_INCLUDE_DIR}"
    "${SRL_SGL_LIB_DIR}")
    if(NOT EXISTS "${_p}")
        set(_srl_missing TRUE)
        message(STATUS "SRL path missing: ${_p}")
    endif()
endforeach()

if(_srl_missing)
    if(SRL_REQUIRED)
        message(FATAL_ERROR
            "SaturnRingLib was not found.\n"
            "Expected at: ${SRL_ROOT}\n"
            "Initialize submodules and/or set -DSRL_ROOT=<path>.")
    else()
        message(WARNING "SaturnRingLib not found; SRL::Core will not be created.")
        return()
    endif()
endif()

set(SRL_LIBS
    "${SRL_SGL_LIB_DIR}/LIBCPK.A"
    "${SRL_SGL_LIB_DIR}/SEGA_SYS.A"
    "${SRL_SGL_LIB_DIR}/LIBCD.A"
    "${SRL_SGL_LIB_DIR}/LIBSGL.A"
)

if(SRL_USE_SGL_SOUND_DRIVER)
    list(APPEND SRL_LIBS "${SRL_SGL_LIB_DIR}/LIBSND.A")
endif()

foreach(_lib IN LISTS SRL_LIBS)
    if(NOT EXISTS "${_lib}")
        if(SRL_REQUIRED)
            message(FATAL_ERROR "Required SRL library not found: ${_lib}")
        else()
            message(WARNING "SRL library not found: ${_lib}")
        endif()
    endif()
endforeach()

if(NOT TARGET SRL::Core)
    add_library(SRL::Core INTERFACE IMPORTED GLOBAL)

    set_property(TARGET SRL::Core PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES
            "${SRL_ROOT}/saturnringlib;${SRL_SGL_INCLUDE_DIR};${SRL_MODULES_DIR}/dummy;${SRL_MODULES_DIR}/SaturnMathPP;${SRL_ROOT}")

    set_property(TARGET SRL::Core PROPERTY
        INTERFACE_LINK_LIBRARIES
            "${SRL_LIBS}")
endif()

function(srl_link_target target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "srl_link_target: target does not exist: ${target_name}")
    endif()

    if(NOT TARGET SRL::Core)
        message(FATAL_ERROR "srl_link_target: SRL::Core is unavailable")
    endif()

    target_link_libraries("${target_name}" PUBLIC SRL::Core)
endfunction()
