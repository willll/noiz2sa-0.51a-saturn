cmake_minimum_required(VERSION 3.0)

SET(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

set(SEGALIB $ENV{SATURN_SBL}/segalib)
set(SEGASGL $ENV{SATURN_SGL})

set(SATURN_COMMON $ENV{SATURN_COMMON})
set(SATURN_CMAKE $ENV{SATURN_CMAKE})

set(SDL_LIB ${CMAKE_SOURCE_DIR}/SDL)
set(SDL_MIXER_LIB ${CMAKE_SOURCE_DIR}/SDL_mixer)


add_subdirectory(src/bulletml)
add_subdirectory(${SDL_LIB})
add_subdirectory(${SDL_MIXER_LIB})

project( noiz2sa VERSION 1.0
                  DESCRIPTION "Sega Saturn noiz2sa"
                  LANGUAGES C CXX ASM)

set(as_flags )
set(cxx_flags
        -O2 -g3 -m2
        -fpermissive
        -Wno-conversion-null
        -fno-lto -Wshadow -Wall -Wswitch -Wpadded
        -fno-common -Wextra -Wno-narrowing -fno-web
        -fno-builtin -funit-at-a-time -Wwrite-strings
        -Wl,--strip-all -Wl,--verbose -mno-fsrra
        -Wno-padded
        -maccumulate-outgoing-args
        -fomit-frame-pointer
        -fno-rtti
        -fno-exceptions
        -Wdeprecated-declarations   # FIXME!
         )

set(SOURCES_NOIZ2SA

  ${SATURN_COMMON}/crt0.s

  src/noiz2sa.c
  src/degutil.c
  src/vector.c
  src/screen.c
  src/shot.c
  src/ship.c
  src/bonus.c
  src/frag.c
  src/clrtbl.c
  src/letterrender.c
  src/gamepad.c
  src/rand.c
  src/attractmanager.c
  src/soundmanager.c
  src/background.c
  src/foe.cc
  src/foecommand.cc
  src/barragemanager.cc

  )

set(CD_FILES
      0.bin
    )

add_compile_definitions(MODEL_S
                        _SH
                        )

add_executable( noiz2sa.elf ${SOURCES_NOIZ2SA} )

target_include_directories(
    noiz2sa.elf PUBLIC
    .
    src
    sdl
    SDL/include
    SDL_mixer
    ${BULLETML_INCLUDE_DIRS}
    #${SEGALIB}/include
    )

target_compile_options(noiz2sa.elf PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
                                                 $<$<COMPILE_LANGUAGE:C>:${c_flags}> )

target_link_options(noiz2sa.elf PUBLIC
    "SHELL:-Xlinker -Map -Xlinker noiz2sa.map"
    "SHELL:-Xlinker --strip-debug"
    "SHELL:-Xlinker -fno-lto"
    "SHELL:-m2 -nostartfiles"
    "SHELL:-T${SATURN_COMMON}/Saturn.lnk"
    "SHELL:-lc -lgcc -lm"
)

target_link_libraries (noiz2sa.elf PUBLIC bulletml sdl_mixer sdl )

#target_link_libraries(noiz2sa.elf PUBLIC ${SEGASGL}/lib/libsgl.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_scl.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_mth.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_spr.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_cdc.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_per.a )
#target_link_libraries(noiz2sa.elf PUBLIC ${SEGALIB}/lib/sega_int.a )
