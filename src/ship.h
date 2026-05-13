/*
 * $Id: ship.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Player data.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

typedef struct {
  Vector pos;
  int cnt, shotCnt;
  int speed;
  int invCnt;
} Ship;

extern Ship ship;

/**
 * @brief Initialises the player ship state.
 */
void initShip();

/**
 * @brief Advances the ship position and handles player movement input.
 */
void moveShip();

/**
 * @brief Renders the ship and associated visual effects.
 */
void drawShip();

/**
 * @brief Destroys the ship and clears any related state.
 */
void destroyShip();

/**
 * @brief Returns the player's direction index toward the given point.
 * @param x Target X coordinate.
 * @param y Target Y coordinate.
 * @return Direction index in the legacy 0-1023 range.
 */
int getPlayerDeg(int x, int y);
