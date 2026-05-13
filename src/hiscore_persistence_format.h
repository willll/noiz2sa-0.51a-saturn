#pragma once

#include <stdint.h>

#include "attractmanager.h"

struct HiScoreBackupStageEntry
{
  int32_t stageScore;
  int32_t sceneScore[SCENE_NUM];
};

// Binary layout mirrors the GP32/PC reference format from attractmanager.c:
//   int32_t version
//   for i in 0..STAGE_NUM-1:
//     int32_t stageScore[i]
//     int32_t sceneScore[i][0..SCENE_NUM-1]
//   int32_t stageScore[STAGE_NUM+0..ENDLESS_STAGE_NUM-1]
//   int32_t stage
struct HiScoreBackupData
{
  int32_t version;
  HiScoreBackupStageEntry stages[STAGE_NUM];
  int32_t endlessStageScore[ENDLESS_STAGE_NUM];
  int32_t stage;
};

/** @brief Encodes a HiScore structure into the on-disk backup layout. */
void encodeHiScoreBackupData(HiScoreBackupData *outData, const HiScore *hiScore);
/** @brief Decodes the on-disk backup layout into a HiScore structure. */
bool decodeHiScoreBackupData(const HiScoreBackupData *data, HiScore *outHiScore);
