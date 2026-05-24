#pragma once

#include <cstddef>
#include <utility>

#include "memory_factory.h"

template <typename T, typename... Args>
/** @brief Creates a BulletML runtime object in high work RAM. */
inline T* createBulletMlRuntimeObject(Args&&... args)
{
  return createHighWorkRamObject<T>(std::forward<Args>(args)...);
}

template <typename T>
/** @brief Creates a BulletML runtime array in high work RAM. */
inline T* createBulletMlRuntimeArray(std::size_t count)
{
  return createHighWorkRamArray<T>(count);
}
