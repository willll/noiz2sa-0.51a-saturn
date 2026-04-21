#include <srl.hpp>
#include <srl_log.hpp>

#include "minunit.h"

#include "../../src/vector.h"

// ---------------------------------------------------------------------------
// FPS counter arithmetic helpers
// Mirror the vblank-based computation in noiz2sa.cpp without any rendering
// or global-state side effects.
//
// NTSC vblank = 60 Hz.  fps*100 = 6000 / vblanks_between_frames.
// ---------------------------------------------------------------------------
static uint32_t computeFpsTimes100(uint32_t vblanksElapsed)
{
  if (vblanksElapsed > 0u)
    return 6000u / vblanksElapsed;
  return 6000u; // first-frame / no-vblank-yet default
}

static void computeFpsDisplay(uint32_t fpsTimes100, int32_t &outWhole, int32_t &outFrac)
{
  outWhole = (int32_t)(fpsTimes100 / 100u);
  outFrac  = (int32_t)(fpsTimes100 % 100u);
}

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
  const double startX = (double)bulletPrevPos.x;
  const double startY = (double)bulletPrevPos.y;
  const double deltaX = (double)(bulletPos.x - bulletPrevPos.x);
  const double deltaY = (double)(bulletPos.y - bulletPrevPos.y);
  const double shipX = (double)shipPos.x;
  const double shipY = (double)shipPos.y;
  const double lengthSquared = deltaX * deltaX + deltaY * deltaY;

  if (lengthSquared <= 1.0)
  {
    const double pointX = shipX - startX;
    const double pointY = shipY - startY;
    return (pointX * pointX + pointY * pointY) <= (double)shipHitWidth;
  }

  double t = ((shipX - startX) * deltaX + (shipY - startY) * deltaY) / lengthSquared;
  if (t < 0.0)
  {
    t = 0.0;
  }
  else if (t > 1.0)
  {
    t = 1.0;
  }

  const double nearestX = startX + deltaX * t;
  const double nearestY = startY + deltaY * t;
  const double distX = shipX - nearestX;
  const double distY = shipY - nearestY;
  return (distX * distX + distY * distY) <= (double)shipHitWidth;
}

static bool shouldDestroyShipNow(int status)
{
  // Invincibility has been removed; only in-game state can consume a life.
  return status == 1;
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

  // Ship on the segment.
  Vector prev{0, 0};
  Vector cur{1000, 0};
  Vector ship{200, 0};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Zero-length motion falls back to point-hit.
  prev = {0, 0};
  cur = {0, 0};
  ship = {200, 0};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Ship far from segment should miss.
  prev = {1000, 0};
  cur = {2000, 0};
  ship = {500, 600};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Ship beyond segment endpoint should miss.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {2600, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Parallel pass outside hit radius should miss.
  prev = {-2000, 1000};
  cur = {2000, 1000};
  ship = {0, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Endpoint contact should count.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {0, 200};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // End endpoint contact should also count.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {1000, 200};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Segment-middle proximity: catches cases missed by ppos-centered checks.
  prev = {-2000, 0};
  cur = {2000, 0};
  ship = {0, 300};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Just outside hit radius should miss.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {300, 520};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {120000, 70000};
  cur = {121200, 70000};
  ship = {120600, 70100};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));
}

MU_TEST(test_destroyShip_no_invincibility_behavior)
{
  const int TITLE = 0;
  const int IN_GAME = 1;
  const int GAMEOVER = 2;

  mu_check(!shouldDestroyShipNow(TITLE));
  mu_check(shouldDestroyShipNow(IN_GAME));
  mu_check(!shouldDestroyShipNow(GAMEOVER));
}

MU_TEST(test_add_sub_mul_div_extended)
{
  Vector a{20, -30};
  Vector b{-4, 10};

  vctAdd(&a, &b);
  mu_assert_int_eq(16, a.x);
  mu_assert_int_eq(-20, a.y);

  vctSub(&a, &b);
  mu_assert_int_eq(20, a.x);
  mu_assert_int_eq(-30, a.y);

  vctMul(&a, 3);
  mu_assert_int_eq(60, a.x);
  mu_assert_int_eq(-90, a.y);

  vctDiv(&a, 3);
  mu_assert_int_eq(20, a.x);
  mu_assert_int_eq(-30, a.y);
}

MU_TEST(test_inner_product_large_values_extended)
{
  Vector a{120000, -50000};
  Vector b{120000, 50000};

  const float dot = vctInnerProduct(&a, &b);
  mu_assert(dot > 1.18e10f, "dot product too small");
  mu_assert(dot < 1.20e10f, "dot product too large");
}

MU_TEST(test_get_element_projection_extended)
{
  Vector v{9000, 3000};
  Vector axis{3000, 0};
  Vector p = vctGetElement(&v, &axis);
  mu_assert_int_eq(9000, p.x);
  mu_assert_int_eq(0, p.y);

  Vector zero{0, 0};
  p = vctGetElement(&v, &zero);
  mu_assert_int_eq(0, p.x);
  mu_assert_int_eq(0, p.y);
}

MU_TEST(test_check_side_extended)
{
  Vector p{10, 10};

  Vector verticalA{5, 0};
  Vector verticalB{5, 20};
  mu_assert_int_eq(5, vctCheckSide(&p, &verticalA, &verticalB));

  Vector horizontalA{0, 5};
  Vector horizontalB{20, 5};
  mu_assert_int_eq(-5, vctCheckSide(&p, &horizontalA, &horizontalB));

  Vector diagA{0, 0};
  Vector diagB{10, 10};
  mu_assert_int_eq(0, vctCheckSide(&p, &diagA, &diagB));
}

MU_TEST(test_size_and_dist_extended)
{
  Vector v{3, 4};
  mu_assert_int_eq(5, vctSize(&v));

  Vector big{30000, 40000};
  mu_assert_int_eq(50000, vctSize(&big));

  Vector a{100, 20};
  Vector b{0, 0};
  mu_assert_int_eq(110, vctDist(&a, &b));

  Vector c{20, 100};
  mu_assert_int_eq(110, vctDist(&c, &b));
}

// ---------------------------------------------------------------------------
// FPS counter tests
// These feed raw Fxp delta-time values through the same arithmetic used by
// drawFpsCounter() and assert the resulting whole/fractional parts.
//
// Raw value encoding: 16.16 fixed point, so 1.0 s = 0x10000 = 65536.
// Hardware division: AsyncDivSet sets 64-bit dividend = (raw << 16),
// divisor = deltaRaw, result stored in dvdntl.
// Equivalent: fpsRaw = (65536 * 65536) / deltaRaw = 4294967296 / deltaRaw.
// ---------------------------------------------------------------------------

MU_TEST(test_fps_zero_vblanks_returns_default_60fps)
{
  // First-frame / no-vblanks-yet path must not divide by zero and must
  // return 60.00 fps as the boot default.
  mu_assert_int_eq(6000u, computeFpsTimes100(0u));
  int32_t w, f;
  computeFpsDisplay(6000u, w, f);
  mu_assert_int_eq(60, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_1_vblank_is_60fps)
{
  // 1 vblank between renders → 60 fps (NTSC full rate)
  const uint32_t t = computeFpsTimes100(1u);
  mu_assert_int_eq(6000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(60, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_2_vblanks_is_30fps)
{
  // 2 vblanks between renders → 30 fps (typical Saturn VDP1 target)
  const uint32_t t = computeFpsTimes100(2u);
  mu_assert_int_eq(3000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(30, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_3_vblanks_is_20fps)
{
  // 3 vblanks → 20 fps
  const uint32_t t = computeFpsTimes100(3u);
  mu_assert_int_eq(2000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(20, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_4_vblanks_is_15fps)
{
  // 4 vblanks → 15 fps
  const uint32_t t = computeFpsTimes100(4u);
  mu_assert_int_eq(1500u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(15, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_display_fractional_frac)
{
  // fpsTimes100 = 4567 → whole=45, frac=67
  int32_t w, f;
  computeFpsDisplay(4567u, w, f);
  mu_assert_int_eq(45, w);
  mu_assert_int_eq(67, f);
}

MU_TEST(test_fps_display_zero_fpstimes100)
{
  // Edge: fpsTimes100 == 0 → whole=0, frac=0
  int32_t w, f;
  computeFpsDisplay(0u, w, f);
  mu_assert_int_eq(0, w);
  mu_assert_int_eq(0, f);
}

MU_TEST_SUITE(fps_test_suite)
{
  MU_RUN_TEST(test_fps_zero_vblanks_returns_default_60fps);
  MU_RUN_TEST(test_fps_1_vblank_is_60fps);
  MU_RUN_TEST(test_fps_2_vblanks_is_30fps);
  MU_RUN_TEST(test_fps_3_vblanks_is_20fps);
  MU_RUN_TEST(test_fps_4_vblanks_is_15fps);
  MU_RUN_TEST(test_fps_display_fractional_frac);
  MU_RUN_TEST(test_fps_display_zero_fpstimes100);
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
  MU_RUN_TEST(test_destroyShip_no_invincibility_behavior);
}

MU_TEST_SUITE(vector_test_suite)
{
  MU_RUN_TEST(test_add_sub_mul_div_extended);
  MU_RUN_TEST(test_inner_product_large_values_extended);
  MU_RUN_TEST(test_get_element_projection_extended);
  MU_RUN_TEST(test_check_side_extended);
  MU_RUN_TEST(test_size_and_dist_extended);
}

int main()
{
  SRL::Core::Initialize(HighColor(20, 10, 50));

  LogInfo(strStart);

  MU_RUN_SUITE(fps_test_suite);
  MU_RUN_SUITE(collision_test_suite);
  MU_RUN_SUITE(vector_test_suite);
  MU_REPORT();

  LogInfo(strEnd);

  // Keep emulator alive long enough for log flush + marker detection.
  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
