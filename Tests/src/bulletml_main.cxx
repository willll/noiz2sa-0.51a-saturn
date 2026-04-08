#include <srl.hpp>
#include <srl_log.hpp>
#include <srl_cd.hpp>
#include <srl_memory.hpp>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "minunit.h"

#ifdef lwnew
#undef lwnew
#endif
#define lwnew hwnew

#include "../../src/bulletml_binary/bulletmlparser_blb.hpp"

using namespace SRL::Logger;
using namespace SRL::Types;

extern "C"
{
  const char *const strStart = "***UT_START***";
  const char *const strEnd = "***UT_END***";
}

namespace
{
constexpr uint32_t kFnvOffset = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;
constexpr int kMaxExpected = 96;
constexpr int kMaxNameLen = 40;
constexpr int kMaxManifestSize = 16384;
constexpr int kMaxListSize = 4096;
constexpr int kMaxPatternSize = 65536;

struct ExpectedEntry
{
  char dir[8];
  char name[kMaxNameLen];
  uint32_t hash;
  uint16_t nodeCount;
  uint16_t topActions;
  uint8_t orientation;
};

ExpectedEntry g_expected[kMaxExpected];
int g_expectedCount = 0;
uint8_t g_patternBuffer[kMaxPatternSize];

inline void fnvUpdateByte(uint32_t &h, uint8_t b)
{
  h ^= b;
  h *= kFnvPrime;
}

inline void fnvUpdateU16(uint32_t &h, uint16_t v)
{
  fnvUpdateByte(h, (uint8_t)(v & 0xFF));
  fnvUpdateByte(h, (uint8_t)((v >> 8) & 0xFF));
}

inline void fnvUpdateU32(uint32_t &h, uint32_t v)
{
  fnvUpdateByte(h, (uint8_t)(v & 0xFF));
  fnvUpdateByte(h, (uint8_t)((v >> 8) & 0xFF));
  fnvUpdateByte(h, (uint8_t)((v >> 16) & 0xFF));
  fnvUpdateByte(h, (uint8_t)((v >> 24) & 0xFF));
}

inline void fnvUpdateStr(uint32_t &h, const char *s)
{
  if (s)
  {
    for (int i = 0; s[i] != '\0'; ++i)
    {
      fnvUpdateByte(h, (uint8_t)s[i]);
    }
  }
  fnvUpdateByte(h, 0);
}

uint32_t hashNodeRecursive(const BulletMLNode *node, uint16_t &nodeCount)
{
  uint32_t h = kFnvOffset;

  const uint8_t valueType = (uint8_t)node->getType();
  const uint16_t childCount = (uint16_t)node->getChildCount();
  const uint32_t refId = node->getRefID();

  fnvUpdateStr(h, node->getName());
  fnvUpdateByte(h, valueType);
  fnvUpdateU16(h, childCount);
  fnvUpdateU32(h, refId);
  fnvUpdateStr(h, node->getValue());
  fnvUpdateStr(h, node->getLabel());

  nodeCount = 1;
  for (uint16_t i = 0; i < childCount; ++i)
  {
    BulletMLNode *child = node->getChild(i);
    if (!child)
    {
      continue;
    }
    uint16_t childNodes = 0;
    const uint32_t childHash = hashNodeRecursive(child, childNodes);
    nodeCount = (uint16_t)(nodeCount + childNodes);
    fnvUpdateU32(h, childHash);
  }

  return h;
}

bool loadFileToBuffer(const char *path, uint8_t *buffer, int capacity, int &sizeOut)
{
  SRL::Cd::File file(path);
  if (!file.Exists())
  {
    LogFatal("Missing file: %s", path);
    return false;
  }

  int32_t bytesToRead = (int32_t)file.Size.Bytes;
  if (bytesToRead <= 0 || bytesToRead > capacity)
  {
    LogFatal("Invalid file size for %s: %d", path, bytesToRead);
    return false;
  }

  int32_t bytesRead = file.LoadBytes(0, bytesToRead, buffer);
  if (bytesRead <= 0)
  {
    LogFatal("Failed to read file: %s", path);
    return false;
  }

  sizeOut = bytesRead;
  return true;
}

ExpectedEntry *findExpected(const char *dir, const char *name)
{
  for (int i = 0; i < g_expectedCount; ++i)
  {
    if (strcmp(g_expected[i].dir, dir) == 0 && strcmp(g_expected[i].name, name) == 0)
    {
      return &g_expected[i];
    }
  }
  return nullptr;
}

bool loadExpectedManifest()
{
  uint8_t buffer[kMaxManifestSize];
  int size = 0;

  SRL::Cd::ChangeDir((char *)nullptr);
  if (!loadFileToBuffer("BLBEXP.TXT", buffer, kMaxManifestSize, size))
  {
    return false;
  }

  g_expectedCount = 0;
  char line[128];
  int lineLen = 0;

  for (int i = 0; i <= size; ++i)
  {
    const char c = (i == size) ? '\n' : (char)buffer[i];
    if (c == '\n' || c == '\r')
    {
      if (lineLen == 0)
      {
        continue;
      }
      line[lineLen] = '\0';
      lineLen = 0;

      if (g_expectedCount >= kMaxExpected)
      {
        LogFatal("Manifest exceeds max entries (%d)", kMaxExpected);
        return false;
      }

      char *parts[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
      int partIdx = 0;
      char *cursor = line;
      parts[partIdx++] = cursor;
      while (*cursor != '\0' && partIdx < 6)
      {
        if (*cursor == '|')
        {
          *cursor = '\0';
          parts[partIdx++] = cursor + 1;
        }
        ++cursor;
      }

      if (partIdx != 6)
      {
        LogFatal("Invalid manifest line: %s", line);
        return false;
      }

      ExpectedEntry &e = g_expected[g_expectedCount++];
      strncpy(e.dir, parts[0], sizeof(e.dir) - 1);
      e.dir[sizeof(e.dir) - 1] = '\0';
      strncpy(e.name, parts[1], sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      e.hash = (uint32_t)strtoul(parts[2], nullptr, 16);
      e.nodeCount = (uint16_t)atoi(parts[3]);
      e.topActions = (uint16_t)atoi(parts[4]);
      e.orientation = (uint8_t)atoi(parts[5]);
      continue;
    }

    if (lineLen < (int)sizeof(line) - 1)
    {
      line[lineLen++] = c;
    }
  }

  return g_expectedCount > 0;
}

bool forEachListEntry(const char *dir, bool (*callback)(const char *dir, const char *filename))
{
  uint8_t buffer[kMaxListSize];
  int size = 0;

  SRL::Cd::ChangeDir((char *)nullptr);
  if (SRL::Cd::ChangeDir(dir) < 0)
  {
    LogFatal("Cannot change to dir: %s", dir);
    return false;
  }

  if (!loadFileToBuffer("LIST.TXT", buffer, kMaxListSize, size))
  {
    return false;
  }

  char line[48];
  int lineLen = 0;

  for (int i = 0; i <= size; ++i)
  {
    const char c = (i == size) ? '\n' : (char)buffer[i];
    if (c == '\n' || c == '\r')
    {
      if (lineLen == 0)
      {
        continue;
      }
      while (lineLen > 0 && (line[lineLen - 1] == ' ' || line[lineLen - 1] == '\t'))
      {
        lineLen--;
      }
      line[lineLen] = '\0';
      lineLen = 0;

      if (line[0] != '\0')
      {
        if (!callback(dir, line))
        {
          return false;
        }
      }
      continue;
    }

    if (lineLen < (int)sizeof(line) - 1)
    {
      line[lineLen++] = c;
    }
  }

  return true;
}

int g_verified = 0;

bool verifyOnePattern(const char *dir, const char *filename)
{
  ExpectedEntry *expected = findExpected(dir, filename);
  if (!expected)
  {
    LogFatal("Missing expected entry for %s/%s", dir, filename);
    return false;
  }

  int patternSize = 0;
  if (!loadFileToBuffer(filename, g_patternBuffer, kMaxPatternSize, patternSize))
  {
    LogFatal("Unable to read pattern bytes for %s/%s", dir, filename);
    return false;
  }

  if (patternSize < 28)
  {
    LogFatal("Pattern too small for header: %s/%s size=%d", dir, filename, patternSize);
    return false;
  }

  const uint32_t stringOffset =
      (uint32_t)g_patternBuffer[8] |
      ((uint32_t)g_patternBuffer[9] << 8) |
      ((uint32_t)g_patternBuffer[10] << 16) |
      ((uint32_t)g_patternBuffer[11] << 24);
  if (stringOffset + 4 > (uint32_t)patternSize)
  {
    LogFatal("Invalid string offset for %s/%s: %u (size=%d)", dir, filename, stringOffset, patternSize);
    return false;
  }
  const uint32_t rawStringCount =
      (uint32_t)g_patternBuffer[stringOffset + 0] |
      ((uint32_t)g_patternBuffer[stringOffset + 1] << 8) |
      ((uint32_t)g_patternBuffer[stringOffset + 2] << 16) |
      ((uint32_t)g_patternBuffer[stringOffset + 3] << 24);
  if (rawStringCount > 512)
  {
    LogFatal("Suspicious string count for %s/%s: %u", dir, filename, rawStringCount);
    return false;
  }
  if (rawStringCount > 128)
  {
    LogFatal("String count exceeds parser max for %s/%s: %u", dir, filename, rawStringCount);
    return false;
  }

  BulletMLParserBLB parser(g_patternBuffer, (uint32_t)patternSize);
  if (!parser.build())
  {
    LogFatal("Parser build failed for %s/%s", dir, filename);
    return false;
  }

  BulletMLNode *root = parser.getRoot();
  if (!root)
  {
    LogFatal("Parser root is null for %s/%s", dir, filename);
    return false;
  }

  uint16_t nodeCount = 0;
  const uint32_t hash = hashNodeRecursive(root, nodeCount);

  uint32_t topCount32 = 0;
  parser.getTopActions(&topCount32);
  const uint16_t topCount = (uint16_t)topCount32;

  const uint8_t orientation = parser.isHorizontal() ? 1 : 0;

  if (hash != expected->hash)
  {
    LogFatal("Hash mismatch for %s/%s: got %08X expected %08X", dir, filename, hash, expected->hash);
    return false;
  }
  if (nodeCount != expected->nodeCount)
  {
    LogFatal("Node-count mismatch for %s/%s: got %u expected %u", dir, filename, nodeCount, expected->nodeCount);
    return false;
  }
  if (topCount != expected->topActions)
  {
    LogFatal("Top-action mismatch for %s/%s: got %u expected %u", dir, filename, topCount, expected->topActions);
    return false;
  }
  if (orientation != expected->orientation)
  {
    LogFatal("Orientation mismatch for %s/%s: got %u expected %u", dir, filename, orientation, expected->orientation);
    return false;
  }

  g_verified++;
  LogTrace("Verified %s/%s", dir, filename);
  return true;
}

MU_TEST(test_bulletml_manifest_covered_and_equal)
{
  mu_check(loadExpectedManifest());

  g_verified = 0;

  mu_check(forEachListEntry("BOSS", verifyOnePattern));
  mu_check(forEachListEntry("MIDDLE", verifyOnePattern));
  mu_check(forEachListEntry("ZAKO", verifyOnePattern));

  mu_assert_int_eq(g_expectedCount, g_verified);
  mu_assert_int_eq(73, g_verified);
}

MU_TEST_SUITE(bulletml_sh2_suite)
{
  MU_RUN_TEST(test_bulletml_manifest_covered_and_equal);
}

} // namespace

int main()
{
  SRL::Memory::Initialize();
  SRL::Core::Initialize(HighColor(20, 10, 50));

  LogInfo("Memory free HWRAM=%u LWRAM=%u", (uint32_t)SRL::Memory::GetFreeSpace(SRL::Memory::Zone::HWRam), (uint32_t)SRL::Memory::GetFreeSpace(SRL::Memory::Zone::LWRam));

  LogInfo(strStart);

  MU_RUN_SUITE(bulletml_sh2_suite);
  MU_REPORT();

  LogInfo(strEnd);

  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
