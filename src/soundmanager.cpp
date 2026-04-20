/*
 * $Id: soundmanager.c,v 1.4 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager.
 *
 * Backend behaviour:
 *   SRL_USE_SGL_SOUND_DRIVER=1  -> SGL driver for CDDA + WAV SFX
 *   SRL_USE_SGL_SOUND_DRIVER=0  -> Ponesound driver for CDDA + optional PCM SFX
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

#if NOIZ2SA_ENABLE_SOUND == 1 && SRL_USE_SGL_SOUND_DRIVER == 1
static SRL::Sound::Pcm::WaveSound* sglChunks[CHUNK_NUM];
static bool sglChunkExists[CHUNK_NUM];
static char sglChunkFileName[CHUNK_NUM][32];
static bool sglStartupSelfTestPlayed = false;
#else
static int16_t chunk[CHUNK_NUM];
static bool ponesoundDriverInitialized = false;
static bool chunkInvalidWarned[CHUNK_NUM];

static int16_t loadPonesoundChunkWithRetry(const uint8_t* data, int32_t size)
{
  int16_t id = -1;
  // -7 means M68K-side pcmCtrl is not ready yet; tick and retry a few times.
  for (int attempt = 0; attempt < 8; ++attempt)
  {
    id = SRL::Ponesound::Pcm::LoadPcmFromMemory(data, size, SRL::Ponesound::BitDepth::PCM16, 32000);
    if (id != -7)
    {
      return id;
    }

    for (int i = 0; i < 8; ++i)
    {
      SRL::Ponesound::Sound::Driver::Tick();
    }
  }
  return id;
}
#endif

void closeSound() {
  SRL::Logger::LogDebug("[SOUND] closeSound() called");
  if (!useAudio) {
    SRL::Logger::LogDebug("[SOUND] closeSound: useAudio=0, skipping");
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Logger::LogDebug("[CDDA] closeSound: stop request (backend=SGL)");
  SRL::Sound::Cdda::StopPause();
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunkFileName[i][0] = '\0';
    if (sglChunks[i] != nullptr) {
      delete sglChunks[i];
      sglChunks[i] = nullptr;
    }
  }
#  else
  SRL::Logger::LogDebug("[SOUND] closeSound: Stopping CDDA (Ponesound)");
  SRL::Ponesound::Sound::Driver::SetTickEnabled(false);
  SRL::Ponesound::CD::Stop();
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
  }
  SRL::Ponesound::Pcm::Unload(-1);
#  endif
#endif

  useAudio = 0;
  SRL::Logger::LogDebug("[SOUND] closeSound: Audio closed");
}

void loadSounds() {
  SRL::Logger::LogDebug("[SOUND] loadSounds() called");

#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] loadSounds: sound disabled at compile time");
  return;
#else
  int loaded = 0;

  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");

#  if SRL_USE_SGL_SOUND_DRIVER == 1
  // SGL backend validates WAV loading at initialization but does not keep all
  // samples resident to avoid startup RAM pressure.
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunks[i] = nullptr;
    sglChunkExists[i] = false;
    sglChunkFileName[i][0] = '\0';

    char normalizedName[32];
    char fallbackName[32];
    snprintf(normalizedName, sizeof(normalizedName), "%s.SGL.WAV", chunkName[i]);
    snprintf(fallbackName, sizeof(fallbackName), "%s.WAV", chunkName[i]);

    const char* selectedName = nullptr;
    SRL::Cd::File normalized(normalizedName);
    if (normalized.Exists()) {
      selectedName = normalizedName;
    } else {
      SRL::Cd::File fallback(fallbackName);
      if (fallback.Exists()) selectedName = fallbackName;
    }

    if (selectedName == nullptr) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (SGL): %s and %s not found on disc", normalizedName, fallbackName);
      continue;
    }

    snprintf(sglChunkFileName[i], sizeof(sglChunkFileName[i]), "%s", selectedName);
    SRL::Logger::LogDebug("[SOUND] loadSounds (SGL): Loading %s", sglChunkFileName[i]);

    SRL::Sound::Pcm::WaveSound* preload = new SRL::Sound::Pcm::WaveSound(sglChunkFileName[i]);
    delete preload;
    sglChunkExists[i] = true;
    SRL::Logger::LogDebug("[SOUND] loadSounds (SGL): Loaded WAV %s during init (released)", sglChunkFileName[i]);
    loaded++;
  }
#  else
#    if NOIZ2SA_ENABLE_PCM_SFX == 0
  SRL::Logger::LogWarning("[SOUND] Ponesound PCM SFX disabled (NOIZ2SA_ENABLE_PCM_SFX=0)");
  for (int i = 0; i < CHUNK_NUM; i++) chunk[i] = -1;
#    else
  SRL::Logger::LogDebug("[CDDA] stop backend=Ponesound");
  uint8_t* rawBuffers[CHUNK_NUM] = {nullptr};
  int32_t rawSizes[CHUNK_NUM] = {0};

  // Phase 1: read PCM files from CD before starting Ponesound driver.
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
    char name[32];
    snprintf(name, sizeof(name), "%s.PCM", chunkName[i]);
    SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Loading %s", name);

    SRL::Cd::File file(name);
    if (!file.Open()) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Failed to open %s", name);
      continue;
    }

    rawSizes[i] = file.Size.Bytes;
    rawBuffers[i] = new uint8_t[rawSizes[i]];
    const int32_t loadedBytes = file.LoadBytes(0, rawSizes[i], rawBuffers[i]);
    file.Close();

    if (loadedBytes != rawSizes[i]) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Failed to read %s (%d/%d)", name, loadedBytes, rawSizes[i]);
      delete[] rawBuffers[i];
      rawBuffers[i] = nullptr;
      rawSizes[i] = 0;
      continue;
    }

    SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Preloaded %s bytes=%d", name, rawSizes[i]);
  }

  SRL::Cd::ChangeDir((char *)nullptr);

  // Phase 2: start Ponesound driver once, then register preloaded PCM buffers.
  if (!ponesoundDriverInitialized) {
    SRL::Logger::LogDebug("[SOUND] Initializing Ponesound driver (deferred)");
    SRL::Ponesound::Sound::Driver::Initialize(SRL::Ponesound::ADXMode::ADX2304);
    SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
    SRL::Ponesound::CD::SetVolume(7);
    // Set CDDA stereo pan to centre (0x0F = centre in the SCSP 5-bit DIPAN scale).
    // The lower 5 bits of the vol/pan byte were 0 (from SNDRAM clear); 0 = full-left,
    // which would collapse both channels to the left speaker on real hardware.
    // pan 0x1F = full-left for left CD channel (slot 16); 0x0F = full-right for right CD channel (slot 17)
    // SCSP slot reg +0x16 EffectVolume: bits[7:5]=level, bits[4:0]=pan
    // pan=0x1F: pan_which=1 + panv=0 => outvol[LEFT]=basev, outvol[RIGHT]=0 (left only)
    // pan=0x0F: pan_which=0 + panv=0 => outvol[LEFT]=0,    outvol[RIGHT]=basev (right only)
    SRL::Ponesound::CD::SetPan(0x1F, 0x0F);
    SRL::Logger::LogDebug("[SOUND] Ponesound CDDA vol=7 pan=centre(0x0F)");
    ponesoundDriverInitialized = true;
  }

  for (int i = 0; i < CHUNK_NUM; i++) {
    char name[32];
    snprintf(name, sizeof(name), "%s.PCM", chunkName[i]);

    if (rawBuffers[i] == nullptr || rawSizes[i] <= 0) {
      chunk[i] = -1;
      continue;
    }

    chunk[i] = loadPonesoundChunkWithRetry(rawBuffers[i], rawSizes[i]);
    delete[] rawBuffers[i];
    rawBuffers[i] = nullptr;

    if (chunk[i] < 0) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Failed to register %s (code=%d)", name, chunk[i]);
    } else {
      SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Loaded %s id=%d", name, chunk[i]);
      loaded++;
    }
  }
#    endif
#  endif

  SRL::Cd::ChangeDir((char *)nullptr);

  if (loaded <= 0) {
    SRL::Logger::LogWarning("[SOUND] No SFX chunks loaded");
  } else {
    SRL::Logger::LogDebug("[SOUND] Loaded %d/%d SFX chunks", loaded, CHUNK_NUM);

#  if SRL_USE_SGL_SOUND_DRIVER == 1
    if (!sglStartupSelfTestPlayed && useAudio) {
      sglStartupSelfTestPlayed = true;
      SRL::Logger::LogDebug("[SOUND] SGL startup self-test: playing HIT");
      playChunk(1); // HIT
    }
#  endif
  }
#endif
}

void initSound() {
#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] initSound() called (NOIZ2SA_ENABLE_SOUND=0)");
  useAudio = 0;
  SRL::Logger::LogDebug("[SOUND] Disabled at compile time");
  return;
#else
  SRL::Logger::LogDebug("[SOUND] initSound() called");

#  if SRL_USE_SGL_SOUND_DRIVER == 1
  for (int i = 0; i < CHUNK_NUM; i++) {
    sglChunks[i] = nullptr;
    sglChunkExists[i] = false;
    sglChunkFileName[i][0] = '\0';
  }
  SRL::Logger::LogDebug("[SOUND] SGL sound driver already initialised by Core::Initialize");
  SRL::Sound::Cdda::SetVolume(7);
  SRL::Logger::LogDebug("[SOUND] SGL CDDA volume set");
  useAudio = 1;
#  elif NOIZ2SA_ENABLE_PCM_SFX == 0
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
  }
  SRL::Logger::LogWarning("[SOUND] Ponesound driver init skipped (NOIZ2SA_ENABLE_PCM_SFX=0)");
  ponesoundDriverInitialized = false;
  useAudio = 0;
  return;
#  else
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
  }
  // Driver init is deferred to loadSounds() so PCM files are read from CD first.
  ponesoundDriverInitialized = false;
  SRL::Logger::LogDebug("[SOUND] Ponesound driver init deferred until loadSounds()");
  useAudio = 1;
#  endif
#endif
}

void playMusic(int idx) {
  if (!useAudio) {
    return;
  }
  if (idx < 0 || idx >= MUSIC_NUM) {
    SRL::Logger::LogWarning("[SOUND] playMusic: Invalid index %d", idx);
    return;
  }

  const uint8_t track = musicTrackMap[idx];

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::PlaySingle(track, true);
#  else
  SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
  SRL::Ponesound::CD::PlaySingle(track, true);
#  endif
#endif
}

void fadeMusic() {
  if (!useAudio) {
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::StopPause();
#  else
  SRL::Ponesound::CD::Stop();
#  endif
#endif
}

void stopMusic() {
  if (!useAudio) {
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 1
  SRL::Sound::Cdda::StopPause();
#  else
  SRL::Ponesound::CD::Stop();
#  endif
#endif
}

void playChunk(int idx) {
  if (!useAudio) {
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
    // Runtime reload happens from root dir; switch into SOUNDS for WaveSound file open.
    SRL::Cd::ChangeDir((char *)nullptr);
    SRL::Cd::ChangeDir("SOUNDS");
    sglChunks[idx] = new SRL::Sound::Pcm::WaveSound(sglChunkFileName[idx]);
    SRL::Cd::ChangeDir((char *)nullptr);
  }

  if (sglChunks[idx]->Play(127, 0) < 0) {
    SRL::Logger::LogWarning("[SOUND] playChunk (SGL): No free PCM channel for %s.WAV, forcing channel 0", chunkName[idx]);

    // If all channels are occupied, steal channel 0 to guarantee audible SFX.
    SRL::Sound::Pcm::StopSound(0);
    if (!SRL::Sound::Pcm::PlayOnChannel(*sglChunks[idx], 0, 127, 0)) {
      SRL::Logger::LogWarning("[SOUND] playChunk (SGL): Failed to start %s.WAV on forced channel", chunkName[idx]);
      return;
    }
  }
#  else
  if (chunk[idx] < 0) {
    if (!chunkInvalidWarned[idx]) {
      SRL::Logger::LogWarning("[SOUND] playChunk: chunk[%d] invalid (id=%d)", idx, chunk[idx]);
      chunkInvalidWarned[idx] = true;
    }
    return;
  }
  SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
  SRL::Ponesound::Pcm::Play(chunk[idx], SRL::Ponesound::PlayMode::Volatile, 7);
#  endif
#endif
}

void soundTick() {
#if NOIZ2SA_ENABLE_SOUND == 1
#  if SRL_USE_SGL_SOUND_DRIVER == 0
  if (useAudio && ponesoundDriverInitialized) {
    SRL::Ponesound::Sound::Driver::Tick();
  }
#  endif
#endif
}
