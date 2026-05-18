#pragma once

#include <srl_sound.hpp>

#include "memory_factory.h"

inline SRL::Sound::Pcm::WaveSound* createWaveSound(const char* fileName)
{
  return createPooledObject<SRL::Sound::Pcm::WaveSound>(fileName);
}

inline void destroyWaveSound(SRL::Sound::Pcm::WaveSound*& sound)
{
  destroyPooledObject(sound);
}

inline void releaseSoundFactoryPools()
{
  releasePooledObjectPool<SRL::Sound::Pcm::WaveSound>();
}
