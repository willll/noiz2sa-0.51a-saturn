#include <srl_log.hpp>
#include <stdint.h>

#include <backup.hpp>

#include "attractmanager.h"
#include "hiscore_persistence.h"
#include "hiscore_persistence_format.h"

using namespace SRL::Backup;

namespace
{
unsigned char kBackupFileName[13] = "NOIZ2SAHS";
unsigned char kBackupComment[11] = "HISCORE";

static Device *gBackupDevice = nullptr;
static bool gBackupInitFailed = false;

static bool isDeviceMounted(const BupDevice device)
{
  return Device::BupState[device].isMounted;
}

static Device *getBackupDevice(bool allowInitialize)
{
  if (gBackupInitFailed)
  {
    return nullptr;
  }

  if (!allowInitialize)
  {
    return gBackupDevice;
  }

  if (gBackupDevice == nullptr)
  {
    gBackupDevice = new Device(kBackupFileName, kBackupComment, sizeof(HiScoreBackupData), English);
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
  HiScoreBackupData data{};

  const int32_t readStatus = device->Read(bupDevice, kBackupFileName, (uint8_t *)&data);
  if (readStatus == NotFound)
  {
    return HiScoreLoadStatus::NotFound;
  }
  if (readStatus != Success)
  {
    return HiScoreLoadStatus::IoError;
  }

  if (!decodeHiScoreBackupData(&data, outHiScore))
  {
    SRL::Logger::LogWarning("[HISCORE] Invalid hi-score backup payload on device=%d", (int)bupDevice);
    return HiScoreLoadStatus::InvalidData;
  }

  SRL::Logger::LogInfo("[HISCORE] Loaded hi-score from device=%d", (int)bupDevice);
  return HiScoreLoadStatus::Loaded;
}

static HiScoreSaveStatus trySaveToDevice(Device *device, const BupDevice bupDevice, const HiScore *hiScore)
{
  HiScoreBackupData data{};
  encodeHiScoreBackupData(&data, hiScore);

  const int32_t saveStatus = device->Save(bupDevice, &data, true);
  if (saveStatus != Success)
  {
    SRL::Logger::LogWarning("[HISCORE] Save failed on device=%d status=%d", (int)bupDevice, (int)saveStatus);
    return HiScoreSaveStatus::IoError;
  }

  SRL::Logger::LogInfo("[HISCORE] Saved hi-score to device=%d", (int)bupDevice);
  return HiScoreSaveStatus::Saved;
}
} // namespace

HiScoreLoadStatus loadHiScorePersistence(HiScore *outHiScore)
{
  if (outHiScore == nullptr)
  {
    return HiScoreLoadStatus::InvalidData;
  }

  Device *device = getBackupDevice(false);
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

  Device *device = getBackupDevice(true);
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
