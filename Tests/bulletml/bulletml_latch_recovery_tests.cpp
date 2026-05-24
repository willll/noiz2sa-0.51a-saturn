#include <stdlib.h>
#include <stdio.h>

#include "bulletml_binary/bulletml_alloc_latch.h"

static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                                 \
  do                                                                                      \
  {                                                                                       \
    if (!(expr))                                                                          \
    {                                                                                     \
      printf("EXPECT_TRUE failed at line %d: %s\n", __LINE__, #expr);                   \
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
                  printf("EXPECT_EQ failed at line %d: %s != %s\n",                                 \
                    __LINE__, #a, #b);                                                          \
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

extern "C" int logic_test_main()
{
  test_latch_can_be_cleared_without_resetting_counter();
  test_repeated_failures_after_tick_clear_are_tracked();

  if (g_failed != 0)
  {
        printf("BulletML latch recovery tests FAILED: %d assertion(s).\n",
          g_failed);
    printf("***UT_END***\n");
    return 1;
  }

  printf("BulletML latch recovery tests PASSED.\n");
  printf("***UT_END***\n");
  return 0;
}
