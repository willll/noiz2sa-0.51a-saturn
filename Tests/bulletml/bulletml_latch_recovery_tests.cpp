#include <cstdlib>
#include <iostream>

#include "bulletml_binary/bulletml_alloc_latch.h"

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

static void test_latch_can_be_cleared_without_resetting_counter()
{
  resetBulletMlAllocFailureState();
  EXPECT_FALSE(hasBulletMlAllocFailureLatched());
  EXPECT_EQ(getBulletMlAllocFailureCount(), 0u);

  recordBulletMlAllocFailure();
  EXPECT_TRUE(hasBulletMlAllocFailureLatched());
  EXPECT_EQ(getBulletMlAllocFailureCount(), 1u);

  clearBulletMlAllocFailureLatch();
  EXPECT_FALSE(hasBulletMlAllocFailureLatched());
  EXPECT_EQ(getBulletMlAllocFailureCount(), 1u);
}

static void test_repeated_failures_after_tick_clear_are_tracked()
{
  resetBulletMlAllocFailureState();
  recordBulletMlAllocFailure();
  EXPECT_TRUE(hasBulletMlAllocFailureLatched());

  clearBulletMlAllocFailureLatch();
  EXPECT_FALSE(hasBulletMlAllocFailureLatched());

  recordBulletMlAllocFailure();
  EXPECT_TRUE(hasBulletMlAllocFailureLatched());
  EXPECT_EQ(getBulletMlAllocFailureCount(), 2u);
}

int main()
{
  test_latch_can_be_cleared_without_resetting_counter();
  test_repeated_failures_after_tick_clear_are_tracked();

  if (g_failed != 0)
  {
    std::cerr << "BulletML latch recovery tests FAILED: " << g_failed << " assertion(s).\n";
    return EXIT_FAILURE;
  }

  std::cout << "BulletML latch recovery tests PASSED.\n";
  return EXIT_SUCCESS;
}
