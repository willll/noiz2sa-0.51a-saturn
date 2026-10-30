/*
 * $Id: foecommand.cc,v 1.2 2003/08/10 04:09:46 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Handle bullet commands.
 *
 * @version $Revision: 1.2 $
 */
#include "bulletml_binary/bulletmlparser_blb.hpp"
#include "foe.h"
#include <cstdint>
#include "foecommand.h"
#include <srl_log.hpp>
#include <srl_memory.hpp>

#include "bulletml_runtime_factory.h"
#include "noiz2sa.h"
#include "degutil.h"
#include "ship.h"
#include "bulletml_binary/bulletml_alloc_latch.h"
#include "memory_factory.h"

#include <new>

#define COMMAND_SCREEN_SPD_RATE (800 / 2)
#define COMMAND_SCREEN_VEL_RATE (800 / 2)

/** @brief Converts fixed-point values to legacy signed integers with rounding. */
static int fxpToLegacyInt(Fxp value)
{
  int result = value.As<int>();
  if (value.RawValue() < 0 && (value.RawValue() & 0xFFFF) != 0)
  {
    result += 1;
  }
  return result;
}

/** @brief Converts a fixed-point value to a legacy integer scaled by the given factor. */
static int fxpToLegacyScaledInt(Fxp value, int scale)
{
  const int64_t scaledRaw = static_cast<int64_t>(value.RawValue()) * scale;
  if (scaledRaw >= 0)
  {
    return static_cast<int>((scaledRaw + 0x8000) >> 16);
  }
  return -static_cast<int>(((-scaledRaw) + 0x8000) >> 16);
}

/** @brief Converts a fixed-point direction into the legacy signed direction index space. */
static int fxpDirectionToLegacySigned(Fxp direction)
{
  const int64_t scaledRaw = static_cast<int64_t>(direction.RawValue()) * DIV / 360;
  return static_cast<int>(scaledRaw / 65536);
}

/** @brief Converts a fixed-point direction into a wrapped legacy direction index. */
static int fxpDirectionToLegacyWrapped(Fxp direction)
{
  int d = fxpDirectionToLegacySigned(direction);
  d &= (DIV - 1);
  return d;
}

/** @brief Converts a legacy direction index back into fixed-point degrees. */
static Fxp legacyDirectionIndexToFxpDegrees(int direction)
{
  const int64_t raw = static_cast<int64_t>(direction) * 360 * 65536 / DIV;
  return Fxp::BuildRaw(static_cast<int32_t>(raw));
}

namespace
{
// Fixed pool: one HWRAM block of FoeCommand slots plus a separate stack of free
// slot indices. The bookkeeping lives outside the objects, so a late write into
// a recycled FoeCommand can no longer break the pool itself.
static constexpr std::size_t kFoeCommandPoolMaxCapacity = 512;
static FoeCommand* sFoeCommandPool = nullptr;
static uint16_t sFoeCommandPoolSize = 0;
static uint16_t sFoeCommandFreeSlots[kFoeCommandPoolMaxCapacity];
static uint16_t sFoeCommandFreeTop = 0;
static bool sFoeCommandSlotInUse[kFoeCommandPoolMaxCapacity];

/** @brief Returns the pool slot index of cmd, or -1 if cmd is not a pool slot. */
static int getFoeCommandSlot(const FoeCommand* cmd)
{
  if (!sFoeCommandPool)
  {
    return -1;
  }
  const uintptr_t base = reinterpret_cast<uintptr_t>(sFoeCommandPool);
  const uintptr_t p = reinterpret_cast<uintptr_t>(cmd);
  if (p < base || (p - base) % sizeof(FoeCommand) != 0)
  {
    return -1;
  }
  const uintptr_t slot = (p - base) / sizeof(FoeCommand);
  return (slot < sFoeCommandPoolSize) ? static_cast<int>(slot) : -1;
}
}

template <typename... Args>
/** @brief Takes a FoeCommand from the fixed pool; never allocates. */
static FoeCommand* createFoeCommandFromPool(Args&&... args)
{
  if (sFoeCommandFreeTop == 0)
  {
    // Pool exhausted: this is spawn suppression, not an error. Callers already
    // treat a null return as "spawn skipped this tick" (see addFoe /
    // addFoeActiveBullet in foe.cc).
    static std::size_t sPoolExhaustedLogs = 0;
    if (sPoolExhaustedLogs < 12 || (sPoolExhaustedLogs % 64) == 0)
    {
      SRL::Logger::LogInfo("[FOECMD_POOL] pool exhausted; suppressing spawn size=%u",
                           (unsigned int)sFoeCommandPoolSize);
    }
    ++sPoolExhaustedLogs;
    return nullptr;
  }

  const uint16_t slot = sFoeCommandFreeSlots[--sFoeCommandFreeTop];
  sFoeCommandSlotInUse[slot] = true;
  return new (&sFoeCommandPool[slot]) FoeCommand(std::forward<Args>(args)...);
}

/** @brief Constructs a command runner bound to a BulletML parser. */
FoeCommand::FoeCommand(BulletMLParserBLB *parser, Foe *f)
  : BulletMLRunner(parser) {
  foe = f;
}

/** @brief Constructs a command runner bound to an existing BulletML state. */
FoeCommand::FoeCommand(BulletMLState *state, Foe *f)
  : BulletMLRunner(state) {
  foe = f;
}

/** @brief Destroys the command runner. */
FoeCommand::~FoeCommand() {
}

/** @brief Creates a FoeCommand for a parser-backed foe. */
FoeCommand* createFoeCommand(BulletMLParserBLB* parser, Foe* f) {
  return createFoeCommandFromPool(parser, f);
}

/** @brief Creates a FoeCommand for an existing BulletML state. */
FoeCommand* createFoeCommand(BulletMLState* state, Foe* f) {
  if (hasBulletMlAllocFailureLatched()) {
    destroyBulletMlState(state);
    return nullptr;
  }
  return createFoeCommandFromPool(state, f);
}

/** @brief Returns a FoeCommand to the pool and clears the pointer. */
void destroyFoeCommand(FoeCommand*& cmd)
{
  if (!cmd)
  {
    return;
  }

  const int slot = getFoeCommandSlot(cmd);
  if (slot < 0 || !sFoeCommandSlotInUse[slot])
  {
    // Double destroy or stray pointer: pushing it would give one slot two owners.
    SRL::Logger::LogWarning("[FOECMD_POOL] destroy of non-live command cmd=%p slot=%d",
                            (void*)cmd, slot);
    cmd = nullptr;
    return;
  }

  cmd->~FoeCommand();
  sFoeCommandSlotInUse[slot] = false;
  sFoeCommandFreeSlots[sFoeCommandFreeTop++] = static_cast<uint16_t>(slot);
  cmd = nullptr;
}

/** @brief Frees the pool block. Shutdown only: live commands go with it. */
void releaseFoeCommandPool()
{
  if (sFoeCommandPool)
  {
    freeHighWorkRamItems(sFoeCommandPool);
  }
  sFoeCommandPoolSize = 0;
  sFoeCommandFreeTop = 0;
}

std::size_t getFoeCommandCachedCount()
{
  return sFoeCommandFreeTop;
}

/** @brief Allocates the fixed FoeCommand pool once at startup.
 *  This prevents allocation failures during peak gameplay load.
 */
void preallocateFoeCommandPool(std::size_t count)
{
  if (sFoeCommandPool)
  {
    SRL::Logger::LogWarning("[FOECMD_POOL] Pool already allocated (size=%u)",
                            (unsigned int)sFoeCommandPoolSize);
    return;
  }
  if (count > kFoeCommandPoolMaxCapacity)
  {
    count = kFoeCommandPoolMaxCapacity;
  }
  if (count == 0)
  {
    return;
  }

  sFoeCommandPool = allocateHighWorkRamItems<FoeCommand>(count);
  if (!sFoeCommandPool)
  {
    SRL::Logger::LogWarning("[FOECMD_POOL] Preallocation of %u FoeCommand objects failed",
                            (unsigned int)count);
    return;
  }

  sFoeCommandPoolSize = static_cast<uint16_t>(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    // Lowest slot on top, so the first spawns use the start of the block.
    sFoeCommandFreeSlots[i] = static_cast<uint16_t>(count - 1 - i);
    sFoeCommandSlotInUse[i] = false;
  }
  sFoeCommandFreeTop = static_cast<uint16_t>(count);
  SRL::Logger::LogInfo("[FOECMD_POOL] Preallocated %u FoeCommand objects (%u bytes)",
                       (unsigned int)count, (unsigned int)(count * sizeof(FoeCommand)));
}

Fxp FoeCommand::getBulletDirection() {
  return legacyDirectionIndexToFxpDegrees(foe->d);
}

Fxp FoeCommand::getAimDirection() {
  return legacyDirectionIndexToFxpDegrees(getPlayerDeg(foe->pos.x, foe->pos.y));
}

Fxp FoeCommand::getBulletSpeed() {
  return Fxp::Convert(foe->spd) / COMMAND_SCREEN_SPD_RATE;
}

Fxp FoeCommand::getDefaultSpeed() {
  return 1;
}

Fxp FoeCommand::getRank() {
  return foe->rank;
}

void FoeCommand::createSimpleBullet(Fxp direction, Fxp speed) {
  int d = fxpDirectionToLegacyWrapped(direction);
  addFoeNormalBullet(&(foe->pos), foe->rank, 
		     d, fxpToLegacyScaledInt(speed, COMMAND_SCREEN_SPD_RATE), foe->color+1);
}

void FoeCommand::createBullet(BulletMLState* state, Fxp direction, Fxp speed) {
  if (hasBulletMlAllocFailureLatched()) {
    destroyBulletMlState(state);
    return;
  }

  int d = fxpDirectionToLegacyWrapped(direction);
  addFoeActiveBullet(&(foe->pos), foe->rank, 
		     d, fxpToLegacyScaledInt(speed, COMMAND_SCREEN_SPD_RATE), foe->color+1, state);
}

int FoeCommand::getTurn() {
  return tick;
}

void FoeCommand::doVanish() {
  removeFoe(foe);
}

void FoeCommand::doChangeDirection(Fxp d) {
  foe->d = fxpDirectionToLegacySigned(d);
}

void FoeCommand::doChangeSpeed(Fxp s) {
  foe->spd = fxpToLegacyScaledInt(s, COMMAND_SCREEN_SPD_RATE);
}

void FoeCommand::doAccelX(Fxp ax) {
  foe->vel.x = fxpToLegacyScaledInt(ax, COMMAND_SCREEN_VEL_RATE);
}

void FoeCommand::doAccelY(Fxp ay) {
  foe->vel.y = fxpToLegacyScaledInt(ay, COMMAND_SCREEN_VEL_RATE);
}

Fxp FoeCommand::getBulletSpeedX() {
  return Fxp::Convert(foe->vel.x) / COMMAND_SCREEN_VEL_RATE;
}

Fxp FoeCommand::getBulletSpeedY() {
  return Fxp::Convert(foe->vel.y) / COMMAND_SCREEN_VEL_RATE;
}
