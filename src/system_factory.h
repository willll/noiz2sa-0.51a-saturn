#pragma once

#include <impl/random.hpp>
#include <srl_input.hpp>

#include "memory_factory.h"

inline SRL::Math::Random<unsigned int>* createRandomGenerator(uint32_t seed)
{
  return createObject<SRL::Math::Random<unsigned int>>(seed);
}

inline SRL::Input::Digital* createDigitalGamepad(int port)
{
  return createObject<SRL::Input::Digital>(port);
}
