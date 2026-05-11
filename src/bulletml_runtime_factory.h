#pragma once

#include <cstddef>
#include <utility>

#include "memory_factory.h"

template <typename T, typename... Args>
inline T* createBulletMlRuntimeObject(Args&&... args)
{
  return createHighWorkRamObject<T>(std::forward<Args>(args)...);
}

template <typename T>
inline T* createBulletMlRuntimeArray(std::size_t count)
{
  return createHighWorkRamArray<T>(count);
}
