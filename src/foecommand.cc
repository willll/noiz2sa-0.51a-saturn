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

#include "noiz2sa.h"
#include "degutil.h"
#include "ship.h"

#define COMMAND_SCREEN_SPD_RATE (insanespeed ? 800 : (800 / 2))
#define COMMAND_SCREEN_VEL_RATE (insanespeed ? 800 : (800 / 2))

static int fxpToLegacyInt(Fxp value)
{
  int result = value.As<int>();
  if (value.RawValue() < 0 && (value.RawValue() & 0xFFFF) != 0)
  {
    result += 1;
  }
  return result;
}

static int fxpToLegacyScaledInt(Fxp value, int scale)
{
  const int64_t scaledRaw = static_cast<int64_t>(value.RawValue()) * scale;
  if (scaledRaw >= 0)
  {
    return static_cast<int>((scaledRaw + 0x8000) >> 16);
  }
  return -static_cast<int>(((-scaledRaw) + 0x8000) >> 16);
}

static int fxpDirectionToLegacySigned(Fxp direction)
{
  const int64_t scaledRaw = static_cast<int64_t>(direction.RawValue()) * DIV / 360;
  return static_cast<int>(scaledRaw / 65536);
}

static int fxpDirectionToLegacyWrapped(Fxp direction)
{
  int d = fxpDirectionToLegacySigned(direction);
  d &= (DIV - 1);
  return d;
}

static Fxp legacyDirectionIndexToFxpDegrees(int direction)
{
  const int64_t raw = static_cast<int64_t>(direction) * 360 * 65536 / DIV;
  return Fxp::BuildRaw(static_cast<int32_t>(raw));
}



FoeCommand::FoeCommand(BulletMLParserBLB *parser, Foe *f)
  : BulletMLRunner(parser) {
  foe = f;
}

FoeCommand::FoeCommand(BulletMLState *state, Foe *f)
  : BulletMLRunner(state) {
  foe = f;
}

FoeCommand::~FoeCommand() {}

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
