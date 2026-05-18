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

class FoeCommand;  // Forward declaration to avoid circular includes

#include "bulletml_binary/bulletmlparser_blb.hpp"
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
  Fxp rank;
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

/**
 * @brief Allocates and initialises a foe or bullet entity.
 * @param x       Initial X position.
 * @param y       Initial Y position.
 * @param rank    BulletML rank value for the entity.
 * @param d       Initial direction index.
 * @param spd     Initial speed.
 * @param typek   Entity type identifier.
 * @param shield  Hit points or shield value.
 * @param parser  BulletML parser used to drive the foe behaviour.
 * @return Pointer to the created foe instance, or nullptr on failure.
 */
Foe* addFoe(int x, int y, Fxp rank, int d, int spd, int typek, int shield, 
      BulletMLParserBLB *parser);

/**
 * @brief Creates a boss-active bullet entity.
 * @param x      Initial X position.
 * @param y      Initial Y position.
 * @param rank   BulletML rank value for the entity.
 * @param d      Initial direction index.
 * @param spd    Initial speed.
 * @param state  BulletML parser/state driving the bullet.
 * @return Pointer to the created foe instance, or nullptr on failure.
 */
Foe* addFoeBossActiveBullet(int x, int y, Fxp rank, 
          int d, int spd, BulletMLParserBLB *state);

/**
 * @brief Creates an active bullet using the current BulletML state.
 * @param pos    Spawn position.
 * @param rank   BulletML rank value for the entity.
 * @param d      Initial direction index.
 * @param spd    Initial speed.
 * @param color  Bullet color index.
 * @param state  Current BulletML state.
 */
void addFoeActiveBullet(Vector *pos, Fxp rank, 
			int d, int spd, int color, BulletMLState *state);

/**
 * @brief Creates a normal enemy bullet.
 * @param pos    Spawn position.
 * @param rank   BulletML rank value for the entity.
 * @param d      Initial direction index.
 * @param spd    Initial speed.
 * @param color  Bullet color index.
 */
void addFoeNormalBullet(Vector *pos, Fxp rank, int d, int spd, int color);

/**
 * @brief Removes a foe entity from the active list.
 * @param fe Foe instance to remove.
 */
void removeFoe(Foe *fe);
#endif
