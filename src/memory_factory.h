#pragma once

#include <cstddef>
#include <cstdlib>
#include <utility>

#include <srl.hpp>
#include <srl_memory.hpp>

template <typename T, typename... Args>
inline T* createObject(Args&&... args)
{
  return new T(std::forward<Args>(args)...);
}

template <typename T>
inline void destroyObject(T*& ptr)
{
  delete ptr;
  ptr = nullptr;
}

template <typename T>
inline T* createArray(std::size_t count)
{
  return new T[count];
}

template <typename T>
inline void destroyArray(T*& ptr)
{
  delete[] ptr;
  ptr = nullptr;
}

template <typename T, typename... Args>
inline T* createHighWorkRamObject(Args&&... args)
{
  return hwnew T(std::forward<Args>(args)...);
}

template <typename T>
inline T* createHighWorkRamArray(std::size_t count)
{
  return hwnew T[count];
}

template <typename T>
inline T* allocateHeapItems(std::size_t count)
{
  return static_cast<T*>(std::malloc(sizeof(T) * count));
}

template <typename T>
inline void freeHeapItems(T*& ptr)
{
  std::free(ptr);
  ptr = nullptr;
}

template <typename T>
inline T* allocateLowWorkRamItems(std::size_t count)
{
  return static_cast<T*>(SRL::Memory::LowWorkRam::Malloc(sizeof(T) * count));
}

template <typename T>
inline T* allocateHighWorkRamItems(std::size_t count)
{
  return static_cast<T*>(SRL::Memory::HighWorkRam::Malloc(sizeof(T) * count));
}
