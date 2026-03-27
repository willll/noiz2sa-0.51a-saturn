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
#include <srl_log.hpp>

#include "noiz2sa.h"
#include "degutil.h"
#include "ship.h"

#define COMMAND_SCREEN_SPD_RATE (insanespeed ? 800 : (800 / 2))
#define COMMAND_SCREEN_VEL_RATE (insanespeed ? 800 : (800 / 2))

static unsigned long gCommandCreatedFromParser = 0;
static unsigned long gCommandCreatedFromState = 0;
static unsigned long gCreateSimpleBulletCalls = 0;
static unsigned long gCreateActiveBulletCalls = 0;

FoeCommand::FoeCommand(BulletMLParserBLB *parser, Foe *f)
  : BulletMLRunner(parser) {
  foe = f;
  gCommandCreatedFromParser++;
  if (gCommandCreatedFromParser <= 5 || (gCommandCreatedFromParser % 30) == 0)
  {
    SRL::Logger::LogWarning("[CMDDBG] parser_cmd=%lu state_cmd=%lu simple=%lu active=%lu",
                            gCommandCreatedFromParser,
                            gCommandCreatedFromState,
                            gCreateSimpleBulletCalls,
                            gCreateActiveBulletCalls);
  }
}

FoeCommand::FoeCommand(BulletMLState *state, Foe *f)
  : BulletMLRunner(state) {
  foe = f;
  gCommandCreatedFromState++;
  if (gCommandCreatedFromState <= 5 || (gCommandCreatedFromState % 30) == 0)
  {
    SRL::Logger::LogWarning("[CMDDBG] parser_cmd=%lu state_cmd=%lu simple=%lu active=%lu",
                            gCommandCreatedFromParser,
                            gCommandCreatedFromState,
                            gCreateSimpleBulletCalls,
                            gCreateActiveBulletCalls);
  }
}

FoeCommand::~FoeCommand() {}

double FoeCommand::getBulletDirection() {
  return (double)foe->d*360/DIV;
}

double FoeCommand::getAimDirection() {
  return ((double)getPlayerDeg(foe->pos.x, foe->pos.y)*360/DIV);
}

double FoeCommand::getBulletSpeed() {
  return ((double)foe->spd)/COMMAND_SCREEN_SPD_RATE;
}

double FoeCommand::getDefaultSpeed() {
  return 1;
}

double FoeCommand::getRank() {
  return foe->rank;
}

void FoeCommand::createSimpleBullet(double direction, double speed) {
  int d = (int)(direction*DIV/360); d &= (DIV-1);
  gCreateSimpleBulletCalls++;
  if (gCreateSimpleBulletCalls <= 5 || (gCreateSimpleBulletCalls % 30) == 0)
  {
    SRL::Logger::LogWarning("[CMDDBG] parser_cmd=%lu state_cmd=%lu simple=%lu active=%lu",
                            gCommandCreatedFromParser,
                            gCommandCreatedFromState,
                            gCreateSimpleBulletCalls,
                            gCreateActiveBulletCalls);
  }
  addFoeNormalBullet(&(foe->pos), foe->rank, 
		     d, (int)(speed*COMMAND_SCREEN_SPD_RATE), foe->color+1);
}

void FoeCommand::createBullet(BulletMLState* state, double direction, double speed) {
  int d = (int)(direction*DIV/360); d &= (DIV-1);
  gCreateActiveBulletCalls++;
  if (gCreateActiveBulletCalls <= 5 || (gCreateActiveBulletCalls % 30) == 0)
  {
    SRL::Logger::LogWarning("[CMDDBG] parser_cmd=%lu state_cmd=%lu simple=%lu active=%lu",
                            gCommandCreatedFromParser,
                            gCommandCreatedFromState,
                            gCreateSimpleBulletCalls,
                            gCreateActiveBulletCalls);
  }
  addFoeActiveBullet(&(foe->pos), foe->rank, 
		     d, (int)(speed*COMMAND_SCREEN_SPD_RATE), foe->color+1, state);
}

int FoeCommand::getTurn() {
  return tick;
}

void FoeCommand::doVanish() {
  removeFoe(foe);
}

void FoeCommand::doChangeDirection(double d) {
  foe->d = (int)(d*DIV/360) & (DIV-1);
}

void FoeCommand::doChangeSpeed(double s) {
  foe->spd = (int)(s*COMMAND_SCREEN_SPD_RATE);
}

void FoeCommand::doAccelX(double ax) {
  foe->vel.x = (int)(ax*COMMAND_SCREEN_VEL_RATE);
}

void FoeCommand::doAccelY(double ay) {
  foe->vel.y = (int)(ay*COMMAND_SCREEN_VEL_RATE);
}

double FoeCommand::getBulletSpeedX() {
  return ((double)foe->vel.x/COMMAND_SCREEN_VEL_RATE);
}

double FoeCommand::getBulletSpeedY() {
  return ((double)foe->vel.y/COMMAND_SCREEN_VEL_RATE);
}
