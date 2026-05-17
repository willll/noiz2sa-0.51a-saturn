/*
 * $Id: shot.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Shot data.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

typedef struct {
  Vector pos;
  int cnt;
} Shot;

#define SHOT_MAX 16

#define SHOT_SPEED (4096 / SCREEN_DIVISOR)

#define SHOT_WIDTH (8 / SCREEN_DIVISOR)
#define SHOT_HEIGHT (24 / SCREEN_DIVISOR)

extern Shot shot[];

/**
 * @brief Initialises the shot pool.
 */
void initShots();

/**
 * @brief Advances all active shots.
 */
void moveShots();

/**
 * @brief Draws all active shots.
 */
void drawShots();

/**
 * @brief Spawns a shot at the supplied position.
 * @param pos Spawn position.
 */
void addShot(Vector *pos);

/** @brief Returns the number of active shots. */
int getActiveShotCount();
/** @brief Returns the total shot pool capacity. */
int getShotPoolCapacity();
