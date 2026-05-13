/*
 * $Id: foecommand.h,v 1.2 2003/08/10 04:09:46 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Foe commands data.
 *
 * @version $Revision: 1.2 $
 */
#ifndef FOECOMMAND_H_
#define FOECOMMAND_H_

#include "bulletml_binary/bulletmlparser_blb.hpp"
#include "bulletml_binary/bulletmlrunner.hpp"
#include "foe.h"

class FoeCommand : public BulletMLRunner {
 public:
  /** @brief Creates a command runner bound to a BulletML parser. */
  FoeCommand(BulletMLParserBLB* parser, struct foe* f);
  /** @brief Creates a command runner bound to an existing BulletML state. */
  FoeCommand(BulletMLState* state, struct foe* f);

  /** @brief Releases the command runner. */
  virtual ~FoeCommand();

  /** @brief Returns the current bullet direction in BulletML space. */
  virtual Fxp getBulletDirection();
  /** @brief Returns the aim direction toward the player. */
  virtual Fxp getAimDirection();
  /** @brief Returns the current bullet speed. */
  virtual Fxp getBulletSpeed();
  /** @brief Returns the default bullet speed. */
  virtual Fxp getDefaultSpeed();
  /** @brief Returns the current BulletML rank value. */
  virtual Fxp getRank();
  /** @brief Spawns a simple bullet without extra state. */
  virtual void createSimpleBullet(Fxp direction, Fxp speed);
  /** @brief Spawns a bullet using a supplied BulletML state. */
  virtual void createBullet(BulletMLState* state, Fxp direction, Fxp speed);
  /** @brief Returns the current frame tick used by BulletML commands. */
  virtual int getTurn();
  /** @brief Removes the current foe from the field. */
  virtual void doVanish();
  
  /** @brief Changes the foe direction. */
  virtual void doChangeDirection(Fxp d);
  /** @brief Changes the foe speed. */
  virtual void doChangeSpeed(Fxp s);
  /** @brief Applies an X-axis acceleration. */
  virtual void doAccelX(Fxp ax);
  /** @brief Applies a Y-axis acceleration. */
  virtual void doAccelY(Fxp ay);
  /** @brief Returns the current bullet X speed. */
  virtual Fxp getBulletSpeedX();
  /** @brief Returns the current bullet Y speed. */
  virtual Fxp getBulletSpeedY();
  
 private:
  struct foe *foe;
};

FoeCommand* createFoeCommand(BulletMLParserBLB* parser, struct foe* f);
FoeCommand* createFoeCommand(BulletMLState* state, struct foe* f);
void destroyFoeCommand(FoeCommand*& cmd);
void releaseFoeCommandPool();

#endif


