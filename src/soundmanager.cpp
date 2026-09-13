/*
 * $Id: soundmanager.c,v 1.4 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager.
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include <srl_cd.hpp>
#include <srl_log.hpp>
#include <srl_system.hpp>

#if NOIZ2SA_ENABLE_SOUND == 1
#    include <ponesound.hpp>
#endif

#include "sound_factory.h"
#include "soundmanager.h"

static int useAudio = 0;
static int currentMusicIdx = -1;

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

#if NOIZ2SA_ENABLE_SOUND == 1
static int16_t chunk[CHUNK_NUM];
static bool ponesoundDriverInitialized = false;
static bool chunkInvalidWarned[CHUNK_NUM];
static bool chunkReloadAttempted[CHUNK_NUM];

/** @brief Loads a PCM chunk into Ponesound, retrying until the driver is ready. */
static int16_t loadPonesoundChunkWithRetry(const uint8_t* data, int32_t size)
{
  int16_t id = -1;
  // -7 means M68K-side pcmCtrl is not ready yet; tick and retry a few times.
  for (int attempt = 0; attempt < 64; ++attempt)
  {
    id = SRL::Ponesound::Pcm::LoadPcmFromMemory(data, size, SRL::Ponesound::BitDepth::PCM16, 32000);
    if (id != -7)
    {
      return id;
    }

    for (int i = 0; i < 16; ++i)
    {
      SRL::Ponesound::Sound::Driver::Tick();
    }
  }
  return id;
}

/** @brief Loads a PCM chunk from disc and registers it with Ponesound. */
static int16_t loadPonesoundChunkFromCd(int idx)
{
  char name[32];
  snprintf(name, sizeof(name), "%s.PCM", chunkName[idx]);

  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");
  SRL::Cd::File file(name);
  if (!file.Open())
  {
    SRL::Cd::ChangeDir((char *)nullptr);
    return -1;
  }

  const int32_t size = file.Size.Bytes;
  uint8_t* raw = createArray<uint8_t>(size);
  const int32_t readBytes = file.LoadBytes(0, size, raw);
  file.Close();
  SRL::Cd::ChangeDir((char *)nullptr);

  if (readBytes != size)
  {
    destroyArray(raw);
    return -2;
  }

  const int16_t id = loadPonesoundChunkWithRetry(raw, size);
  destroyArray(raw);
  return id;
}
#endif

/** @brief Shuts down the audio backend and releases sound resources. */
void closeSound() {
  SRL::Logger::LogDebug("[SOUND] closeSound() called");
  if (!useAudio) {
    SRL::Logger::LogDebug("[SOUND] closeSound: useAudio=0, skipping");
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
  SRL::Logger::LogDebug("[SOUND] closeSound: Stopping CDDA (Ponesound)");
  SRL::Ponesound::Sound::Driver::SetTickEnabled(false);
  SRL::Ponesound::CD::Stop();
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
    chunkReloadAttempted[i] = false;
  }
  SRL::Ponesound::Pcm::Unload(-1);
#endif

  useAudio = 0;
  currentMusicIdx = -1;
  SRL::Logger::LogDebug("[SOUND] closeSound: Audio closed");
}

/** @brief Loads BGM and SFX assets from disc. */
void loadSounds() {
  SRL::Logger::LogDebug("[SOUND] loadSounds() called");
  int loaded = 0;

#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] loadSounds: sound disabled at compile time");
  return;
#else
  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("SOUNDS");

  SRL::Logger::LogDebug("[CDDA] stop backend=Ponesound");
  int availableChunks = 0;

  // Phase 1: only validate PCM assets at startup.
  // Actual PCM registration is deferred until first use to avoid M68K
  // pcmCtrl readiness races during boot.
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
    chunkReloadAttempted[i] = false;
    char name[32];
    snprintf(name, sizeof(name), "%s.PCM", chunkName[i]);
    SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Loading %s", name);

    SRL::Cd::File file(name);
    if (!file.Open()) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Failed to open %s", name);
      continue;
    }

    const int32_t rawSize = file.Size.Bytes;
    file.Close();

    if (rawSize <= 0) {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): Invalid size for %s (%d)", name, rawSize);
      continue;
    }

    availableChunks++;
    SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): Found %s size=%d", name, rawSize);
  }

  SRL::Cd::ChangeDir((char *)nullptr);

  // Phase 2: start Ponesound driver once.
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

  // Phase 3: load all PCM chunks upfront now that the driver is ready.
  // Loading here (during the loading screen) avoids deferred CD reads inside
  // the game loop, which seek the drive head away from the CDDA audio track,
  // silencing music and causing visible frame stalls on real hardware.
  int loadedNow = 0;
  for (int i = 0; i < CHUNK_NUM; i++) {
    if (chunk[i] >= 0) continue;  // already registered
    const int16_t id = loadPonesoundChunkFromCd(i);
    if (id >= 0) {
      chunk[i] = id;
      chunkReloadAttempted[i] = true;
      loadedNow++;
      SRL::Logger::LogDebug("[SOUND] loadSounds (Ponesound): PCM chunk[%d]=%s loaded id=%d", i, chunkName[i], id);
    } else {
      SRL::Logger::LogWarning("[SOUND] loadSounds (Ponesound): PCM chunk[%d]=%s failed id=%d", i, chunkName[i], (int)id);
    }
  }
  loaded = loadedNow;
  SRL::Logger::LogInfo("[SOUND] loadSounds (Ponesound): loaded %d/%d PCM chunks upfront", loadedNow, CHUNK_NUM);

#endif

  SRL::Cd::ChangeDir((char *)nullptr);

  if (loaded <= 0) {
    SRL::Logger::LogWarning("[SOUND] No SFX chunks loaded");
  } else {
    SRL::Logger::LogDebug("[SOUND] Loaded %d/%d SFX chunks", loaded, CHUNK_NUM);
  }
}

/** @brief Initialises the sound backend selection and runtime state. */
void initSound() {
#if NOIZ2SA_ENABLE_SOUND == 0
  SRL::Logger::LogDebug("[SOUND] initSound() called (NOIZ2SA_ENABLE_SOUND=0)");
  useAudio = 0;
  SRL::Logger::LogDebug("[SOUND] Disabled at compile time");
  return;
#else
  SRL::Logger::LogDebug("[SOUND] initSound() called");

#  if NOIZ2SA_ENABLE_PCM_SFX == 0
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
    chunkReloadAttempted[i] = false;
  }
  SRL::Logger::LogWarning("[SOUND] Ponesound driver init skipped (NOIZ2SA_ENABLE_PCM_SFX=0)");
  ponesoundDriverInitialized = false;
  useAudio = 0;
  return;
#  else
  for (int i = 0; i < CHUNK_NUM; i++) {
    chunk[i] = -1;
    chunkInvalidWarned[i] = false;
    chunkReloadAttempted[i] = false;
  }
  // Driver init is deferred to loadSounds() so PCM files are read from CD first.
  ponesoundDriverInitialized = false;
  SRL::Logger::LogDebug("[SOUND] Ponesound driver init deferred until loadSounds()");
  useAudio = 1;
#  endif
#endif
}

/** @brief Starts playback of the requested music track. */
void playMusic(int idx) {
  if (!useAudio) {
    return;
  }
  if (idx < 0 || idx >= MUSIC_NUM) {
    SRL::Logger::LogWarning("[SOUND] playMusic: Invalid index %d", idx);
    return;
  }

  const uint8_t track = musicTrackMap[idx];
  SRL::Logger::LogInfo("[CDDA] playMusic idx=%d track=%u", idx, (unsigned int)track);

#if NOIZ2SA_ENABLE_SOUND == 1
  SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
  SRL::Ponesound::CD::SetVolume(7);
  SRL::Ponesound::CD::SetPan(0x1F, 0x0F);
  SRL::Logger::LogInfo("[CDDA] backend=Ponesound stop before play track=%u", (unsigned int)track);
  SRL::Ponesound::CD::Stop();

  // Give the sound driver a few ticks to process the stop command before re-playing.
  for (int i = 0; i < 4; ++i) {
    SRL::Ponesound::Sound::Driver::Tick();
  }

  SRL::Logger::LogInfo("[CDDA] backend=Ponesound play track=%u loop=1", (unsigned int)track);
  SRL::Ponesound::CD::PlaySingle(track, true);
#endif
  currentMusicIdx = idx;
}

void preloadChunksNow() {
  if (!useAudio) {
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
  if (!ponesoundDriverInitialized) {
    return;
  }

  int loadedNow = 0;
  for (int i = 0; i < CHUNK_NUM; i++) {
    if (chunk[i] >= 0) {
      continue;
    }

    const int16_t reloadedId = loadPonesoundChunkFromCd(i);
    if (reloadedId >= 0) {
      chunk[i] = reloadedId;
      chunkInvalidWarned[i] = false;
      chunkReloadAttempted[i] = true;
      loadedNow++;
    } else {
      chunk[i] = reloadedId;
      if (reloadedId == -7) {
        chunkReloadAttempted[i] = false;
      }
      SRL::Logger::LogWarning("[SOUND] preloadChunksNow: chunk[%d] load failed (id=%d)", i, reloadedId);
    }
  }

  SRL::Logger::LogInfo("[SOUND] preloadChunksNow: loaded=%d/%d", loadedNow, CHUNK_NUM);
#endif
}

/** @brief Fades out the currently playing music track. */
void fadeMusic() {
  if (!useAudio) {
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
  SRL::Logger::LogInfo("[CDDA] fadeMusic backend=Ponesound stop");
  SRL::Ponesound::CD::Stop();
#endif
  currentMusicIdx = -1;
}

/** @brief Stops the currently playing music track immediately. */
void stopMusic() {
  if (!useAudio) {
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
  SRL::Logger::LogInfo("[CDDA] stopMusic backend=Ponesound stop");
  SRL::Ponesound::CD::Stop();
#endif
  currentMusicIdx = -1;
}

/** @brief Plays a sound effect chunk. */
void playChunk(int idx) {
  if (!useAudio) {
    return;
  }
  if (idx < 0 || idx >= CHUNK_NUM) {
    SRL::Logger::LogWarning("[SOUND] playChunk: Invalid index %d", idx);
    return;
  }

#if NOIZ2SA_ENABLE_SOUND == 1
  if (chunk[idx] < 0) {
    // Retry transient M68K readiness failures (-7) on subsequent calls.
    // Non-transient failures keep one-shot behaviour to avoid CD thrashing.
    if (!chunkReloadAttempted[idx] || chunk[idx] == -7) {
      chunkReloadAttempted[idx] = true;
      const int16_t reloadedId = loadPonesoundChunkFromCd(idx);
      if (reloadedId >= 0) {
        chunk[idx] = reloadedId;
        chunkInvalidWarned[idx] = false;
      } else {
        chunk[idx] = reloadedId;
        if (reloadedId == -7) {
          chunkReloadAttempted[idx] = false;
        }
      }
    }

    if (chunk[idx] >= 0) {
      SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
      SRL::Ponesound::Pcm::Play(chunk[idx], SRL::Ponesound::PlayMode::Volatile, 7);
      return;
    }

    if (!chunkInvalidWarned[idx]) {
      SRL::Logger::LogWarning("[SOUND] playChunk: chunk[%d] invalid (id=%d)", idx, chunk[idx]);
      chunkInvalidWarned[idx] = true;
    }
    return;
  }
  SRL::Ponesound::Sound::Driver::SetTickEnabled(true);
  SRL::Logger::LogDebug("[SOUND] playChunk idx=%d id=%d", idx, chunk[idx]);
  SRL::Ponesound::Pcm::Play(chunk[idx], SRL::Ponesound::PlayMode::Volatile, 7);
#endif
}

/** @brief Advances sound driver state once per frame. */
void soundTick() {
#if NOIZ2SA_ENABLE_SOUND == 1
  if (useAudio && ponesoundDriverInitialized) {
    SRL::Ponesound::Sound::Driver::Tick();
  }
#endif
}
