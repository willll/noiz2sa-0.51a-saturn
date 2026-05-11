#include <cstdlib>
#include <iostream>

#include "attractmanager.h"
#include "hiscore_persistence_format.h"

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

static HiScore makeSampleHiScore()
{
  HiScore hi{};
  for (int i = 0; i < STAGE_NUM + ENDLESS_STAGE_NUM; i++)
  {
    hi.stageScore[i] = (i + 1) * 1111;
  }
  for (int i = 0; i < STAGE_NUM; i++)
  {
    for (int j = 0; j < SCENE_NUM; j++)
    {
      hi.sceneScore[i][j] = i * 100 + j;
    }
  }
  hi.stage = 7;
  return hi;
}

static void test_encode_then_decode_round_trip()
{
  const HiScore original = makeSampleHiScore();
  HiScoreBackupData data{};
  encodeHiScoreBackupData(&data, &original);

  HiScore decoded{};
  EXPECT_TRUE(decodeHiScoreBackupData(&data, &decoded));

  for (int i = 0; i < STAGE_NUM + ENDLESS_STAGE_NUM; i++)
  {
    EXPECT_EQ(decoded.stageScore[i], original.stageScore[i]);
  }
  for (int i = 0; i < STAGE_NUM; i++)
  {
    for (int j = 0; j < SCENE_NUM; j++)
    {
      EXPECT_EQ(decoded.sceneScore[i][j], original.sceneScore[i][j]);
    }
  }
  EXPECT_EQ(decoded.stage, original.stage);
}

static void test_decode_rejects_version_mismatch()
{
  const HiScore original = makeSampleHiScore();
  HiScoreBackupData data{};
  encodeHiScoreBackupData(&data, &original);

  data.version += 1;
  HiScore decoded{};
  EXPECT_FALSE(decodeHiScoreBackupData(&data, &decoded));
}

static void test_binary_layout_matches_gp32_sequence()
{
  const HiScore original = makeSampleHiScore();
  HiScoreBackupData data{};
  encodeHiScoreBackupData(&data, &original);

  const int32_t *words = reinterpret_cast<const int32_t *>(&data);

  int index = 0;
  EXPECT_EQ(words[index++], data.version);

  for (int i = 0; i < STAGE_NUM; i++)
  {
    EXPECT_EQ(words[index++], original.stageScore[i]);
    for (int j = 0; j < SCENE_NUM; j++)
    {
      EXPECT_EQ(words[index++], original.sceneScore[i][j]);
    }
  }

  for (int i = 0; i < ENDLESS_STAGE_NUM; i++)
  {
    EXPECT_EQ(words[index++], original.stageScore[STAGE_NUM + i]);
  }

  EXPECT_EQ(words[index++], original.stage);
}

int main()
{
  test_encode_then_decode_round_trip();
  test_decode_rejects_version_mismatch();
  test_binary_layout_matches_gp32_sequence();

  if (g_failed != 0)
  {
    std::cerr << "Hi-score persistence unit tests FAILED: " << g_failed << " assertion(s).\n";
    return EXIT_FAILURE;
  }

  std::cout << "Hi-score persistence unit tests PASSED.\n";
  return EXIT_SUCCESS;
}
