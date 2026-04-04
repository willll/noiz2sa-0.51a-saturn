/*
 * $Id: soundmanager.c,v 1.4 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager.
 *
 * Supports two sound driver backends selected at compile time:
 *   SRL_USE_SGL_SOUND_DRIVER=1  -> SGL driver (SDDRVS.TSK/BOOTSND.MAP) for CDDA + SGL PCM for SFX
 *   SRL_USE_SGL_SOUND_DRIVER=0  -> Ponesound driver for CDDA + PCM SFX
 *
 * PCM SFX loading can be independently toggled:
 *   NOIZ2SA_ENABLE_PCM_SFX=1   -> load and play PCM sound effects
 *   NOIZ2SA_ENABLE_PCM_SFX=0   -> skip PCM SFX (CDDA BGM still works with SGL driver path)
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include <srl_cd.hpp>
#include <srl_log.hpp>
#include <srl_system.hpp>

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
#    include <srl_sound.hpp>
#  else
#    include <ponesound.hpp>
#  endif
#endif

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

static const char *chunkName[CHUNK_NUM] = {
  "SHOT", "HIT", "FOEDES", "BOSDES", "SHIPDS", "BONUS", "EXTEND",
};

// ---- Per-driver chunk storage ----
#if NOIZ2SA_ENABLE_SOUND == 1 && SRL_USE_SGL_SOUND_DRIVER == 1
// SGL backend: heap-allocated WaveSound objects, one per SFX slot
static SRL::Sound::Pcm::WaveSound* sglChunks[CHUNK_NUM];
static bool sglChunkExists[CHUNK_NUM];
#else
// Ponesound backend: integer IDs returned by Pcm::Load8
static int16_t chunk[CHUNK_NUM];
#endif


// ---- closeSound ----------------------------------------------------------------

void closeSound() {
  SRL::Logger::LogDebug("[SOUND] closeSound() called");
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] closeSound: useAudio=0, skipping");
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  // Stop CDDA playback via SGL
  SRL::Logger::LogDebug("[SOUND] closeSound: Stopping CDDA (SGL)");
  SRL::Sound::Cdda::StopPause();
  // Free loaded PCM SFX objects
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunkExists[i] = false;
    if (sglChunks[i] != nullptr) {
      delete sglChunks[i];
      sglChunks[i] = nullptr;
    }
  }
#  else
  // Stop CDDA playback via Ponesound
  SRL::Logger::LogDebug("[SOUND] closeSound: Stopping CDDA (Ponesound)");
  SRL::Ponesound::CD::Stop();
  // Reset chunk IDs and unload all Ponesound PCM
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
  }
  SRL::Ponesound::Pcm::Unload(-1);
#  endif
#endif

  useAudio = 0;
  SRL::Logger::LogDebug("[SOUND] closeSound: Audio closed");
}


// ---- loadSounds ----------------------------------------------------------------

void loadSounds() {
  SRL::Logger::LogDebug("[SOUND] loadSounds() called");

#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] loadSounds: sound disabled at compile time");
  return;

#elif NOIZ2SA_ENABLE_PCM_SFX == 0
  // PCM SFX disabled: nothing to load here.
  // CDDA playback is still available (SGL driver path only).
  SRL::Logger::LogWarning("[SOUND] PCM SFX loading disabled (NOIZ2SA_ENABLE_PCM_SFX=0)");
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunks[i] = nullptr;
    sglChunkExists[i] = false;
  }
#  else
  for (int i = 0; i < CHUNK_NUM; i++) chunk[i] = -1;
#  endif
  return;

#else  // NOIZ2SA_ENABLE_SOUND==1 && NOIZ2SA_ENABLE_PCM_SFX==1

  int loaded = 0;

  // Navigate into the SOUNDS directory on disc
  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");

#  if SRL_USE_SGL_SOUND_DRIVER == 1
  // ---- SGL PCM path ----
  // SGL path uses lazy WAV loading to reduce init-time memory pressure.
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunks[i] = nullptr;
    sglChunkExists[i] = false;
    char name[32];
    snprintf(name, sizeof(name), "%s.WAV", chunkName[i]);
    SRL::Logger::LogDebug("[SOUND] loadSounds (SGL): Loading %s", name);
    SRL::Cd::File file(name);
    if (!file.Exists()) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (SGL): %s not found on disc", name);
      continue;
    }
    sglChunkExists[i] = true;
    SRL::Logger::LogDebug("[SOUND] loadSounds (SGL): Found WAV %s (deferred)", name);
    loaded++;
  }
#  else
  // ---- Ponesound PCM path ----
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    char name[32];
    snprintf(name, sizeof(name), "%s.PCM", chunkName[i]);
    SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Loading %s", name);
    chunk[i] = SRL::Ponesound::Pcm::Load8(name, 15360);
    if (chunk[i] < 0) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Failed to load %s (code=%d)", name, chunk[i]);
    } else {
      SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Loaded %s id=%d", name, chunk[i]);
      loaded++;
    }
  }
#  endif

  SRL::Cd::ChangeDir((char *)nullptr);

  if (loaded <= 0) {
    SRL::Logger::LogWarning("[SOUND] No PCM chunks loaded; SFX unavailable");
  } else {
    SRL::Logger::LogInfo("[SOUND] Loaded %d/%d PCM chunks", loaded, CHUNK_NUM);
  }
#endif // NOIZ2SA_ENABLE_PCM_SFX
}


// ---- initSound -----------------------------------------------------------------

void initSound() {
#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] initSound() called (NOIZ2SA_ENABLE_SOUND=0)");
  useAudio = 0;
  SRL::Logger::LogInfo("[SOUND] Disabled at compile time");
  return;

#else
  SRL::Logger::LogDebug("[SOUND] initSound() called");

  // Zero out chunk storage before any driver init
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunks[i] = nullptr;
    sglChunkExists[i] = false;
  }
#  else
  for (int i = 0; i < CHUNK_NUM; i++) chunk[i] = -1;
#  endif

#  if SRL_USE_SGL_SOUND_DRIVER == 1
  // ---- SGL driver path ----
  // SRL::Core::Initialize() already called Hardware::Initialize() via srl_core.hpp
  // (guard: #if SRL_USE_SGL_SOUND_DRIVER == 1 in Core::Initialize).
  // We only need to set CDDA volume here.
  SRL::Logger::LogInfo("[SOUND] SGL sound driver already initialised by Core::Initialize");
  SRL::Sound::Cdda::SetVolume(7);
  SRL::Logger::LogInfo("[SOUND] SGL CDDA volume set");
  useAudio = 1;

#  elif NOIZ2SA_ENABLE_PCM_SFX == 0
  // Ponesound driver with PCM SFX disabled.
  // Skip driver initialisation entirely to avoid M68K/SCSP bus conflicts —
  // the Ponesound M68K program interacts with SCSP registers in ways that
  // destabilise later bus accesses when no PCM is ever registered.
  SRL::Logger::LogWarning("[SOUND] Ponesound driver init skipped (NOIZ2SA_ENABLE_PCM_SFX=0)");
  useAudio = 0;
  return;

#  else
  // ---- Ponesound driver init (PCM SFX enabled) ----
  SRL::Logger::LogInfo("[SOUND] Initializing Ponesound driver");
  SRL::Ponesound::Sound::Driver::Initialize(SRL::Ponesound::ADXMode::ADX2304);
  SRL::Ponesound::CD::SetVolume(7);
  SRL::Logger::LogInfo("[SOUND] Ponesound driver ready");
  useAudio = 1;
#  endif

#endif // NOIZ2SA_ENABLE_SOUND
}


// ---- playMusic -----------------------------------------------------------------

void playMusic(int idx) {
  SRL::Logger::LogDebug("[SOUND] playMusic(%d) called", idx);
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] playMusic: useAudio=0, skipping");
    return;
  }
  if (idx < 0 || idx >= MUSIC_NUM) {
    SRL::Logger::LogWarning("[SOUND] playMusic: Invalid index %d", idx);
    return;
  }
  uint8_t track = musicTrackMap[idx];
  SRL::Logger::LogDebug("[SOUND] playMusic: Playing CDDA track %d", track);

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::PlaySingle(track, true);
#  else
  SRL::Ponesound::CD::PlaySingle(track, true);
#  endif
#endif

  SRL::Logger::LogInfo("[SOUND] Playing music: Track %d", track);
}


// ---- fadeMusic -----------------------------------------------------------------

void fadeMusic() {
  SRL::Logger::LogDebug("[SOUND] fadeMusic() called");
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] fadeMusic: useAudio=0, skipping");
    return;
  }
  // Neither SGL CDDA nor Ponesound supports fade-out; just stop.
  SRL::Logger::LogDebug("[SOUND] fadeMusic: stopping CDDA (no fade support)");

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::StopPause();
#  else
  SRL::Ponesound::CD::Stop();
#  endif
#endif

  SRL::Logger::LogInfo("[SOUND] Fading music (stopped)");
}


// ---- stopMusic -----------------------------------------------------------------

void stopMusic() {
  SRL::Logger::LogDebug("[SOUND] stopMusic() called");
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] stopMusic: useAudio=0, skipping");
    return;
  }
  SRL::Logger::LogDebug("[SOUND] stopMusic: Stopping CDDA");

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::StopPause();
#  else
  SRL::Ponesound::CD::Stop();
#  endif
#endif

  SRL::Logger::LogInfo("[SOUND] Stopped music");
}


// ---- playChunk -----------------------------------------------------------------

void playChunk(int idx) {
  SRL::Logger::LogDebug("[SOUND] playChunk(%d) called", idx);
  if ( !useAudio ) {
    SRL::Logger::LogDebug("[SOUND] playChunk: useAudio=0, skipping");
    return;
  }
  if (idx < 0 || idx >= CHUNK_NUM) {
    SRL::Logger::LogWarning("[SOUND] playChunk: Invalid index %d", idx);
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1

#  if SRL_USE_SGL_SOUND_DRIVER == 1
  if (!sglChunkExists[idx]) {
    SRL::Logger::LogWarning("[SOUND] playChunk: %s.WAV missing", chunkName[idx]);
    return;
  }
  if (sglChunks[idx] == nullptr) {
    char name[32];
    snprintf(name, sizeof(name), "%s.WAV", chunkName[idx]);
    sglChunks[idx] = new SRL::Sound::Pcm::WaveSound(name);
    SRL::Logger::LogDebug("[SOUND] playChunk (SGL): Lazy-loaded %s", name);
  }
  SRL::Logger::LogDebug("[SOUND] playChunk (SGL): Playing %s.WAV", chunkName[idx]);
  // Play at full volume (127/127) with no panning
  sglChunks[idx]->Play(127, 0);

#  else
  if (chunk[idx] < 0) {
    SRL::Logger::LogWarning("[SOUND] playChunk: chunk[%d] invalid (id=%d)", idx, chunk[idx]);
    return;
  }
  SRL::Logger::LogDebug("[SOUND] playChunk (Ponesound): Playing %s.PCM id=%d", chunkName[idx], chunk[idx]);
  SRL::Ponesound::Pcm::Play(chunk[idx], SRL::Ponesound::PlayMode::Volatile, 7);
#  endif

  SRL::Logger::LogInfo("[SOUND] Playing chunk: %s", chunkName[idx]);
#endif // NOIZ2SA_ENABLE_SOUND
}
