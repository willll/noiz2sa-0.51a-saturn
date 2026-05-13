/*
 * $Id: bonus.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Bonus item data.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

typedef struct {
  Vector pos, vel;
  int cnt;
  int down;
} Bonus;

#define BONUS_MAX 256

extern int bonusScore;

/** @brief Resets the bonus score counter. */
void resetBonusScore();
/** @brief Initialises bonus entities. */
void initBonuses();
/** @brief Advances all active bonuses. */
void moveBonuses();
/** @brief Draws all active bonuses. */
void drawBonuses();
/** @brief Spawns a bonus entity at the given position and velocity. */
void addBonus(Vector *pos, Vector *vel);
