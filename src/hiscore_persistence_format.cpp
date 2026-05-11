#include "hiscore_persistence_format.h"

#include "noiz2sa.h"

void encodeHiScoreBackupData(HiScoreBackupData *outData, const HiScore *hiScore)
{
  if (outData == nullptr || hiScore == nullptr)
  {
    return;
  }

  outData->version = VERSION_NUM;

  for (int i = 0; i < STAGE_NUM; i++)
  {
    outData->stages[i].stageScore = (int32_t)hiScore->stageScore[i];
    for (int j = 0; j < SCENE_NUM; j++)
    {
      outData->stages[i].sceneScore[j] = (int32_t)hiScore->sceneScore[i][j];
    }
  }
  for (int i = 0; i < ENDLESS_STAGE_NUM; i++)
  {
    outData->endlessStageScore[i] = (int32_t)hiScore->stageScore[STAGE_NUM + i];
  }
  outData->stage = (int32_t)hiScore->stage;
}

bool decodeHiScoreBackupData(const HiScoreBackupData *data, HiScore *outHiScore)
{
  if (data == nullptr || outHiScore == nullptr)
  {
    return false;
  }

  if (data->version != VERSION_NUM)
  {
    return false;
  }

  for (int i = 0; i < STAGE_NUM; i++)
  {
    outHiScore->stageScore[i] = (int)data->stages[i].stageScore;
    for (int j = 0; j < SCENE_NUM; j++)
    {
      outHiScore->sceneScore[i][j] = (int)data->stages[i].sceneScore[j];
    }
  }
  for (int i = 0; i < ENDLESS_STAGE_NUM; i++)
  {
    outHiScore->stageScore[STAGE_NUM + i] = (int)data->endlessStageScore[i];
  }
  outHiScore->stage = (int)data->stage;

  return true;
}
