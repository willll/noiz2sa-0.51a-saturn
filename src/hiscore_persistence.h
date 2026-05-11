#pragma once

#include "attractmanager.h"

enum class HiScoreLoadStatus
{
  Loaded,
  NotFound,
  InvalidData,
  BackendUnavailable,
  IoError,
};

enum class HiScoreSaveStatus
{
  Saved,
  BackendUnavailable,
  IoError,
};

HiScoreLoadStatus loadHiScorePersistence(HiScore *outHiScore);
HiScoreSaveStatus saveHiScorePersistence(const HiScore *hiScore);
