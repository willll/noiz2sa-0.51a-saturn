/**
 * Native unit tests for the FoeCommand free-list pool trim logic.
 *
 * These tests exercise the same intrusive free-list algorithm implemented in
 * src/foecommand.cc, using std::malloc / std::free so the tests run on the
 * host without the Saturn SRL libraries.
 *
 * Covered invariants:
 *  - Pool starts empty.
 *  - destroyFoeCommand pushes a node onto the free list and increments the count.
 *  - releaseFoeCommandPool drains all nodes and resets the count to zero.
 *  - trimFoeCommandPool(N) frees nodes until cachedCount <= N.
 *  - trimFoeCommandPool(0) releases everything.
 *  - trimFoeCommandPool larger than current pool count is a no-op.
 *  - getFoeCommandCachedCount returns the current free-list depth.
 */
#include <stddef.h>
#include <cstdlib>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Minimal standalone reimplementation of the FoeCommand pool
 * (mirrors the exact algorithm in src/foecommand.cc)
 * ========================================================================= */

namespace
{

struct FoeCommandFreeNode
{
  FoeCommandFreeNode* next;
};

static FoeCommandFreeNode* sFoeCommandFreeList = nullptr;
static std::size_t         sFoeCommandFreeCount = 0;

/* Push a raw block (simulating a destroyed FoeCommand) onto the free list. */
static void pushFreeNode(void* rawBlock)
{
  FoeCommandFreeNode* node = static_cast<FoeCommandFreeNode*>(rawBlock);
  node->next = sFoeCommandFreeList;
  sFoeCommandFreeList = node;
  sFoeCommandFreeCount++;
}

static std::size_t getFoeCommandCachedCount()
{
  return sFoeCommandFreeCount;
}

static void releaseFoeCommandPool()
{
  while (sFoeCommandFreeList)
  {
    FoeCommandFreeNode* node = sFoeCommandFreeList;
    sFoeCommandFreeList = node->next;
    std::free(node);
    if (sFoeCommandFreeCount > 0)
    {
      sFoeCommandFreeCount--;
    }
  }
  sFoeCommandFreeCount = 0;
}

static void trimFoeCommandPool(std::size_t maxCached)
{
  while (sFoeCommandFreeCount > maxCached && sFoeCommandFreeList)
  {
    FoeCommandFreeNode* node = sFoeCommandFreeList;
    sFoeCommandFreeList = node->next;
    std::free(node);
    sFoeCommandFreeCount--;
  }
}

/* Allocate a raw block (large enough for a FoeCommandFreeNode overlay) that
 * simulates a FoeCommand allocation. */
static void* allocNode()
{
  return std::malloc(sizeof(FoeCommandFreeNode) + 64 /* extra padding */);
}

} // namespace

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

#define EXPECT_LE(a, b)                                                       \
  do                                                                          \
  {                                                                           \
    const auto _a = (a);                                                      \
    const auto _b = (b);                                                      \
    if (!(_a <= _b))                                                          \
    {                                                                         \
      printf("EXPECT_LE failed at line %d: %s > %s\n", __LINE__, #a, #b);  \
      g_failed++;                                                             \
    }                                                                         \
  } while (0)

/* =========================================================================
 * Tests
 * ========================================================================= */

static void test_pool_starts_empty()
{
  releaseFoeCommandPool();
  EXPECT_EQ(getFoeCommandCachedCount(), 0u);
}

static void test_push_increments_count()
{
  releaseFoeCommandPool();

  void* a = allocNode();
  void* b = allocNode();
  pushFreeNode(a);
  EXPECT_EQ(getFoeCommandCachedCount(), 1u);
  pushFreeNode(b);
  EXPECT_EQ(getFoeCommandCachedCount(), 2u);

  releaseFoeCommandPool();
  EXPECT_EQ(getFoeCommandCachedCount(), 0u);
}

static void test_release_drains_all()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 5; ++i)
  {
    pushFreeNode(allocNode());
  }
  EXPECT_EQ(getFoeCommandCachedCount(), 5u);

  releaseFoeCommandPool();
  EXPECT_EQ(getFoeCommandCachedCount(), 0u);
}

static void test_trim_to_zero_releases_all()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 4; ++i)
  {
    pushFreeNode(allocNode());
  }

  trimFoeCommandPool(0);
  EXPECT_EQ(getFoeCommandCachedCount(), 0u);
  EXPECT_EQ(sFoeCommandFreeList, static_cast<FoeCommandFreeNode*>(nullptr));
}

static void test_trim_to_exact_count()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 6; ++i)
  {
    pushFreeNode(allocNode());
  }

  trimFoeCommandPool(3);
  EXPECT_EQ(getFoeCommandCachedCount(), 3u);

  releaseFoeCommandPool();
}

static void test_trim_larger_than_pool_is_noop()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 3; ++i)
  {
    pushFreeNode(allocNode());
  }

  trimFoeCommandPool(100);
  EXPECT_EQ(getFoeCommandCachedCount(), 3u); /* nothing freed */

  releaseFoeCommandPool();
}

static void test_trim_exactly_at_boundary()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 5; ++i)
  {
    pushFreeNode(allocNode());
  }

  trimFoeCommandPool(5); /* trim to exactly the current count — no-op */
  EXPECT_EQ(getFoeCommandCachedCount(), 5u);

  trimFoeCommandPool(4); /* remove exactly one */
  EXPECT_EQ(getFoeCommandCachedCount(), 4u);

  releaseFoeCommandPool();
}

static void test_cached_count_never_exceeds_trim_target()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 10; ++i)
  {
    pushFreeNode(allocNode());
  }

  const std::size_t targets[] = {8, 5, 2, 0};
  for (std::size_t t : targets)
  {
    trimFoeCommandPool(t);
    EXPECT_LE(getFoeCommandCachedCount(), t);
  }

  releaseFoeCommandPool();
}

static void test_release_after_trim_is_safe()
{
  releaseFoeCommandPool();

  for (int i = 0; i < 4; ++i)
  {
    pushFreeNode(allocNode());
  }

  trimFoeCommandPool(2);
  EXPECT_EQ(getFoeCommandCachedCount(), 2u);

  releaseFoeCommandPool();
  EXPECT_EQ(getFoeCommandCachedCount(), 0u);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

extern "C" int logic_test_main()
{
  test_pool_starts_empty();
  test_push_increments_count();
  test_release_drains_all();
  test_trim_to_zero_releases_all();
  test_trim_to_exact_count();
  test_trim_larger_than_pool_is_noop();
  test_trim_exactly_at_boundary();
  test_cached_count_never_exceeds_trim_target();
  test_release_after_trim_is_safe();

  if (g_failed != 0)
  {
    printf("FoeCommand trim tests FAILED: %d assertion(s).\n", g_failed);
    printf("***UT_END***\n");
    return 1;
  }

  printf("FoeCommand trim tests PASSED.\n");
  printf("***UT_END***\n");
  return 0;
}
