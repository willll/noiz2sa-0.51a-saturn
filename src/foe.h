/*
 * $Id: foe.h,v 1.2 2003/08/10 04:09:46 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Enemy data.
 *
 * @version $Revision: 1.2 $
 */
#ifndef FOE_H_
#define FOE_H_

#include "bulletml_binary/bulletmlparser_blb.hpp"
#include "foecommand.h"
#include "barragemanager.h"

#include "vector.h"

#define FOE 0
#define BOSS_ACTIVE_BULLET 1
#define ACTIVE_BULLET 2
#define BULLET 3

struct foe {
  Vector pos, vel, ppos, spos, mv;
  int d, spd;
  FoeCommand *cmd;
  double rank;
  int spc;
  int type;
  int shield;
  int cnt, color;
  int hit;
  
  BulletMLParserBLB *parser;
};

typedef struct foe Foe;

#include "foe_mtd.h"

extern int foeCnt, enNum[];

Foe* addFoe(int x, int y, double rank, int d, int spd, int typek, int shield, 
      BulletMLParserBLB *parser);
Foe* addFoeBossActiveBullet(int x, int y, double rank, 
          int d, int spd, BulletMLParserBLB *state);
void addFoeActiveBullet(Vector *pos, double rank, 
			int d, int spd, int color, BulletMLState *state);
void addFoeNormalBullet(Vector *pos, double rank, int d, int spd, int color);
void removeFoe(Foe *fe);
#endif
