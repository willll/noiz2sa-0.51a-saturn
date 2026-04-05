#include <cstdlib>
#include <iostream>

#include "vector.h"

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

static void test_add_sub_mul_div()
{
  Vector a{20, -30};
  Vector b{-4, 10};

  vctAdd(&a, &b);
  EXPECT_EQ(a.x, 16);
  EXPECT_EQ(a.y, -20);

  vctSub(&a, &b);
  EXPECT_EQ(a.x, 20);
  EXPECT_EQ(a.y, -30);

  vctMul(&a, 3);
  EXPECT_EQ(a.x, 60);
  EXPECT_EQ(a.y, -90);

  vctDiv(&a, 3);
  EXPECT_EQ(a.x, 20);
  EXPECT_EQ(a.y, -30);
}

static void test_inner_product_large_values()
{
  Vector a{120000, -50000};
  Vector b{120000, 50000};

  // 120000*120000 + (-50000*50000) = 11900000000
  const float dot = vctInnerProduct(&a, &b);
  EXPECT_TRUE(dot > 1.18e10f);
  EXPECT_TRUE(dot < 1.20e10f);
}

static void test_get_element_projection()
{
  Vector v{9000, 3000};
  Vector axis{3000, 0};
  Vector p = vctGetElement(&v, &axis);
  EXPECT_EQ(p.x, 9000);
  EXPECT_EQ(p.y, 0);

  Vector zero{0, 0};
  p = vctGetElement(&v, &zero);
  EXPECT_EQ(p.x, 0);
  EXPECT_EQ(p.y, 0);
}

static void test_check_side_cases()
{
  Vector p{10, 10};

  Vector verticalA{5, 0};
  Vector verticalB{5, 20};
  EXPECT_EQ(vctCheckSide(&p, &verticalA, &verticalB), 5);

  Vector horizontalA{0, 5};
  Vector horizontalB{20, 5};
  EXPECT_EQ(vctCheckSide(&p, &horizontalA, &horizontalB), -5);

  Vector diagA{0, 0};
  Vector diagB{10, 10};
  EXPECT_EQ(vctCheckSide(&p, &diagA, &diagB), 0);
}

static void test_size_and_dist()
{
  Vector v{3, 4};
  EXPECT_EQ(vctSize(&v), 5);

  Vector big{30000, 40000};
  EXPECT_EQ(vctSize(&big), 50000);

  Vector a{100, 20};
  Vector b{0, 0};
  EXPECT_EQ(vctDist(&a, &b), 110);

  Vector c{20, 100};
  EXPECT_EQ(vctDist(&c, &b), 110);
}

int main()
{
  test_add_sub_mul_div();
  test_inner_product_large_values();
  test_get_element_projection();
  test_check_side_cases();
  test_size_and_dist();

  if (g_failed != 0)
  {
    std::cerr << "Vector unit tests FAILED: " << g_failed << " assertion(s).\n";
    return EXIT_FAILURE;
  }

  std::cout << "Vector unit tests PASSED.\n";
  return EXIT_SUCCESS;
}
