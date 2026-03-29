/*
 * $Id: soundmanager.c,v 1.4 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager(using SDL_mixer for SFX and CDDA for music).
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include <srl_memory.hpp>  // for malloc/free
#include <srl_log.hpp>     // for logging
#include <srl_string.hpp>  // for string functions
#include <srl_cd.hpp>      // for CD file access
#include <srl_sound.hpp>   // for CDDA support

#include "soundmanager.h"

static int useAudio = 0;

#define MUSIC_NUM 9

// Music track mapping
// Track numbering: Track 1 = data, Tracks 2+ = audio tracks from tracklist
// stg0.ogg = Track 2, stg1.ogg = Track 3, ..., stg00.ogg = Track 8
const uint8_t musicTrackMap[MUSIC_NUM] = {
  2,  // stg0 -> Track 2
  3,  // stg1 -> Track 3
  4,  // stg2 -> Track 4
  5,  // stg3 -> Track 5
  6,  // stg4 -> Track 6
  7,  // stg5 -> Track 7
  8,  // stg00 -> Track 8
  9,  // Hydraulic_Maw (Loading) -> Track 9
  10  // Avert_Collision (Unused) -> Track 10
};

#define CHUNK_NUM 7

static const char *chunkFileName[CHUNK_NUM] = {
  "SHOT.WAV", "HIT.WAV", "FOEDES.WAV", "BOSDES.WAV", "SHIPDS.WAV", "BONUS.WAV", "EXTEND.WAV",
};
static SRL::Sound::Pcm::WaveSound *chunk[CHUNK_NUM];

void closeSound() {
  SRL::Logger::LogDebug("[SOUND] closeSound() called");
  int i;
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] closeSound: useAudio=0, skipping");
    return;
  }
  // Stop CDDA playback
  SRL::Logger::LogDebug("[SOUND] closeSound: Stopping CDDA");
  SRL::Sound::Cdda::StopPause();
  // Clean up sound effects
  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    if ( chunk[i] ) {
      SRL::Logger::LogDebug("[SOUND] closeSound: Deleting chunk[%d]", i);
      delete chunk[i];
    }
    chunk[i] = nullptr;
  }
  useAudio = 0;
  SRL::Logger::LogDebug("[SOUND] closeSound: Audio closed");
}


// Initialize the sound.

void loadSounds() {
  SRL::Logger::LogDebug("[SOUND] loadSounds() called");
  int i;
  char name[56];
  int loaded = 0;

  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");

  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    chunk[i] = nullptr;
    strcpy(name, chunkFileName[i]);
    SRL::Logger::LogDebug("[SOUND] loadSounds: Loading %s", name);
    chunk[i] = lwnew SRL::Sound::Pcm::WaveSound(name);
    if ( chunk[i] == nullptr ) {
      SRL::Logger::LogWarning("[SOUND] Allocation failed for: %s", name);
      continue;
    }
    loaded++;
  }

  SRL::Cd::ChangeDir((char *)nullptr);

  if (loaded <= 0) {
    SRL::Logger::LogWarning("[SOUND] No PCM chunks loaded; disabling audio");
    useAudio = 0;
  } else {
    SRL::Logger::LogInfo("[SOUND] Loaded %d/%d PCM chunks", loaded, CHUNK_NUM);
  }
}

void initSound() {
#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] initSound() called (NOIZ2SA_ENABLE_SOUND=0)");
  useAudio = 0;
  SRL::Logger::LogInfo("[SOUND] Disabled at compile time (NOIZ2SA_ENABLE_SOUND=0)");
  return;
#else
  SRL::Logger::LogDebug("[SOUND] initSound() called");
  for (int i = 0; i < CHUNK_NUM; i++) chunk[i] = nullptr;

//   // Ensure we are in root before loading optional SGL sound driver files.
//   SRL::Cd::ChangeDir((char *)nullptr);

// #if defined(SRL_USE_SGL_SOUND_DRIVER) && SRL_USE_SGL_SOUND_DRIVER == 1
//   SRL::Logger::LogInfo("[SOUND] Initializing SGL sound driver");
//   SRL::Sound::Hardware::Initialize();
// #endif

  useAudio = 1;
#endif
}

// Play/Stop the music/chunk.

void playMusic(int idx) {
  SRL::Logger::LogDebug("[SOUND] playMusic(%d) called", idx);
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] playMusic: useAudio=0, skipping");
    return;
  }
  if (idx >= 0 && idx < MUSIC_NUM) {
    SRL::Logger::LogDebug("[SOUND] playMusic: Playing CDDA track %d", musicTrackMap[idx]);
    SRL::Sound::Cdda::PlaySingle(musicTrackMap[idx], true);
    SRL::Logger::LogInfo("[SOUND] Playing music: Track %d", musicTrackMap[idx]);
  } else {
    SRL::Logger::LogWarning("[SOUND] playMusic: Invalid index %d", idx);
  }
}

void fadeMusic() {
  SRL::Logger::LogDebug("[SOUND] fadeMusic() called");
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] fadeMusic: useAudio=0, skipping");
    return;
  }
  // CDDA doesn't support fade, so just stop
  SRL::Logger::LogDebug("[SOUND] fadeMusic: Stopping CDDA");
  SRL::Sound::Cdda::StopPause();
  SRL::Logger::LogInfo("[SOUND] Fading music (CDDA doesn't support fade, so stopped)");
}

void stopMusic() {
  SRL::Logger::LogDebug("[SOUND] stopMusic() called");
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] stopMusic: useAudio=0, skipping");
    return;
  }
  SRL::Logger::LogDebug("[SOUND] stopMusic: Stopping CDDA");
  SRL::Sound::Cdda::StopPause();
  SRL::Logger::LogInfo("[SOUND] Stopped music");
}

void playChunk(int idx) {
  SRL::Logger::LogDebug("[SOUND] playChunk(%d) called", idx);
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] playChunk: useAudio=0, skipping");
    return;
  }
  if ( idx < 0 || idx >= CHUNK_NUM ) {
    SRL::Logger::LogWarning("[SOUND] playChunk: Invalid index %d", idx);
    return;
  }
  if ( chunk[idx] == nullptr ) {
    SRL::Logger::LogWarning("[SOUND] playChunk: chunk[%d] is nullptr", idx);
    return;
  }

  // 4 PCM channels are available, let SRL pick the first free one.
  SRL::Logger::LogDebug("[SOUND] playChunk: Playing %s", chunkFileName[idx]);
  SRL::Sound::Pcm::Play(*chunk[idx], 127, 0);
  SRL::Logger::LogInfo("[SOUND] Playing chunk: %s", chunkFileName[idx]);

}
