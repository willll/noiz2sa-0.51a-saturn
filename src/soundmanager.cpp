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

#define MUSIC_NUM 7

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
  8   // stg00 -> Track 8
};

#define CHUNK_NUM 7

static const char *chunkFileName[CHUNK_NUM] = {
  "SHOT.WAV", "HIT.WAV", "FOEDES.WAV", "BOSDES.WAV", "SHIPDS.WAV", "BONUS.WAV", "EXTEND.WAV",
};
static SRL::Sound::Pcm::WaveSound *chunk[CHUNK_NUM];

void closeSound() {
  int i;
  if ( !useAudio ) return;
  // Stop CDDA playback
  SRL::Sound::Cdda::StopPause();
  // Clean up sound effects
  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    if ( chunk[i] ) delete chunk[i];
    chunk[i] = nullptr;
  }
  useAudio = 0;
}


// Initialize the sound.

static void loadSounds() {
  int i;
  char name[56];
  int loaded = 0;

  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");

  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    chunk[i] = nullptr;
    strcpy(name, chunkFileName[i]);
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
  useAudio = 0;
  SRL::Logger::LogInfo("[SOUND] Disabled at compile time (NOIZ2SA_ENABLE_SOUND=0)");
  return;
#else
  for (int i = 0; i < CHUNK_NUM; i++) chunk[i] = nullptr;

  // Ensure we are in root before loading optional SGL sound driver files.
  SRL::Cd::ChangeDir((char *)nullptr);

#if defined(SRL_USE_SGL_SOUND_DRIVER) && SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Logger::LogInfo("[SOUND] Initializing SGL sound driver");
  SRL::Sound::Hardware::Initialize();
#endif

  useAudio = 1;
  loadSounds();
#endif
}

// Play/Stop the music/chunk.

void playMusic(int idx) {
  if ( !useAudio ) return;
  if (idx >= 0 && idx < MUSIC_NUM) {
    SRL::Sound::Cdda::PlaySingle(musicTrackMap[idx], true);
  }
}

void fadeMusic() {
  if ( !useAudio ) return;
  // CDDA doesn't support fade, so just stop
  SRL::Sound::Cdda::StopPause();
}

void stopMusic() {
  if ( !useAudio ) return;
  SRL::Sound::Cdda::StopPause();
}

void playChunk(int idx) {
  if ( !useAudio ) return;
  if ( idx < 0 || idx >= CHUNK_NUM ) return;
  if ( chunk[idx] == nullptr ) return;

  // 4 PCM channels are available, let SRL pick the first free one.
  SRL::Sound::Pcm::Play(*chunk[idx], 127, 0);
}
