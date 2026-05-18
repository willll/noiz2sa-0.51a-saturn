#pragma once

#include <impl/random.hpp>
#include <srl_input.hpp>

#include "memory_factory.h"

inline SRL::Math::Random<unsigned int>* createRandomGenerator(uint32_t seed)
{
  return createPooledObject<SRL::Math::Random<unsigned int>>(seed);
}

inline SRL::Input::Digital* createDigitalGamepad(int port)
{
  return createPooledObject<SRL::Input::Digital>(port);
}

inline void destroyRandomGenerator(SRL::Math::Random<unsigned int>*& random)
{
  destroyPooledObject(random);
}

inline void destroyDigitalGamepad(SRL::Input::Digital*& gamepad)
{
  destroyPooledObject(gamepad);
}

inline void releaseSystemFactoryPools()
{
  releasePooledObjectPool<SRL::Math::Random<unsigned int>>();
  releasePooledObjectPool<SRL::Input::Digital>();
}
