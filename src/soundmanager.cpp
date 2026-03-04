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

#include "SDL_mixer.h"
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
static Mix_Chunk *chunk[CHUNK_NUM];
static int chunkFlag[CHUNK_NUM];

void closeSound() {
  int i;
  if ( !useAudio ) return;
  // Stop CDDA playback
  SRL::Sound::Cdda::StopPause();
  // Clean up sound effects
  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    if ( chunk[i] ) {
      Mix_FreeChunk(chunk[i]);
    }
  }
  Mix_CloseAudio();
}


// Initialize the sound.

static void loadSounds() {
  int i;
  char name[56];

  // Change to SOUNDS directory on CD
  SRL::Cd::ChangeDir("SOUNDS");

  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    strcpy(name, chunkFileName[i]);
    if ( nullptr == (chunk[i] = Mix_LoadWAV(name)) ) {
      SRL::Logger::LogWarning("Couldn't load: %s", name);
      useAudio = 0;
      return;
    }
    chunkFlag[i] = 0;
  }
}

void initSound() {
  int audio_rate;
  Uint16 audio_format;
  int audio_channels;
  int audio_buffers;
  int interactive = 0;

  if ( SDL_InitSubSystem(SDL_INIT_AUDIO) < 0 ) {
    SRL::Logger::LogWarning("Unable to initialize SDL_AUDIO: %s", SDL_GetError());
    return;
  }

  audio_rate = 44100;
  audio_format = AUDIO_S16;
  audio_channels = 1;
  audio_buffers = 4096;

  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) < 0) {
    SRL::Logger::LogWarning("Couldn't open audio: %s", SDL_GetError());
    return;
  } else {
    Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
  }

  useAudio = 1;
  loadSounds();
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
  Mix_PlayChannel(idx, chunk[idx], 0);
}
