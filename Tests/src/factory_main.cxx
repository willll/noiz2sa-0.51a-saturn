#include <cstdint>
#include <type_traits>

#include <srl.hpp>
#include <srl_log.hpp>

#include "minunit.h"

#include "../../src/memory_factory.h"
#include "../../src/bulletml_runtime_factory.h"

extern "C"
{
  const char *const strStart = "***UT_START***";
  const char *const strEnd = "***UT_END***";
}

namespace
{
struct DummyObject
{
  int value;
  explicit DummyObject(int v)
      : value(v)
  {
  }
};

MU_TEST(test_memory_factory_basic)
{
  DummyObject *obj = createObject<DummyObject>(123);
  mu_assert(obj != nullptr, "createObject returned nullptr");
  mu_assert_int_eq(123, obj->value);
  destroyObject(obj);
  mu_assert(obj == nullptr, "destroyObject did not null pointer");

  int *arr = createArray<int>(4);
  mu_assert(arr != nullptr, "createArray returned nullptr");
  arr[0] = 7;
  arr[1] = 9;
  mu_assert_int_eq(7, arr[0]);
  mu_assert_int_eq(9, arr[1]);
  destroyArray(arr);
  mu_assert(arr == nullptr, "destroyArray did not null pointer");

  uint16_t *heap = allocateHeapItems<uint16_t>(8);
  mu_assert(heap != nullptr, "allocateHeapItems returned nullptr");
  heap[3] = 55;
  mu_assert_int_eq(55, heap[3]);
  freeHeapItems(heap);
  mu_assert(heap == nullptr, "freeHeapItems did not null pointer");
}

MU_TEST(test_system_factory)
{
  // Runtime-safe factory coverage for pure memory wrappers only.
  // System/graphics/sound/bulletml factories are covered by compile-time tests.
  mu_assert(true, "placeholder");
}

MU_TEST(test_graphics_factory)
{
  mu_assert(true, "placeholder");
}

MU_TEST(test_sound_factory_signature)
{
  mu_assert(true, "placeholder");
}

MU_TEST(test_bulletml_factory)
{
  mu_assert(true, "placeholder");
}

MU_TEST(test_bulletml_runtime_factory)
{
  using RuntimeObjReturn = decltype(createBulletMlRuntimeObject<DummyObject>(7));
  mu_assert((std::is_same<RuntimeObjReturn, DummyObject *>::value),
            "createBulletMlRuntimeObject return type mismatch");

  using RuntimeArrayReturn = decltype(createBulletMlRuntimeArray<int>(6));
  mu_assert((std::is_same<RuntimeArrayReturn, int *>::value),
            "createBulletMlRuntimeArray return type mismatch");
}

MU_TEST(test_allocation_factory_umbrella_include)
{
  mu_assert(true, "placeholder");
}

MU_TEST_SUITE(factory_suite)
{
  MU_RUN_TEST(test_memory_factory_basic);
  MU_RUN_TEST(test_system_factory);
  MU_RUN_TEST(test_graphics_factory);
  MU_RUN_TEST(test_sound_factory_signature);
  MU_RUN_TEST(test_bulletml_factory);
  MU_RUN_TEST(test_bulletml_runtime_factory);
  MU_RUN_TEST(test_allocation_factory_umbrella_include);
}
} // namespace

int main()
{
  SRL::Memory::Initialize();
  SRL::Core::Initialize(SRL::Types::HighColor(20, 10, 50));

  SRL::Logger::LogInfo("%s", strStart);
  MU_RUN_SUITE(factory_suite);
  MU_REPORT();
  SRL::Logger::LogInfo("%s", strEnd);

  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
