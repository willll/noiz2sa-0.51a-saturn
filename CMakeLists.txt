cmake_minimum_required(VERSION 3.0)

SET(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

set(BULLETML_INSTALL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/bulletml/")
set(BULLETML_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/src/bulletml/lib/bulletml.a")

set(SEGALIB $ENV{SATURN_SBL}/segalib)
set(SEGASGL $ENV{SATURN_SGL})

set(SATURN_COMMON $ENV{SATURN_COMMON})
set(SATURN_CMAKE $ENV{SATURN_CMAKE})

set(TARGET_NAME noiz2sa)
set(TARGET_PATH noiz2sa)

project( ${TARGET_NAME} VERSION 1.0
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
        -maccumulate-outgoing-args -fomit-frame-pointer
        -fno-rtti
        -fno-exceptions
        -Wdeprecated-declarations   # FIXME!
         )

set(SOURCES

  SDL/src/SDL.c
  SDL/src/SDL_error.c
  SDL/src/SDL_fatal.c
  SDL/src/file/SDL_rwops.c
  SDL/src/video/SDL_surface.c
  SDL/src/video/SDL_video.c
  SDL/src/video/SDL_pixels.c
  SDL/src/video/SDL_bmp.c
  SDL/src/video/SDL_cursor.c
  SDL/src/video/SDL_RLEaccel.c
  SDL/src/video/SDL_blit.c
  SDL/src/video/SDL_blit_0.c
  SDL/src/video/SDL_blit_1.c
  SDL/src/video/SDL_blit_N.c
  SDL/src/video/SDL_blit_A.c
  SDL/src/timer/SDL_timer.c
  SDL/src/timer/saturn/SDL_systimer.c
  SDL/src/audio/SDL_audio.c
  SDL/src/audio/SDL_wave.c
  SDL/src/audio/SDL_mixer.c
  SDL/src/audio/SDL_audiomem.c
  SDL/src/audio/SDL_audiocvt.c
  SDL/src/thread/SDL_thread.c
  SDL/src/thread/saturn/SDL_systhread.c
  SDL/src/thread/saturn/SDL_syscond.c
  SDL/src/thread/saturn/SDL_sysmutex.c
  SDL/src/thread/saturn/SDL_syssem.c
  SDL/src/events/SDL_events.c
  SDL/src/events/SDL_keyboard.c
  SDL/src/events/SDL_mouse.c
  SDL/src/events/SDL_active.c
  SDL/src/events/SDL_quit.c
  SDL/src/events/SDL_resize.c
  SDL/src/endian/SDL_endian.c
  SDL/src/joystick/SDL_joystick.c
  SDL/src/joystick/saturn/SDL_sysjoystick.c

  SDL/SDL_mixer/music.c
  SDL/SDL_mixer/effects_internal.c
  SDL/SDL_mixer/mixer.c
  SDL/SDL_mixer/load_aiff.c

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
                        DISABLE_AUDIO
                        DISABLE_VIDEO
                        DISABLE_JOYSTICK
                        DISABLE_CDROM
                        DISABLE_TIMERS
                        DISABLE_THREADS
                        DISABLE_EVENTS
                        DISABLE_ENDIAN
                        DISABLE_FILE
                        NO_SIGNAL_H
                        )

add_executable( ${TARGET_NAME}.elf ${SOURCES} )

target_include_directories(
    ${TARGET_NAME}.elf PUBLIC
    .
    src
    sdl
    SDL/include
    SDL/SDL_mixer
    ${BULLETML_INCLUDE_DIRS}
    #${SEGALIB}/include
    )

target_compile_options(${TARGET_NAME}.elf PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>
                                                 $<$<COMPILE_LANGUAGE:C>:${c_flags}> )

target_link_options(${TARGET_NAME}.elf PUBLIC
    "SHELL:-Xlinker -Map -Xlinker ${TARGET_NAME}.map"
    "SHELL:-Xlinker --strip-debug"
    "SHELL:-Xlinker -fno-lto"
    "SHELL:-m2 -nostartfiles"
    "SHELL:-T${SATURN_COMMON}/Saturn.lnk"
    "SHELL:-lc -lgcc -lm"
)

target_link_libraries (${TARGET_NAME}.elf PUBLIC ${BULLETML_LIBRARIES})

#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGASGL}/lib/libsgl.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_scl.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_mth.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_spr.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_cdc.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_per.a )
#target_link_libraries(${TARGET_NAME}.elf PUBLIC ${SEGALIB}/lib/sega_int.a )
