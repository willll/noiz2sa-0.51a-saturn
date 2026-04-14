#include <srl.hpp>
#include <srl_log.hpp>
#include <srl_memory.hpp>

#include "minunit.h"

#include "../../src/background.h"

using namespace SRL::Logger;

extern "C"
{
  const char *const strStart = "***UT_START***";
  const char *const strEnd = "***UT_END***";
}

MU_TEST(test_background_stage_initializes_boards)
{
  initBackground();
  setStageBackground(1);

  mu_check(getBackgroundDebugBoardCount() > 0);
  mu_check(getBackgroundDebugBitmapHash() != 0u);
}

MU_TEST(test_background_bitmap_changes_across_frames)
{
  initBackground();
  setStageBackground(1);

  const unsigned int initialHash = getBackgroundDebugBitmapHash();
  moveBackground();
  const unsigned int nextHash = getBackgroundDebugBitmapHash();
  moveBackground();
  const unsigned int thirdHash = getBackgroundDebugBitmapHash();

  mu_check(initialHash != 0u);
  mu_check(nextHash != 0u);
  mu_check(thirdHash != 0u);
  mu_check(initialHash != nextHash);
  mu_check(nextHash != thirdHash);
}

MU_TEST(test_background_multiple_stages_animate)
{
  const int stages[] = {0, 1, 2, 3, 4, 5, 6};
  for (unsigned int i = 0; i < sizeof(stages) / sizeof(stages[0]); i++)
  {
    initBackground();
    setStageBackground(stages[i]);
    const unsigned int before = getBackgroundDebugBitmapHash();
    moveBackground();
    const unsigned int after = getBackgroundDebugBitmapHash();

    mu_check(getBackgroundDebugBoardCount() > 0);
    mu_check(before != 0u);
    mu_check(after != 0u);
    mu_check(before != after);
  }
}

MU_TEST_SUITE(background_test_suite)
{
  MU_RUN_TEST(test_background_stage_initializes_boards);
  MU_RUN_TEST(test_background_bitmap_changes_across_frames);
  MU_RUN_TEST(test_background_multiple_stages_animate);
}

int main()
{
  SRL::Memory::Initialize();
  SRL::Core::Initialize(SRL::Types::HighColor(20, 10, 50));

  LogInfo(strStart);

  MU_RUN_SUITE(background_test_suite);
  MU_REPORT();

  LogInfo(strEnd);

  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
