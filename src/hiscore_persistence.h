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

/** @brief Loads the persistent high-score record from the backup backend. */
HiScoreLoadStatus loadHiScorePersistence(HiScore *outHiScore);
/** @brief Saves the persistent high-score record to the backup backend. */
HiScoreSaveStatus saveHiScorePersistence(const HiScore *hiScore);
