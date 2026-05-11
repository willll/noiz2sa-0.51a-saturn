#include <srl_log.hpp>
#include <stdint.h>
#include <string.h>

#include <backup.hpp>

#include "attractmanager.h"
#include "hiscore_persistence.h"
#include "noiz2sa.h"

using namespace SRL::Backup;

namespace
{
constexpr uint32_t kHiScoreMagic = 0x48534352u; // "HSCR"
constexpr uint16_t kHiScoreFormatVersion = 1;

unsigned char kBackupFileName[13] = "NOIZ2SAHS";
unsigned char kBackupComment[11] = "HISCORE";

struct HiScorePayload
{
  int32_t stageScore[STAGE_NUM + ENDLESS_STAGE_NUM];
  int32_t sceneScore[STAGE_NUM][SCENE_NUM];
  int32_t stage;
};

struct HiScoreRecord
{
  uint32_t magic;
  uint16_t formatVersion;
  uint16_t reserved;
  uint32_t gameVersion;
  uint32_t payloadSize;
  uint32_t checksum;
  HiScorePayload payload;
};

static Device *gBackupDevice = nullptr;
static bool gBackupInitFailed = false;

static uint32_t fnv1a(const void *data, const uint32_t size)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t hash = 2166136261u;
  for (uint32_t i = 0; i < size; i++)
  {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

static void writePayloadFromHiScore(HiScorePayload *payload, const HiScore *hiScore)
{
  for (int i = 0; i < STAGE_NUM + ENDLESS_STAGE_NUM; i++)
  {
    payload->stageScore[i] = (int32_t)hiScore->stageScore[i];
  }
  for (int i = 0; i < STAGE_NUM; i++)
  {
    for (int j = 0; j < SCENE_NUM; j++)
    {
      payload->sceneScore[i][j] = (int32_t)hiScore->sceneScore[i][j];
    }
  }
  payload->stage = (int32_t)hiScore->stage;
}

static void readHiScoreFromPayload(HiScore *hiScore, const HiScorePayload *payload)
{
  for (int i = 0; i < STAGE_NUM + ENDLESS_STAGE_NUM; i++)
  {
    hiScore->stageScore[i] = (int)payload->stageScore[i];
  }
  for (int i = 0; i < STAGE_NUM; i++)
  {
    for (int j = 0; j < SCENE_NUM; j++)
    {
      hiScore->sceneScore[i][j] = (int)payload->sceneScore[i][j];
    }
  }
  hiScore->stage = (int)payload->stage;
}

static bool isDeviceMounted(const BupDevice device)
{
  return Device::BupState[device].isMounted;
}

static Device *getBackupDevice()
{
  if (gBackupInitFailed)
  {
    return nullptr;
  }

  if (gBackupDevice == nullptr)
  {
    gBackupDevice = new Device(kBackupFileName, kBackupComment, sizeof(HiScoreRecord), English);
    if (gBackupDevice == nullptr)
    {
      gBackupInitFailed = true;
      SRL::Logger::LogWarning("[HISCORE] Failed to allocate backup device wrapper");
      return nullptr;
    }
  }

  return gBackupDevice;
}

static HiScoreLoadStatus tryLoadFromDevice(Device *device, const BupDevice bupDevice, HiScore *outHiScore)
{
  HiScoreRecord record{};

  const int32_t readStatus = device->Read(bupDevice, kBackupFileName, (uint8_t *)&record);
  if (readStatus == NotFound)
  {
    return HiScoreLoadStatus::NotFound;
  }
  if (readStatus != Success)
  {
    return HiScoreLoadStatus::IoError;
  }

  if (record.magic != kHiScoreMagic ||
      record.formatVersion != kHiScoreFormatVersion ||
      record.payloadSize != sizeof(HiScorePayload) ||
      record.gameVersion != (uint32_t)VERSION_NUM)
  {
    SRL::Logger::LogWarning("[HISCORE] Invalid backup payload header (device=%d)", (int)bupDevice);
    return HiScoreLoadStatus::InvalidData;
  }

  const uint32_t expectedChecksum = fnv1a(&record.payload, (uint32_t)sizeof(record.payload));
  if (expectedChecksum != record.checksum)
  {
    SRL::Logger::LogWarning("[HISCORE] Backup checksum mismatch (device=%d)", (int)bupDevice);
    return HiScoreLoadStatus::InvalidData;
  }

  readHiScoreFromPayload(outHiScore, &record.payload);
  SRL::Logger::LogInfo("[HISCORE] Loaded hi-score backup from device=%d", (int)bupDevice);
  return HiScoreLoadStatus::Loaded;
}

static HiScoreSaveStatus trySaveToDevice(Device *device, const BupDevice bupDevice, const HiScore *hiScore)
{
  HiScoreRecord record{};
  record.magic = kHiScoreMagic;
  record.formatVersion = kHiScoreFormatVersion;
  record.gameVersion = (uint32_t)VERSION_NUM;
  record.payloadSize = (uint32_t)sizeof(HiScorePayload);
  writePayloadFromHiScore(&record.payload, hiScore);
  record.checksum = fnv1a(&record.payload, (uint32_t)sizeof(record.payload));

  const int32_t saveStatus = device->Save(bupDevice, &record, true);
  if (saveStatus != Success)
  {
    SRL::Logger::LogWarning("[HISCORE] Save failed on device=%d status=%d", (int)bupDevice, (int)saveStatus);
    return HiScoreSaveStatus::IoError;
  }

  SRL::Logger::LogInfo("[HISCORE] Saved hi-score backup to device=%d", (int)bupDevice);
  return HiScoreSaveStatus::Saved;
}
} // namespace

HiScoreLoadStatus loadHiScorePersistence(HiScore *outHiScore)
{
  if (outHiScore == nullptr)
  {
    return HiScoreLoadStatus::InvalidData;
  }

  Device *device = getBackupDevice();
  if (device == nullptr)
  {
    return HiScoreLoadStatus::BackendUnavailable;
  }

  const BupDevice candidates[] = {InternalMemoryBackup, CartridgeMemoryBackup};

  bool hasMountedDevice = false;
  bool sawNotFound = false;
  for (const BupDevice candidate : candidates)
  {
    if (!isDeviceMounted(candidate))
    {
      continue;
    }

    hasMountedDevice = true;
    const HiScoreLoadStatus status = tryLoadFromDevice(device, candidate, outHiScore);
    if (status == HiScoreLoadStatus::Loaded)
    {
      return status;
    }
    if (status == HiScoreLoadStatus::NotFound)
    {
      sawNotFound = true;
      continue;
    }
    if (status == HiScoreLoadStatus::InvalidData)
    {
      return status;
    }
  }

  if (!hasMountedDevice)
  {
    return HiScoreLoadStatus::BackendUnavailable;
  }

  return sawNotFound ? HiScoreLoadStatus::NotFound : HiScoreLoadStatus::IoError;
}

HiScoreSaveStatus saveHiScorePersistence(const HiScore *hiScore)
{
  if (hiScore == nullptr)
  {
    return HiScoreSaveStatus::IoError;
  }

  Device *device = getBackupDevice();
  if (device == nullptr)
  {
    return HiScoreSaveStatus::BackendUnavailable;
  }

  const BupDevice candidates[] = {InternalMemoryBackup, CartridgeMemoryBackup};

  bool hasMountedDevice = false;
  for (const BupDevice candidate : candidates)
  {
    if (!isDeviceMounted(candidate))
    {
      continue;
    }

    hasMountedDevice = true;
    if (trySaveToDevice(device, candidate, hiScore) == HiScoreSaveStatus::Saved)
    {
      return HiScoreSaveStatus::Saved;
    }
  }

  return hasMountedDevice ? HiScoreSaveStatus::IoError : HiScoreSaveStatus::BackendUnavailable;
}
