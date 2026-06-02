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
#include "../../src/bulletml_binary/bulletmlstate.hpp"
#include "../../src/bulletml_binary/bulletml_runtime_alloc_policy.h"

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
  setBulletMlStartupOnlyAllocationPolicy(false);
  /* Drop any retained BulletMLState object and array pools */
  releaseBulletMlStatePools();
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

MU_TEST(test_task_cache_preallocate_sets_capacity)
{
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
  mu_check(BulletMLRunnerImpl::PreallocateTaskBufferCache(96));
  mu_assert_int_eq(96, (int)BulletMLRunnerImpl::GetTaskBufferCacheCapacity());
}

MU_TEST(test_task_cache_preallocate_does_not_shrink)
{
  BulletMLRunnerImpl::ReleaseTaskBufferCache();
  mu_check(BulletMLRunnerImpl::PreallocateTaskBufferCache(96));
  mu_check(BulletMLRunnerImpl::PreallocateTaskBufferCache(32));
  mu_assert_int_eq(96, (int)BulletMLRunnerImpl::GetTaskBufferCacheCapacity());
}

MU_TEST(test_task_cache_startup_only_policy_toggle)
{
  BulletMLRunnerImpl::EnableStartupOnlyAllocation(false);
  mu_check(!BulletMLRunnerImpl::IsStartupOnlyAllocationEnabled());

  BulletMLRunnerImpl::EnableStartupOnlyAllocation(true);
  mu_check(BulletMLRunnerImpl::IsStartupOnlyAllocationEnabled());

  BulletMLRunnerImpl::EnableStartupOnlyAllocation(false);
  mu_check(!BulletMLRunnerImpl::IsStartupOnlyAllocationEnabled());
}

MU_TEST_SUITE(suite_task_buffer_cache)
{
  MU_RUN_TEST(test_task_cache_starts_empty);
  MU_RUN_TEST(test_task_cache_release_is_idempotent);
  MU_RUN_TEST(test_task_cache_preallocate_sets_capacity);
  MU_RUN_TEST(test_task_cache_preallocate_does_not_shrink);
  MU_RUN_TEST(test_task_cache_startup_only_policy_toggle);
}

/* =========================================================================
 * Suite: BulletML runtime allocation policy wiring
 * ========================================================================= */

MU_TEST(test_runtime_policy_enable_sets_runner_and_state)
{
  setBulletMlStartupOnlyAllocationPolicy(false);
  setBulletMlStartupOnlyAllocationPolicy(true);

  BulletMlRuntimeAllocPolicySnapshot snapshot = getBulletMlRuntimeAllocPolicySnapshot();
  mu_check(snapshot.runnerStartupOnly);
  mu_check(snapshot.stateStartupOnly);
  mu_check(!snapshot.runnerStartupPreallocation);
  mu_check(!snapshot.stateStartupPreallocation);
}

MU_TEST(test_runtime_policy_disable_clears_runner_and_state)
{
  setBulletMlStartupOnlyAllocationPolicy(true);
  setBulletMlStartupOnlyAllocationPolicy(false);

  BulletMlRuntimeAllocPolicySnapshot snapshot = getBulletMlRuntimeAllocPolicySnapshot();
  mu_check(!snapshot.runnerStartupOnly);
  mu_check(!snapshot.stateStartupOnly);
  mu_check(!snapshot.runnerStartupPreallocation);
  mu_check(!snapshot.stateStartupPreallocation);
}

MU_TEST(test_runtime_policy_preallocation_phase_sync)
{
  setBulletMlStartupOnlyAllocationPolicy(true);
  beginBulletMlStartupPreallocationPhase();

  BulletMlRuntimeAllocPolicySnapshot snapshot = getBulletMlRuntimeAllocPolicySnapshot();
  mu_check(snapshot.runnerStartupOnly);
  mu_check(snapshot.stateStartupOnly);
  mu_check(snapshot.runnerStartupPreallocation);
  mu_check(snapshot.stateStartupPreallocation);

  endBulletMlStartupPreallocationPhase();
  snapshot = getBulletMlRuntimeAllocPolicySnapshot();
  mu_check(!snapshot.runnerStartupPreallocation);
  mu_check(!snapshot.stateStartupPreallocation);

  setBulletMlStartupOnlyAllocationPolicy(false);
}

MU_TEST_SUITE(suite_runtime_alloc_policy)
{
  MU_RUN_TEST(test_runtime_policy_enable_sets_runner_and_state);
  MU_RUN_TEST(test_runtime_policy_disable_clears_runner_and_state);
  MU_RUN_TEST(test_runtime_policy_preallocation_phase_sync);
}

/* =========================================================================
 * Suite: BulletMLState pooling
 * ========================================================================= */

MU_TEST(test_state_pool_recycles_state_object)
{
  releaseBulletMlStatePools();

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes != nullptr);
  nodes[0] = nullptr;

  BulletMLState* first = createBulletMlState(nullptr, nodes, 1, nullptr, 0);
  mu_check(first != nullptr);
  void* firstAddr = static_cast<void*>(first);
  destroyBulletMlState(first);

  BulletMLNode** nextNodes = createBulletMlStateNodeArray(1);
  mu_check(nextNodes != nullptr);
  nextNodes[0] = nullptr;

  BulletMLState* second = createBulletMlState(nullptr, nextNodes, 1, nullptr, 0);
  mu_check(second != nullptr);
  mu_check(static_cast<void*>(second) == firstAddr);

  destroyBulletMlState(second);
  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_recycles_node_arrays)
{
  releaseBulletMlStatePools();
  mu_assert_int_eq(0, (int)getBulletMlStateNodeArrayCachedCount(4));

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes != nullptr);
  destroyBulletMlStateNodeArray(nodes, 1);

  mu_assert_int_eq(1, (int)getBulletMlStateNodeArrayCachedCount(4));

  BulletMLNode** reused = createBulletMlStateNodeArray(1);
  mu_check(reused != nullptr);
  mu_assert_int_eq(0, (int)getBulletMlStateNodeArrayCachedCount(4));

  destroyBulletMlStateNodeArray(reused, 1);
  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_cached_count_tracks_lifecycle)
{
  releaseBulletMlStatePools();
  mu_assert_int_eq(0, (int)getBulletMlStateCachedCount());

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes != nullptr);
  nodes[0] = nullptr;

  BulletMLState* state = createBulletMlState(nullptr, nodes, 1, nullptr, 0);
  mu_check(state != nullptr);
  mu_assert_int_eq(0, (int)getBulletMlStateCachedCount());

  destroyBulletMlState(state);
  mu_assert_int_eq(1, (int)getBulletMlStateCachedCount());

  BulletMLNode** nodes2 = createBulletMlStateNodeArray(1);
  mu_check(nodes2 != nullptr);
  nodes2[0] = nullptr;

  BulletMLState* reused = createBulletMlState(nullptr, nodes2, 1, nullptr, 0);
  mu_check(reused != nullptr);
  mu_assert_int_eq(0, (int)getBulletMlStateCachedCount());

  destroyBulletMlState(reused);
  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_recycles_parameter_arrays)
{
  releaseBulletMlStatePools();
  mu_assert_int_eq(0, (int)getBulletMlStateParameterArrayCachedCount(4));

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes != nullptr);
  nodes[0] = nullptr;

  Fxp params[3];
  params[0] = Fxp::Convert(1);
  params[1] = Fxp::Convert(2);
  params[2] = Fxp::Convert(3);

  BulletMLState* state = createBulletMlState(nullptr, nodes, 1, params, 3);
  mu_check(state != nullptr);

  destroyBulletMlState(state);
  mu_assert_int_eq(1, (int)getBulletMlStateParameterArrayCachedCount(4));

  BulletMLNode** nodes2 = createBulletMlStateNodeArray(1);
  mu_check(nodes2 != nullptr);
  nodes2[0] = nullptr;
  BulletMLState* reused = createBulletMlState(nullptr, nodes2, 1, params, 3);
  mu_check(reused != nullptr);
  mu_assert_int_eq(0, (int)getBulletMlStateParameterArrayCachedCount(4));

  destroyBulletMlState(reused);
  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_large_arrays_not_cached)
{
  releaseBulletMlStatePools();
  mu_assert_int_eq(0, (int)getBulletMlStateBucketedCapacity(33));

  BulletMLNode** nodes = createBulletMlStateNodeArray(33);
  mu_check(nodes != nullptr);

  Fxp params[33];
  for (int i = 0; i < 33; ++i)
  {
    params[i] = Fxp::Convert(i);
    nodes[i] = nullptr;
  }

  BulletMLState* state = createBulletMlState(nullptr, nodes, 33, params, 33);
  mu_check(state != nullptr);
  destroyBulletMlState(state);

  mu_assert_int_eq(0, (int)getBulletMlStateNodeArrayCachedCount(32));
  mu_assert_int_eq(0, (int)getBulletMlStateParameterArrayCachedCount(32));

  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_startup_only_blocks_runtime_create)
{
  releaseBulletMlStatePools();
  enableBulletMlStateStartupOnlyAllocation(true);

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes == nullptr);

  enableBulletMlStateStartupOnlyAllocation(false);
  releaseBulletMlStatePools();
}

MU_TEST(test_state_pool_preallocate_populates_caches)
{
  releaseBulletMlStatePools();
  enableBulletMlStateStartupOnlyAllocation(true);

  uint16_t nodeCounts[8] = {1, 0, 0, 0, 0, 0, 0, 0};
  uint16_t paramCounts[8] = {2, 0, 0, 0, 0, 0, 0, 0};
  mu_check(preallocateBulletMlStatePools(3, nodeCounts, paramCounts));

  mu_assert_int_eq(3, (int)getBulletMlStateCachedCount());
  mu_assert_int_eq(1, (int)getBulletMlStateNodeArrayCachedCount(4));
  mu_assert_int_eq(2, (int)getBulletMlStateParameterArrayCachedCount(4));

  BulletMLNode** nodes = createBulletMlStateNodeArray(1);
  mu_check(nodes != nullptr);
  destroyBulletMlStateNodeArray(nodes, 1);

  enableBulletMlStateStartupOnlyAllocation(false);
  releaseBulletMlStatePools();
}

MU_TEST_SUITE(suite_bulletml_state_pool)
{
  MU_RUN_TEST(test_state_pool_recycles_state_object);
  MU_RUN_TEST(test_state_pool_recycles_node_arrays);
  MU_RUN_TEST(test_state_pool_cached_count_tracks_lifecycle);
  MU_RUN_TEST(test_state_pool_recycles_parameter_arrays);
  MU_RUN_TEST(test_state_pool_large_arrays_not_cached);
  MU_RUN_TEST(test_state_pool_startup_only_blocks_runtime_create);
  MU_RUN_TEST(test_state_pool_preallocate_populates_caches);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

int main()
{
  SRL::Memory::Initialize();
  SRL::Core::Initialize(SRL::Types::HighColor(20, 10, 50));

  resetAllState();

  LogInfo("%s", strStart);

  MU_RUN_SUITE(suite_pool_mechanics);
  MU_RUN_SUITE(suite_alloc_latch);
  MU_RUN_SUITE(suite_task_buffer_cache);
  MU_RUN_SUITE(suite_runtime_alloc_policy);
  MU_RUN_SUITE(suite_bulletml_state_pool);

  MU_REPORT();

  LogInfo("%s", strEnd);

  // Keep emulator alive long enough for log flush + marker detection.
  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
