#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#include <srl.hpp>
#include <srl_memory.hpp>

namespace factory_pool
{
template <typename T>
struct HeapFreeNode
{
  HeapFreeNode* next;
};

template <typename T>
struct HeapPoolState
{
  static inline HeapFreeNode<T>* freeList = nullptr;
  static inline std::size_t cachedCount = 0;
};
}

template <typename T, typename... Args>
/** @brief Allocates a heap object and forwards the constructor arguments. */
inline T* createObject(Args&&... args)
{
  return new T(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
/** @brief Allocates or reuses a heap object from a type-local pool. */
inline T* createPooledObject(Args&&... args)
{
  using PoolState = factory_pool::HeapPoolState<T>;
  using FreeNode = factory_pool::HeapFreeNode<T>;

  if (PoolState::freeList)
  {
    FreeNode* node = PoolState::freeList;
    PoolState::freeList = node->next;
    if (PoolState::cachedCount > 0)
    {
      PoolState::cachedCount--;
    }
    return new (node) T(std::forward<Args>(args)...);
  }

  return createObject<T>(std::forward<Args>(args)...);
}

template <typename T>
/** @brief Destroys a heap object and clears the pointer. */
inline void destroyObject(T*& ptr)
{
  delete ptr;
  ptr = nullptr;
}

template <typename T>
/** @brief Destroys an object and returns its storage to the heap object pool. */
inline void destroyPooledObject(T*& ptr)
{
  using PoolState = factory_pool::HeapPoolState<T>;
  using FreeNode = factory_pool::HeapFreeNode<T>;

  if (!ptr)
  {
    return;
  }

  ptr->~T();
  FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
  node->next = PoolState::freeList;
  PoolState::freeList = node;
  PoolState::cachedCount++;
  ptr = nullptr;
}

template <typename T>
/** @brief Releases all currently cached heap nodes for type T. */
inline void releasePooledObjectPool()
{
  using PoolState = factory_pool::HeapPoolState<T>;
  using FreeNode = factory_pool::HeapFreeNode<T>;

  while (PoolState::freeList)
  {
    FreeNode* node = PoolState::freeList;
    PoolState::freeList = node->next;
    ::operator delete(node);
  }

  PoolState::cachedCount = 0;
}

template <typename T>
/** @brief Returns the number of cached heap nodes for type T. */
inline std::size_t getPooledObjectCachedCount()
{
  return factory_pool::HeapPoolState<T>::cachedCount;
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
