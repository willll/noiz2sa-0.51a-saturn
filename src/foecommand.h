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
  FoeCommand(BulletMLParserBLB* parser, struct foe* f);
  FoeCommand(BulletMLState* state, struct foe* f);

  virtual ~FoeCommand();

  virtual Fxp getBulletDirection();
  virtual Fxp getAimDirection();
  virtual Fxp getBulletSpeed();
  virtual Fxp getDefaultSpeed();
  virtual Fxp getRank();
  virtual void createSimpleBullet(Fxp direction, Fxp speed);
  virtual void createBullet(BulletMLState* state, Fxp direction, Fxp speed);
  virtual int getTurn();
  virtual void doVanish();
  
  virtual void doChangeDirection(Fxp d);
  virtual void doChangeSpeed(Fxp s);
  virtual void doAccelX(Fxp ax);
  virtual void doAccelY(Fxp ay);
  virtual Fxp getBulletSpeedX();
  virtual Fxp getBulletSpeedY();
  
 private:
  struct foe *foe;
};
#endif


