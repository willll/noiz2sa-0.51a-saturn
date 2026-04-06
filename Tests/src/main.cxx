#include <srl.hpp>
#include <srl_log.hpp>

#include "minunit.h"

#include "../../src/vector.h"

static bool shotHitsFoe(const Vector &foePos, const Vector &shotPos, int foeScanSize, int shotScanHeight)
{
  return absN(foePos.x - shotPos.x) < foeScanSize &&
         absN(foePos.y - shotPos.y) < foeScanSize + shotScanHeight;
}

static bool shotHitsFoeSwept(const Vector &foePos,
                             const Vector &shotPos,
                             int shotPrevY,
                             int foeScanSize,
                             int shotScanHeight,
                             int shotScanHalfWidth)
{
  (void)shotPrevY;
  (void)shotScanHalfWidth;
  return shotHitsFoe(foePos, shotPos, foeScanSize, shotScanHeight);
}

static bool movingBulletHitsShip(const Vector &bulletPos,
                                 const Vector &bulletPrevPos,
                                 const Vector &shipPos,
                                 int shipHitWidth)
{
  Vector bmv = bulletPos;
  vctSub(&bmv, (Vector *)&bulletPrevPos);
  const float inaa = vctInnerProduct(&bmv, &bmv);
  if (inaa <= 1.0f)
  {
    return false;
  }

  Vector sofs = shipPos;
  vctSub(&sofs, (Vector *)&bulletPrevPos);
  const float inab = vctInnerProduct(&bmv, &sofs);
  const float ht = inab / inaa;
  if (ht <= 0.0f || ht >= 1.0f)
  {
    return false;
  }

  const float hd = vctInnerProduct(&sofs, &sofs) - inab * inab / inaa / inaa;
  return hd >= 0.0f && hd < (float)shipHitWidth;
}

using namespace SRL::Types;
using namespace SRL::Logger;

extern "C"
{
  const char *const strStart = "***UT_START***";
  const char *const strEnd = "***UT_END***";
}

MU_TEST(test_vctDist_branches)
{
  Vector a{100, 20};
  Vector b{0, 0};
  mu_assert_int_eq(110, vctDist(&a, &b));

  Vector c{20, 100};
  mu_assert_int_eq(110, vctDist(&c, &b));
}

MU_TEST(test_basic_vector_ops)
{
  Vector a{10, -4};
  Vector b{2, 6};

  vctAdd(&a, &b);
  mu_assert_int_eq(12, a.x);
  mu_assert_int_eq(2, a.y);

  vctSub(&a, &b);
  mu_assert_int_eq(10, a.x);
  mu_assert_int_eq(-4, a.y);

  vctMul(&a, 3);
  mu_assert_int_eq(30, a.x);
  mu_assert_int_eq(-12, a.y);

  vctDiv(&a, 3);
  mu_assert_int_eq(10, a.x);
  mu_assert_int_eq(-4, a.y);
}

MU_TEST(test_vctCheckSide_branches)
{
  Vector p{10, 10};

  Vector l1{5, 0};
  Vector l2{5, 20};
  mu_assert_int_eq(5, vctCheckSide(&p, &l1, &l2));

  Vector h1{0, 5};
  Vector h2{20, 5};
  mu_assert_int_eq(-5, vctCheckSide(&p, &h1, &h2));

  Vector d1{0, 0};
  Vector d2{10, 10};
  mu_assert_int_eq(0, vctCheckSide(&p, &d1, &d2));

  Vector d3{0, 10};
  Vector d4{10, 0};
  mu_assert(vctCheckSide(&p, &d3, &d4) != 0, "negative slope side check should be non-zero");
}

MU_TEST(test_vctInnerProduct_large_values_no_overflow)
{
  Vector a{120000, 0};
  Vector b{120000, 0};
  const float dot = vctInnerProduct(&a, &b);
  mu_assert(dot > 1.0e10f, "dot product too small");
  mu_assert(dot < 2.0e10f, "dot product too large");
}

MU_TEST(test_vctGetElement_projection)
{
  Vector alongX{10000, 0};
  Vector base{500, 0};
  Vector p = vctGetElement(&alongX, &base);
  mu_assert_int_eq(10000, p.x);
  mu_assert_int_eq(0, p.y);

  Vector zero{0, 0};
  p = vctGetElement(&alongX, &zero);
  mu_assert_int_eq(0, p.x);
  mu_assert_int_eq(0, p.y);
}

MU_TEST(test_vctSize)
{
  Vector v{3, 4};
  mu_assert_int_eq(5, vctSize(&v));
}

MU_TEST(test_shotHitsFoe_boundaries)
{
  const Vector foe{10000, 20000};
  const int foeScan = 2000;
  const int shotScanHeight = 300;

  const Vector hit{11000, 20500};
  mu_check(shotHitsFoe(foe, hit, foeScan, shotScanHeight));

  const Vector missBoundaryX{12000, 20500};
  mu_check(!shotHitsFoe(foe, missBoundaryX, foeScan, shotScanHeight));

  const Vector missBoundaryY{11000, 22300};
  mu_check(!shotHitsFoe(foe, missBoundaryY, foeScan, shotScanHeight));
}

MU_TEST(test_shotHitsFoeSwept_tunneling)
{
  const Vector foe{10000, 20000};
  const int foeScan = 2000;
  const int shotScanHeight = 300;
  const int shotScanHalfWidth = 512;

  const Vector shotNow{10000, 17000};
  const int shotPrevY = 23000;
  mu_check(!shotHitsFoeSwept(foe, shotNow, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotCurrentHit{10000, 19900};
  mu_check(shotHitsFoeSwept(foe, shotCurrentHit, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotFarX{13000, 17000};
  mu_check(!shotHitsFoeSwept(foe, shotFarX, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotMissY{10000, 12000};
  mu_check(!shotHitsFoeSwept(foe, shotMissY, 14000, foeScan, shotScanHeight, shotScanHalfWidth));
}

MU_TEST(test_movingBulletHitsShip_full_coverage)
{
  const int shipHitWidth = 512 * 512;

  Vector prev{0, 0};
  Vector cur{1000, 0};
  Vector ship{200, 0};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {0, 0};
  cur = {0, 0};
  ship = {700, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {1000, 0};
  cur = {2000, 0};
  ship = {500, 600};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {0, 0};
  cur = {1000, 0};
  ship = {2600, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {-2000, 1000};
  cur = {2000, 1000};
  ship = {0, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {0, 0};
  cur = {0, 0};
  ship = {200, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {120000, 70000};
  cur = {121200, 70000};
  ship = {120600, 70100};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));
}

MU_TEST_SUITE(collision_test_suite)
{
  MU_RUN_TEST(test_basic_vector_ops);
  MU_RUN_TEST(test_vctCheckSide_branches);
  MU_RUN_TEST(test_vctDist_branches);
  MU_RUN_TEST(test_vctInnerProduct_large_values_no_overflow);
  MU_RUN_TEST(test_vctGetElement_projection);
  MU_RUN_TEST(test_vctSize);
  MU_RUN_TEST(test_shotHitsFoe_boundaries);
  MU_RUN_TEST(test_shotHitsFoeSwept_tunneling);
  MU_RUN_TEST(test_movingBulletHitsShip_full_coverage);
}

int main()
{
  SRL::Core::Initialize(HighColor(20, 10, 50));

  LogInfo(strStart);

  MU_RUN_SUITE(collision_test_suite);
  MU_REPORT();

  LogInfo(strEnd);

  // Keep emulator alive long enough for log flush + marker detection.
  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
