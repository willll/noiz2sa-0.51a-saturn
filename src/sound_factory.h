#pragma once

#include <srl_sound.hpp>

#include "memory_factory.h"

inline SRL::Sound::Pcm::WaveSound* createWaveSound(const char* fileName)
{
  return createObject<SRL::Sound::Pcm::WaveSound>(fileName);
}
