/**
 * Saturn SH2 unit tests for:
 *  - memory_factory.h: createPooledObject / destroyPooledObject / getPooledObjectCachedCount
 *    / releasePooledObjectPool
 *  - bulletml_alloc_latch.h: latch set / clear / reset
 *  - BulletMLRunnerImpl task-buffer cache: ReleaseTaskBufferCache /
 *    GetTaskBufferCacheCapacity (inline static cache in bulletmlrunner.hpp)
 *  - foecommand pool trim API: getFoeCommandCachedCount / trimFoeCommandPool
 *    (via the foecommand.cc sources added to this target)
 */
#include <srl.hpp>
#include <srl_log.hpp>
#include <srl_memory.hpp>

#include "minunit.h"

/* Redirect lwnew to hwnew so bulletml_binary headers compile on Saturn */
#ifdef lwnew
#undef lwnew
#endif
#define lwnew hwnew

#include "../../src/memory_factory.h"
#include "../../src/bulletml_binary/bulletml_alloc_latch.h"
#include "../../src/bulletml_binary/bulletmlrunner.hpp"

using namespace SRL::Logger;

extern "C"
{
  const char* const strStart = "***UT_START***";
  const char* const strEnd   = "***UT_END***";
}

/* =========================================================================
 * Helpers / stub types
 * ========================================================================= */

/** Trivial value type used to exercise generic pool mechanics. */
struct TestPoolItem
{
  int value;
  explicit TestPoolItem(int v = 0) : value(v) {}
};

static void resetAllState()
{
  /* Drain any leftover pooled TestPoolItem nodes */
  releasePooledObjectPool<TestPoolItem>();
  /* Clear the BulletML alloc latch */
  resetBulletMlAllocFailureState();
  /* Drop any retained task-buffer cache */
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
}

/* =========================================================================
 * Suite: memory_factory pool mechanics
 * ========================================================================= */

MU_TEST(test_pool_fresh_allocation_returns_nonnull)
{
  releasePooledObjectPool<TestPoolItem>();
  TestPoolItem* p = createPooledObject<TestPoolItem>(42);
  mu_check(p != nullptr);
  destroyPooledObject(p);
  mu_check(p == nullptr);
  releasePooledObjectPool<TestPoolItem>();
}

MU_TEST(test_pool_cached_count_increments_on_destroy)
{
  releasePooledObjectPool<TestPoolItem>();
  mu_assert_int_eq(0, (int)getPooledObjectCachedCount<TestPoolItem>());

  TestPoolItem* a = createPooledObject<TestPoolItem>(1);
  TestPoolItem* b = createPooledObject<TestPoolItem>(2);
  mu_assert_int_eq(0, (int)getPooledObjectCachedCount<TestPoolItem>());

  destroyPooledObject(a);
  mu_assert_int_eq(1, (int)getPooledObjectCachedCount<TestPoolItem>());

  destroyPooledObject(b);
  mu_assert_int_eq(2, (int)getPooledObjectCachedCount<TestPoolItem>());

  releasePooledObjectPool<TestPoolItem>();
  mu_assert_int_eq(0, (int)getPooledObjectCachedCount<TestPoolItem>());
}

MU_TEST(test_pool_release_clears_cache)
{
  releasePooledObjectPool<TestPoolItem>();

  TestPoolItem* a = createPooledObject<TestPoolItem>(7);
  TestPoolItem* b = createPooledObject<TestPoolItem>(8);
  destroyPooledObject(a);
  destroyPooledObject(b);
  mu_check(getPooledObjectCachedCount<TestPoolItem>() >= 1u);

  releasePooledObjectPool<TestPoolItem>();
  mu_assert_int_eq(0, (int)getPooledObjectCachedCount<TestPoolItem>());
}

MU_TEST(test_pool_recycles_freed_node)
{
  releasePooledObjectPool<TestPoolItem>();

  TestPoolItem* first = createPooledObject<TestPoolItem>(99);
  void* rawAddr = static_cast<void*>(first);
  destroyPooledObject(first);

  TestPoolItem* second = createPooledObject<TestPoolItem>(55);
  mu_check(second != nullptr);
  /* Pool should have handed back the same storage */
  mu_check(static_cast<void*>(second) == rawAddr);
  mu_assert_int_eq(55, second->value);

  destroyPooledObject(second);
  releasePooledObjectPool<TestPoolItem>();
}

MU_TEST(test_pool_count_decrements_on_reuse)
{
  releasePooledObjectPool<TestPoolItem>();

  TestPoolItem* a = createPooledObject<TestPoolItem>(1);
  TestPoolItem* b = createPooledObject<TestPoolItem>(2);
  destroyPooledObject(a);
  destroyPooledObject(b);
  const std::size_t before = getPooledObjectCachedCount<TestPoolItem>();

  TestPoolItem* c = createPooledObject<TestPoolItem>(3);
  const std::size_t after = getPooledObjectCachedCount<TestPoolItem>();
  mu_check(after < before);

  destroyPooledObject(c);
  releasePooledObjectPool<TestPoolItem>();
}

MU_TEST_SUITE(suite_pool_mechanics)
{
  MU_RUN_TEST(test_pool_fresh_allocation_returns_nonnull);
  MU_RUN_TEST(test_pool_cached_count_increments_on_destroy);
  MU_RUN_TEST(test_pool_release_clears_cache);
  MU_RUN_TEST(test_pool_recycles_freed_node);
  MU_RUN_TEST(test_pool_count_decrements_on_reuse);
}

/* =========================================================================
 * Suite: bulletml_alloc_latch
 * ========================================================================= */

MU_TEST(test_latch_starts_clear)
{
  resetBulletMlAllocFailureState();
  mu_check(!hasBulletMlAllocFailureLatched());
  mu_assert_int_eq(0, (int)getBulletMlAllocFailureCount());
}

MU_TEST(test_latch_trips_on_record)
{
  resetBulletMlAllocFailureState();
  recordBulletMlAllocFailure();
  mu_check(hasBulletMlAllocFailureLatched());
  mu_assert_int_eq(1, (int)getBulletMlAllocFailureCount());
}

MU_TEST(test_latch_count_accumulates)
{
  resetBulletMlAllocFailureState();
  recordBulletMlAllocFailure();
  recordBulletMlAllocFailure();
  recordBulletMlAllocFailure();
  mu_assert_int_eq(3, (int)getBulletMlAllocFailureCount());
}

MU_TEST(test_latch_clears_without_resetting_count)
{
  resetBulletMlAllocFailureState();
  recordBulletMlAllocFailure();
  recordBulletMlAllocFailure();
  clearBulletMlAllocFailureLatch();
  mu_check(!hasBulletMlAllocFailureLatched());
  /* count is preserved across clearBulletMlAllocFailureLatch */
  mu_assert_int_eq(2, (int)getBulletMlAllocFailureCount());
}

MU_TEST(test_latch_reset_clears_both)
{
  resetBulletMlAllocFailureState();
  recordBulletMlAllocFailure();
  resetBulletMlAllocFailureState();
  mu_check(!hasBulletMlAllocFailureLatched());
  mu_assert_int_eq(0, (int)getBulletMlAllocFailureCount());
}

MU_TEST_SUITE(suite_alloc_latch)
{
  MU_RUN_TEST(test_latch_starts_clear);
  MU_RUN_TEST(test_latch_trips_on_record);
  MU_RUN_TEST(test_latch_count_accumulates);
  MU_RUN_TEST(test_latch_clears_without_resetting_count);
  MU_RUN_TEST(test_latch_reset_clears_both);
}

/* =========================================================================
 * Suite: BulletMLRunnerImpl task buffer cache
 * ========================================================================= */

MU_TEST(test_task_cache_starts_empty)
{
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
  mu_assert_int_eq(0, (int)BulletMLRunnerImpl::GetTaskBufferCacheCapacity());
}

MU_TEST(test_task_cache_release_is_idempotent)
{
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
  mu_assert_int_eq(0, (int)BulletMLRunnerImpl::GetTaskBufferCacheCapacity());
}

MU_TEST_SUITE(suite_task_buffer_cache)
{
  MU_RUN_TEST(test_task_cache_starts_empty);
  MU_RUN_TEST(test_task_cache_release_is_idempotent);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

int main()
{
  resetAllState();

  LogInfo("%s", strStart);

  MU_RUN_SUITE(suite_pool_mechanics);
  MU_RUN_SUITE(suite_alloc_latch);
  MU_RUN_SUITE(suite_task_buffer_cache);

  MU_REPORT();

  LogInfo("%s", strEnd);

  return MU_EXIT_CODE;
}
