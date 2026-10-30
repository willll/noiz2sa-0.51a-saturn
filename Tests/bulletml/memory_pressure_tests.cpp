/**
 * Native unit tests for the forced BulletML latch-recovery rule,
 * isForcedLatchClearDue() in src/bulletml_binary/bulletml_alloc_latch.h,
 * which shouldClearBulletMlAllocFailureLatch() in noiz2sa.cpp calls.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bulletml_binary/bulletml_alloc_latch.h"

/* =========================================================================
 * Minimal test harness
 * ========================================================================= */

static int g_failed = 0;

#define EXPECT_EQ(a, b)                                                       \
  do                                                                          \
  {                                                                           \
    const auto _a = (a);                                                      \
    const auto _b = (b);                                                      \
    if (_a != _b)                                                             \
    {                                                                         \
      printf("EXPECT_EQ failed at line %d: %s != %s\n", __LINE__, #a, #b); \
      g_failed++;                                                             \
    }                                                                         \
  } while (0)

#define EXPECT_TRUE(expr)                                                     \
  do                                                                          \
  {                                                                           \
    if (!(expr))                                                              \
    {                                                                         \
      printf("EXPECT_TRUE failed at line %d: %s\n", __LINE__, #expr);      \
      g_failed++;                                                             \
    }                                                                         \
  } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

/* =========================================================================
 * Tests: forced latch recovery
 * ========================================================================= */

static void test_forced_clear_not_before_300_ticks()
{
  /* Even with few projectiles, forced clear must not fire before 300 ticks */
  EXPECT_FALSE(isForcedLatchClearDue(false, 0,   0,   16000));
  EXPECT_FALSE(isForcedLatchClearDue(false, 100, 0,   16000));
  EXPECT_FALSE(isForcedLatchClearDue(false, 299, 0,   16000));
}

static void test_forced_clear_fires_at_300_ticks_with_few_projectiles()
{
  /* Exactly 300 ticks with ≤ 64 projectiles and adequate hwFree → must clear */
  EXPECT_TRUE(isForcedLatchClearDue(false, 300, 0,   16000));
  EXPECT_TRUE(isForcedLatchClearDue(false, 300, 64,  16000));
  EXPECT_TRUE(isForcedLatchClearDue(false, 500, 10,  16000));
}

static void test_300_tick_rule_does_not_fire_with_too_many_projectiles()
{
  /* 300+ ticks but > 64 live projectiles: the 300-tick rule should NOT fire.
   * (The 900-tick rule would still fire once age >= 900.) */
  EXPECT_FALSE(isForcedLatchClearDue(false, 300, 65,  16000));
  EXPECT_FALSE(isForcedLatchClearDue(false, 500, 100, 16000));
  EXPECT_FALSE(isForcedLatchClearDue(false, 899, 200, 16000));
}

static void test_forced_clear_requires_minimum_hwfree()
{
  /* Even at 300+ ticks with few projectiles, forced clear requires >= 8KB hwfree.
   * Below that, recovery would likely crash, so we wait for normal retry path. */
  EXPECT_FALSE(isForcedLatchClearDue(false, 300, 0,   4096));  /* < 8KB → no recovery */
  EXPECT_FALSE(isForcedLatchClearDue(false, 300, 0,   8191));  /* < 8KB → no recovery */
  EXPECT_TRUE (isForcedLatchClearDue(false, 300, 0,   8192));  /* == 8KB → recovery OK */
  EXPECT_TRUE (isForcedLatchClearDue(false, 300, 0,   16000)); /* >= 8KB → recovery OK */
}

static void test_900_tick_rule_also_requires_minimum_hwfree()
{
  /* The 900-tick unconditional rule also requires >= 8KB hwfree */
  EXPECT_FALSE(isForcedLatchClearDue(false, 900, 200, 4096));  /* < 8KB → no recovery */
  EXPECT_TRUE (isForcedLatchClearDue(false, 900, 200, 8192));  /* >= 8KB → recovery OK */
}

static void test_forced_clear_never_fires_on_title()
{
  /* Title screen should never trigger forced recovery */
  EXPECT_FALSE(isForcedLatchClearDue(true, 300,  0,   16000));
  EXPECT_FALSE(isForcedLatchClearDue(true, 900,  0,   16000));
  EXPECT_FALSE(isForcedLatchClearDue(true, 9000, 0,   16000));
}

static void test_boundary_300_ticks_exactly()
{
  EXPECT_FALSE(isForcedLatchClearDue(false, 299, 64, 16000));
  EXPECT_TRUE (isForcedLatchClearDue(false, 300, 64, 16000));
}

static void test_boundary_64_projectiles()
{
  EXPECT_TRUE (isForcedLatchClearDue(false, 300, 64, 16000));
  EXPECT_FALSE(isForcedLatchClearDue(false, 300, 65, 16000));
}

static void test_boundary_900_ticks_exactly()
{
  EXPECT_FALSE(isForcedLatchClearDue(false, 899, 65, 16000)); /* not yet */
  EXPECT_TRUE (isForcedLatchClearDue(false, 900, 65, 16000)); /* exactly at boundary */
}

/**
 * Regression: forced recovery must NOT be gated by sRetryTick AND must
 * require minimum hwFree.
 *
 * When repeated allocation failures keep pushing sRetryTick forward (each
 * fail adds 90 ticks of backoff), the old code evaluated:
 *   if (tick < sRetryTick) return false;   ← blocks forced recovery too!
 * before the 300/900-tick forced-clear clauses.
 *
 * The fix reorders the checks so the forced-clear paths are evaluated first,
 * AND enforces a minimum hwFree (8KB) to avoid allocating in a crash scenario.
 * This test documents the intended semantics: isForcedLatchClearDue()
 * returns true at age 300 with 0 live projectiles and adequate hwFree,
 * regardless of any backoff state.
 */
static void test_forced_clear_not_blocked_by_retry_backoff_with_adequate_memory()
{
  /* Scenario: latch started at tick T, new fails at T+90, T+180, T+270,
   * T+360 — each pushing sRetryTick 90 ticks out.  At age=300 with adequate
   * hwFree (>= 8KB) the forced rule must still fire (live=0). */
  EXPECT_TRUE(isForcedLatchClearDue(false, 300, 0,   16000));
  EXPECT_TRUE(isForcedLatchClearDue(false, 360, 0,   16000));
  EXPECT_TRUE(isForcedLatchClearDue(false, 450, 0,   16000));

  /* Similarly the 900-tick unconditional rule fires even with many projectiles
   * that would prevent the 300-tick path, as long as hwFree is sufficient. */
  EXPECT_TRUE(isForcedLatchClearDue(false, 900, 200, 16000));
}

/**
 * Regression: forced recovery is suppressed if hwFree drops below the safe floor.
 *
 * At tick 5421 in the original hardware test, hwfree was ~4KB.  The 300-tick
 * forced-recovery rule fired and tried to allocate BulletML state, got nullptr,
 * and froze.  The fix adds a minimum hwFree floor (8KB) so that forced recovery
 * waits for the normal retry path when memory is too tight to safely allocate.
 */
static void test_forced_clear_suppressed_below_hwfree_floor()
{
  /* With low hwFree, even at age=300 with live=0, no forced recovery */
  EXPECT_FALSE(isForcedLatchClearDue(false, 300, 0,   4096));  /* < 8KB floor */
  EXPECT_FALSE(isForcedLatchClearDue(false, 360, 0,   7000));  /* < 8KB floor */
  
  /* The 900-tick rule is also suppressed below the floor */
  EXPECT_FALSE(isForcedLatchClearDue(false, 900, 100, 4096));  /* < 8KB floor */
  
  /* At or above the floor, forced recovery is allowed */
  EXPECT_TRUE (isForcedLatchClearDue(false, 300, 0,   8192));  /* >= 8KB floor */
  EXPECT_TRUE (isForcedLatchClearDue(false, 900, 100, 8192));  /* >= 8KB floor */
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

extern "C" int logic_test_main()
{
  /* -- forced latch recovery tests -- */
  test_forced_clear_not_before_300_ticks();
  test_forced_clear_fires_at_300_ticks_with_few_projectiles();
  test_300_tick_rule_does_not_fire_with_too_many_projectiles();
  test_forced_clear_requires_minimum_hwfree();
  test_900_tick_rule_also_requires_minimum_hwfree();
  test_forced_clear_never_fires_on_title();
  test_boundary_300_ticks_exactly();
  test_boundary_64_projectiles();
  test_boundary_900_ticks_exactly();
  test_forced_clear_not_blocked_by_retry_backoff_with_adequate_memory();
  test_forced_clear_suppressed_below_hwfree_floor();

  if (g_failed != 0)
  {
    printf("Memory pressure policy tests FAILED: %d assertion(s).\n",
           g_failed);
    printf("***UT_END***\n");
    return 1;
  }

  printf("Memory pressure policy tests PASSED.\n");
  printf("***UT_END***\n");
  return 0;
}
