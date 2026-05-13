/*
 * $Id: frag.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Fragment data.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

typedef struct {
  Vector pos, vel;
  int width, height;
  int cnt;
  int spc;
} Frag;

#define FRAG_MAX 64

/** @brief Initialises the fragment pool. */
void initFrags();
/** @brief Advances all active fragments. */
void moveFrags();
/** @brief Draws all active fragments. */
void drawFrags();
/** @brief Spawns fragments for a shot impact. */
void addShotFrag(Vector *pos);
/** @brief Spawns fragments for an enemy explosion. */
void addEnemyFrag(Vector *p, int mx, int my, int type);
/** @brief Spawns fragments for the player's ship. */
void addShipFrag(Vector *p);
/** @brief Spawns fragments for a stage-clear effect. */
void addClearFrag(Vector *p, Vector *v);
