#ifndef NOIZ2SA_COLLISION_MATH_HPP
#define NOIZ2SA_COLLISION_MATH_HPP

#include "vector.h"

inline long long squaredDistance(const Vector &lhs, const Vector &rhs)
{
  const long long dx = (long long)lhs.x - (long long)rhs.x;
  const long long dy = (long long)lhs.y - (long long)rhs.y;
  return dx * dx + dy * dy;
}

inline bool shotHitsFoe(const Vector &foePos, const Vector &shotPos, int foeScanSize, int shotScanHeight)
{
  return absN(foePos.x - shotPos.x) < foeScanSize &&
         absN(foePos.y - shotPos.y) < (foeScanSize + shotScanHeight);
}

inline bool movingBulletHitsShip(const Vector &bulletPos,
                                 const Vector &bulletPrevPos,
                                 const Vector &shipPos,
                                 int shipHitWidth)
{
  if (squaredDistance(bulletPos, shipPos) < shipHitWidth ||
      squaredDistance(bulletPrevPos, shipPos) < shipHitWidth)
  {
    return true;
  }

  const long long dx = (long long)bulletPos.x - (long long)bulletPrevPos.x;
  const long long dy = (long long)bulletPos.y - (long long)bulletPrevPos.y;
  const long long segmentLengthSquared = dx * dx + dy * dy;
  if (segmentLengthSquared <= 1)
  {
    return false;
  }

  const long long relX = (long long)shipPos.x - (long long)bulletPrevPos.x;
  const long long relY = (long long)shipPos.y - (long long)bulletPrevPos.y;
  const long long projection = dx * relX + dy * relY;
  if (projection <= 0 || projection >= segmentLengthSquared)
  {
    return false;
  }

  const long long relLengthSquared = relX * relX + relY * relY;
  const long long perpendicularNumerator = relLengthSquared * segmentLengthSquared - projection * projection;
  return perpendicularNumerator >= 0 &&
         perpendicularNumerator < (long long)shipHitWidth * segmentLengthSquared;
}

#endif
