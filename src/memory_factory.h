#pragma once

#include <cstddef>
#include <cstdlib>
#include <utility>

#include <srl.hpp>
#include <srl_memory.hpp>

template <typename T, typename... Args>
/** @brief Allocates a heap object and forwards the constructor arguments. */
inline T* createObject(Args&&... args)
{
  return new T(std::forward<Args>(args)...);
}

template <typename T>
/** @brief Destroys a heap object and clears the pointer. */
inline void destroyObject(T*& ptr)
{
  delete ptr;
  ptr = nullptr;
}

template <typename T>
/** @brief Allocates a heap array. */
inline T* createArray(std::size_t count)
{
  return new T[count];
}

template <typename T>
/** @brief Destroys a heap array and clears the pointer. */
inline void destroyArray(T*& ptr)
{
  delete[] ptr;
  ptr = nullptr;
}

template <typename T, typename... Args>
/** @brief Allocates an object in high work RAM and forwards the constructor arguments. */
inline T* createHighWorkRamObject(Args&&... args)
{
  return hwnew T(std::forward<Args>(args)...);
}

template <typename T>
/** @brief Allocates an array in high work RAM. */
inline T* createHighWorkRamArray(std::size_t count)
{
  return hwnew T[count];
}

template <typename T>
/** @brief Allocates raw heap storage for count items. */
inline T* allocateHeapItems(std::size_t count)
{
  return static_cast<T*>(std::malloc(sizeof(T) * count));
}

template <typename T>
/** @brief Frees raw heap storage and clears the pointer. */
inline void freeHeapItems(T*& ptr)
{
  std::free(ptr);
  ptr = nullptr;
}

template <typename T>
/** @brief Allocates low work RAM storage for count items. */
inline T* allocateLowWorkRamItems(std::size_t count)
{
  return static_cast<T*>(SRL::Memory::LowWorkRam::Malloc(sizeof(T) * count));
}

template <typename T>
/** @brief Allocates high work RAM storage for count items. */
inline T* allocateHighWorkRamItems(std::size_t count)
{
  return static_cast<T*>(SRL::Memory::HighWorkRam::Malloc(sizeof(T) * count));
}
