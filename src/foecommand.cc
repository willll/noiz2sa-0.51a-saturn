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
#include <srl_log.hpp>
#include <srl_memory.hpp>

#include "bulletml_runtime_factory.h"
#include "noiz2sa.h"
#include "degutil.h"
#include "ship.h"

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
struct FoeCommandFreeNode
{
  FoeCommandFreeNode* next;
};

static FoeCommandFreeNode* sFoeCommandFreeList = nullptr;
}

template <typename... Args>
/** @brief Allocates a FoeCommand from the recycled pool or high work RAM. */
static FoeCommand* createFoeCommandFromPool(Args&&... args)
{
  if (sFoeCommandFreeList)
  {
    FoeCommandFreeNode* node = sFoeCommandFreeList;
    sFoeCommandFreeList = node->next;
    return new (node) FoeCommand(std::forward<Args>(args)...);
  }
  return createBulletMlRuntimeObject<FoeCommand>(std::forward<Args>(args)...);
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
FoeCommand::~FoeCommand() {}

/** @brief Creates a FoeCommand for a parser-backed foe. */
FoeCommand* createFoeCommand(BulletMLParserBLB* parser, Foe* f) {
  return createFoeCommandFromPool(parser, f);
}

/** @brief Creates a FoeCommand for an existing BulletML state. */
FoeCommand* createFoeCommand(BulletMLState* state, Foe* f) {
  if (hasBulletMlAllocFailureLatched()) {
    delete state;
    return nullptr;
  }
  return createFoeCommandFromPool(state, f);
}

/** @brief Returns a FoeCommand to the recycle pool and clears the pointer. */
void destroyFoeCommand(FoeCommand*& cmd)
{
  if (!cmd)
  {
    return;
  }

  cmd->~FoeCommand();

  FoeCommandFreeNode* node = reinterpret_cast<FoeCommandFreeNode*>(cmd);
  node->next = sFoeCommandFreeList;
  sFoeCommandFreeList = node;

  cmd = nullptr;
}

/** @brief Frees all pooled FoeCommand storage. */
void releaseFoeCommandPool()
{
  while (sFoeCommandFreeList)
  {
    FoeCommandFreeNode* node = sFoeCommandFreeList;
    sFoeCommandFreeList = node->next;
    SRL::Memory::Free(node);
  }
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
    delete state;
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
