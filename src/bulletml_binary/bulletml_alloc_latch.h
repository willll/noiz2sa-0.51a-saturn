#pragma once

#include <cstdint>

inline uint32_t gBulletMlAllocFailures = 0;
inline bool gBulletMlAllocFailureLatched = false;

inline uint32_t& getBulletMlAllocFailureCount()
{
  return gBulletMlAllocFailures;
}

inline bool& getBulletMlAllocFailureLatch()
{
  return gBulletMlAllocFailureLatched;
}

inline bool hasBulletMlAllocFailureLatched()
{
  return getBulletMlAllocFailureLatch();
}

inline void resetBulletMlAllocFailureState()
{
  getBulletMlAllocFailureCount() = 0;
  getBulletMlAllocFailureLatch() = false;
}

inline void clearBulletMlAllocFailureLatch()
{
  getBulletMlAllocFailureLatch() = false;
}

/** @brief Whether a long-held latch must be cleared now, regardless of retry backoff. */
inline bool isForcedLatchClearDue(bool onTitle, int latchAge, int liveProjectiles, uint32_t hwFree)
{
  // Below 8KB HWRAM the recovery allocations would likely fail (hardware freeze
  // seen at ~4KB), so wait for the normal retry path instead.
  const uint32_t kMinHwFreeForRecovery = 8192u;
  if (onTitle || hwFree < kMinHwFreeForRecovery)
  {
    return false;
  }
  return latchAge >= 900 || (latchAge >= 300 && liveProjectiles <= 64);
}

inline uint32_t recordBulletMlAllocFailure()
{
  uint32_t& failCount = getBulletMlAllocFailureCount();
  bool& latched = getBulletMlAllocFailureLatch();
  failCount++;
  latched = true;
  return failCount;
}
