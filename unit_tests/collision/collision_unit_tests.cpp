#include <cmath>
#include <cstdlib>
#include <iostream>

#include "collision_math.hpp"

static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                                 \
  do                                                                                      \
  {                                                                                       \
    if (!(expr))                                                                          \
    {                                                                                     \
      std::cerr << "EXPECT_TRUE failed at line " << __LINE__ << ": " #expr "\n";      \
      g_failed++;                                                                         \
    }                                                                                     \
  } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(a, b)                                                                   \
  do                                                                                      \
  {                                                                                       \
    const auto _a = (a);                                                                  \
    const auto _b = (b);                                                                  \
    if (!(_a == _b))                                                                      \
    {                                                                                     \
      std::cerr << "EXPECT_EQ failed at line " << __LINE__ << ": " #a "=" << _a         \
                << " " #b "=" << _b << "\n";                                             \
      g_failed++;                                                                         \
    }                                                                                     \
  } while (0)

static void test_vctDist_branches()
{
  Vector a{100, 20};
  Vector b{0, 0};
  EXPECT_EQ(vctDist(&a, &b), 100 + (20 >> 1));

  Vector c{20, 100};
  EXPECT_EQ(vctDist(&c, &b), 100 + (20 >> 1));
}

static void test_basic_vector_ops()
{
  Vector a{10, -4};
  Vector b{2, 6};

  vctAdd(&a, &b);
  EXPECT_EQ(a.x, 12);
  EXPECT_EQ(a.y, 2);

  vctSub(&a, &b);
  EXPECT_EQ(a.x, 10);
  EXPECT_EQ(a.y, -4);

  vctMul(&a, 3);
  EXPECT_EQ(a.x, 30);
  EXPECT_EQ(a.y, -12);

  vctDiv(&a, 3);
  EXPECT_EQ(a.x, 10);
  EXPECT_EQ(a.y, -4);
}

static void test_vctCheckSide_branches()
{
  Vector p{10, 10};

  // xo == 0, yo != 0
  Vector l1{5, 0};
  Vector l2{5, 20};
  EXPECT_EQ(vctCheckSide(&p, &l1, &l2), 5);

  // yo == 0
  Vector h1{0, 5};
  Vector h2{20, 5};
  EXPECT_EQ(vctCheckSide(&p, &h1, &h2), -5);

  // xo * yo > 0
  Vector d1{0, 0};
  Vector d2{10, 10};
  EXPECT_EQ(vctCheckSide(&p, &d1, &d2), 0);

  // xo * yo < 0
  Vector d3{0, 10};
  Vector d4{10, 0};
  EXPECT_TRUE(vctCheckSide(&p, &d3, &d4) != 0);
}

static void test_vctInnerProduct_large_values_no_overflow()
{
  Vector a{120000, 0};
  Vector b{120000, 0};
  const float dot = vctInnerProduct(&a, &b);
  EXPECT_TRUE(dot > 1.0e10f);
  EXPECT_TRUE(dot < 2.0e10f);
}

static void test_vctGetElement_projection()
{
  Vector alongX{10000, 0};
  Vector base{500, 0};
  Vector p = vctGetElement(&alongX, &base);
  EXPECT_EQ(p.x, 10000);
  EXPECT_EQ(p.y, 0);

  Vector zero{0, 0};
  p = vctGetElement(&alongX, &zero);
  EXPECT_EQ(p.x, 0);
  EXPECT_EQ(p.y, 0);
}

static void test_vctSize()
{
  Vector v{3, 4};
  EXPECT_EQ(vctSize(&v), 5);
}

static void test_shotHitsFoe_full_coverage()
{
  const Vector foe{10000, 20000};
  const int foeScan = 2000;
  const int shotScanHeight = 300;

  const Vector hit{11000, 20500};
  EXPECT_TRUE(shotHitsFoe(foe, hit, foeScan, shotScanHeight));

  // Strictly less-than boundary: equal should not hit.
  const Vector missBoundaryX{12000, 20500};
  EXPECT_FALSE(shotHitsFoe(foe, missBoundaryX, foeScan, shotScanHeight));

  const Vector missBoundaryY{11000, 22300};
  EXPECT_FALSE(shotHitsFoe(foe, missBoundaryY, foeScan, shotScanHeight));

  const Vector missFar{5000, 10000};
  EXPECT_FALSE(shotHitsFoe(foe, missFar, foeScan, shotScanHeight));
}

static void test_movingBulletHitsShip_full_coverage()
{
  const int shipHitWidth = 512 * 512;

  // True branch: segment crosses near ship center.
  Vector prev{-2000, 0};
  Vector cur{2000, 0};
  Vector ship{0, 0};
  EXPECT_TRUE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // inaa <= 1 branch.
  prev = {0, 0};
  cur = {0, 0};
  ship = {600, 0};
  EXPECT_FALSE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // ht <= 0 branch.
  prev = {1000, 0};
  cur = {2000, 0};
  ship = {500, 600};
  EXPECT_FALSE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // ht >= 1 branch.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {2500, 600};
  EXPECT_FALSE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // hd >= shipHitWidth branch (parallel pass, too far).
  prev = {-2000, 1000};
  cur = {2000, 1000};
  ship = {0, 0};
  EXPECT_FALSE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Endpoint overlap must still count as a hit.
  prev = {0, 0};
  cur = {0, 0};
  ship = {200, 0};
  EXPECT_TRUE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // GP32-style gameplay case at large world coordinates.
  prev = {120000, 70000};
  cur = {121200, 70000};
  ship = {120600, 70100};
  EXPECT_TRUE(movingBulletHitsShip(cur, prev, ship, shipHitWidth));
}

int main()
{
  test_basic_vector_ops();
  test_vctCheckSide_branches();
  test_vctDist_branches();
  test_vctInnerProduct_large_values_no_overflow();
  test_vctGetElement_projection();
  test_vctSize();
  test_shotHitsFoe_full_coverage();
  test_movingBulletHitsShip_full_coverage();

  if (g_failed != 0)
  {
    std::cerr << "Collision unit tests FAILED: " << g_failed << " assertion(s).\n";
    return EXIT_FAILURE;
  }

  std::cout << "Collision unit tests PASSED.\n";
  return EXIT_SUCCESS;
}
