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

inline uint32_t recordBulletMlAllocFailure()
{
  uint32_t& failCount = getBulletMlAllocFailureCount();
  bool& latched = getBulletMlAllocFailureLatch();
  failCount++;
  latched = true;
  return failCount;
}
