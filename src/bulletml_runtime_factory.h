#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "memory_factory.h"

namespace bulletml_runtime_trace {
struct RuntimeAllocStats {
  std::size_t objectAllocCalls = 0;
  std::size_t objectFreeCalls = 0;
  std::size_t arrayAllocCalls = 0;
  std::size_t arrayFreeCalls = 0;
  std::size_t objectLiveBytes = 0;
  std::size_t arrayLiveBytes = 0;
};

inline RuntimeAllocStats& stats()
{
  static RuntimeAllocStats state{};
  return state;
}

inline void noteAlloc(std::size_t bytes, bool arrayAlloc)
{
  RuntimeAllocStats& s = stats();
  if (arrayAlloc)
  {
    s.arrayAllocCalls++;
    s.arrayLiveBytes += bytes;
    return;
  }
  s.objectAllocCalls++;
  s.objectLiveBytes += bytes;
}

inline void noteFree(std::size_t bytes, bool arrayFree)
{
  RuntimeAllocStats& s = stats();
  if (arrayFree)
  {
    s.arrayFreeCalls++;
    if (s.arrayLiveBytes >= bytes)
    {
      s.arrayLiveBytes -= bytes;
    }
    else
    {
      s.arrayLiveBytes = 0;
    }
    return;
  }
  s.objectFreeCalls++;
  if (s.objectLiveBytes >= bytes)
  {
    s.objectLiveBytes -= bytes;
  }
  else
  {
    s.objectLiveBytes = 0;
  }
}
} // namespace bulletml_runtime_trace

template <typename T, typename... Args>
/** @brief Creates a BulletML runtime object in high work RAM. */
inline T* createBulletMlRuntimeObject(Args&&... args)
{
  bulletml_runtime_trace::noteAlloc(sizeof(T), false);
  return lwnew T(std::forward<Args>(args)...);
}

template <typename T>
/** @brief Creates a BulletML runtime array in high work RAM. */
inline T* createBulletMlRuntimeArray(std::size_t count)
{
  bulletml_runtime_trace::noteAlloc(sizeof(T) * count, true);
  return lwnew T[count];
}

template <typename T>
/** @brief Destroys a BulletML runtime object and clears the pointer. */
inline void destroyBulletMlRuntimeObject(T*& ptr)
{
  if (!ptr)
  {
    return;
  }
  bulletml_runtime_trace::noteFree(sizeof(T), false);
  delete ptr;
  ptr = nullptr;
}

template <typename T>
/** @brief Destroys a BulletML runtime array and clears the pointer. */
inline void destroyBulletMlRuntimeArray(T*& ptr, std::size_t count)
{
  if (!ptr)
  {
    return;
  }
  bulletml_runtime_trace::noteFree(sizeof(T) * count, true);
  delete[] ptr;
  ptr = nullptr;
}

/** @brief Frees runtime-allocated raw storage while updating trace counters. */
inline void freeBulletMlRuntimeRaw(void* ptr, std::size_t bytes, bool arrayAlloc)
{
  if (!ptr)
  {
    return;
  }
  bulletml_runtime_trace::noteFree(bytes, arrayAlloc);
  SRL::Memory::Free(ptr);
}

/** @brief Returns outstanding BulletML runtime object bytes. */
inline std::size_t getBulletMlRuntimeObjectLiveBytes()
{
  return bulletml_runtime_trace::stats().objectLiveBytes;
}

/** @brief Returns outstanding BulletML runtime array bytes. */
inline std::size_t getBulletMlRuntimeArrayLiveBytes()
{
  return bulletml_runtime_trace::stats().arrayLiveBytes;
}

/** @brief Returns total outstanding BulletML runtime bytes. */
inline std::size_t getBulletMlRuntimeLiveBytes()
{
  return getBulletMlRuntimeObjectLiveBytes() + getBulletMlRuntimeArrayLiveBytes();
}

/** @brief Returns cumulative runtime object alloc/free counts. */
inline void getBulletMlRuntimeObjectCallCounts(std::size_t& allocCalls, std::size_t& freeCalls)
{
  const bulletml_runtime_trace::RuntimeAllocStats& s = bulletml_runtime_trace::stats();
  allocCalls = s.objectAllocCalls;
  freeCalls = s.objectFreeCalls;
}

/** @brief Returns cumulative runtime array alloc/free counts. */
inline void getBulletMlRuntimeArrayCallCounts(std::size_t& allocCalls, std::size_t& freeCalls)
{
  const bulletml_runtime_trace::RuntimeAllocStats& s = bulletml_runtime_trace::stats();
  allocCalls = s.arrayAllocCalls;
  freeCalls = s.arrayFreeCalls;
}
