/*
 * $Id: frag.c,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Fragments.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "SDL.h"
#include <srl_memory.hpp> // for memory allocation
#include <srl_log.hpp>    // for logging

#include "noiz2sa.h"
#include "screen.h"
#include "vector.h"
#include "degutil.h"
#include "shot.h"
#include "frag.h"

Frag frag[FRAG_MAX];

static constexpr int kFragSpawnMultiplier = 3;

/** @brief Initialises the fragment pool. */
void initFrags()
{
  int i;
  for (i = 0; i < FRAG_MAX; i++)
  {
    frag[i].cnt = 0;
  }
}

static int fragIdx = FRAG_MAX;

/** @brief Adds a fragment with the requested style and size. */
static void addFrag(Vector *pos, Vector *vel, int spc, int size)
{
  int i;
  for (i = 0; i < FRAG_MAX; i++)
  {
    fragIdx--;
    if (fragIdx < 0)
      fragIdx = FRAG_MAX - 1;
    if (frag[i].cnt <= 0)
      break;
  }
  if (i >= FRAG_MAX)
    return;
  frag[i].pos = *pos;
  frag[i].vel = *vel;
  switch (spc)
  {
  case 0:
    frag[i].width = 5 + randN(10) / SCREEN_DIVISOR;
    frag[i].height = 5 + randN(10) / SCREEN_DIVISOR;
    frag[i].cnt = 4 + randN(8);
    break;
  case 1:
    frag[i].width = size * 5 + randN(size * 3) / SCREEN_DIVISOR;
    frag[i].height = size * 5 + randN(size * 3) / SCREEN_DIVISOR;
    frag[i].cnt = 12 + randN(12);
    break;
  case 2:
    frag[i].width = 4 / SCREEN_DIVISOR;
    frag[i].height = 4 / SCREEN_DIVISOR;
    frag[i].cnt = 10 + randN(4);
    break;
  }
  frag[i].spc = spc;
}

/** @brief Frees the oldest fragments until the requested capacity is available. */
static void ensureFragCapacity(int required)
{
  int freeSlots = 0;
  for (int i = 0; i < FRAG_MAX; i++)
  {
    if (frag[i].cnt <= 0)
    {
      freeSlots++;
    }
  }

  int reclaim = required - freeSlots;
  while (reclaim > 0)
  {
    int victim = -1;
    int minCnt = 0x7fffffff;
    for (int i = 0; i < FRAG_MAX; i++)
    {
      if (frag[i].cnt > 0 && frag[i].cnt < minCnt)
      {
        minCnt = frag[i].cnt;
        victim = i;
      }
    }

    if (victim < 0)
    {
      break;
    }

    frag[victim].cnt = 0;
    reclaim--;
  }
}

/** @brief Spawns fragments for a shot impact. */
void addShotFrag(Vector *p)
{
  Vector pos, vel;
  pos.x = (p->x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  pos.y = (p->y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  for (int i = 0; i < kFragSpawnMultiplier; i++)
  {
    vel.x = randNS(SHOT_SPEED >> 11) * LAYER_WIDTH / SCAN_WIDTH;
    vel.y = (-(SHOT_SPEED >> 8) + randNS(SHOT_SPEED >> 11)) * LAYER_HEIGHT / SCAN_HEIGHT;
    addFrag(&pos, &vel, 0, 0);
  }
}

/** @brief Spawns fragments for an enemy explosion. */
void addEnemyFrag(Vector *p, int mx, int my, int type)
{
  Vector pos, vel;
  int cmx, cmy;
  unsigned int i;
  pos.x = (p->x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  pos.y = (p->y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  cmx = (mx / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  cmy = (my / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  type = type * 2 + 1;
  for (i = 0; i < (unsigned int)((type + randN(type * 2)) * kFragSpawnMultiplier); i++)
  {
    vel.x = randNS(16);
    vel.y = randNS(16);
    addFrag(&pos, &vel, 0, 0);
  }
  for (i = 0; i < (unsigned int)((type * 2 + randN(type)) * kFragSpawnMultiplier); i++)
  {
    vel.x = cmx + randNS(3);
    vel.y = cmy + randNS(3);
    addFrag(&pos, &vel, 1, 2 + type);
  }
}

/** @brief Spawns fragments for the player's ship explosion. */
void addShipFrag(Vector *p)
{
  Vector pos, vel;
  int cmx, cmy;
  int i;

  // Ship explosion should remain visible even in effect-heavy scenes.
  ensureFragCapacity((48 + 32) * kFragSpawnMultiplier);

  pos.x = (p->x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  pos.y = (p->y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  for (i = 0; i < 48 * kFragSpawnMultiplier; i++)
  {
    vel.x = randNS(24);
    vel.y = randNS(24);
    addFrag(&pos, &vel, 0, 0);
  }
  for (i = 0; i < 32 * kFragSpawnMultiplier; i++)
  {
    vel.x = randNS(4);
    vel.y = randNS(4);
    addFrag(&pos, &vel, 1, 1 + randN(6));
  }
}

/** @brief Spawns fragments for a stage-clear effect. */
void addClearFrag(Vector *p, Vector *v)
{
  Vector pos, vel;
  pos.x = (p->x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  pos.y = (p->y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  vel.x = (v->x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
  vel.y = (v->y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
  for (int i = 0; i < kFragSpawnMultiplier; i++)
  {
    addFrag(&pos, &vel, 2, 0);
  }
}

/** @brief Advances all active fragments. */
void moveFrags()
{
  int i;
  Frag *fr;
  for (i = 0; i < FRAG_MAX; i++)
  {
    if (frag[i].cnt <= 0)
      continue;
    fr = &(frag[i]);
    fr->pos.x += fr->vel.x;
    fr->pos.y += fr->vel.y;
    fr->cnt--;
  }
}

static int fragColor[3][2][2] = {
    {{16 * 8 - 7, 16 * 2 - 2}, {16 * 2 - 7, 16 * 8 - 2}},
    {{16 * 5 - 7, 16 * 2 - 2}, {16 * 2 - 7, 16 * 5 - 2}},
    {{16 * 1 - 10, 16 * 1 - 5}, {16 * 1 - 5, 16 * 1 - 10}},
};

/** @brief Draws all active fragments. */
void drawFrags()
{
  int x, y, c;
  int i;
  Frag *fr;
  for (i = 0; i < FRAG_MAX; i++)
  {
    if (frag[i].cnt <= 0)
      continue;
    fr = &(frag[i]);
    c = fr->cnt & 1;
    drawBox(fr->pos.x, fr->pos.y, fr->width, fr->height,
          fragColor[fr->spc][c][0], fragColor[fr->spc][c][1], buf);
  }
}

/** @brief Returns the number of active fragments. */
int getActiveFragCount()
{
  int active = 0;
  for (int i = 0; i < FRAG_MAX; i++)
  {
    if (frag[i].cnt > 0)
    {
      active++;
    }
  }
  return active;
}

/** @brief Returns the total fragment pool capacity. */
int getFragPoolCapacity()
{
  return FRAG_MAX;
}
