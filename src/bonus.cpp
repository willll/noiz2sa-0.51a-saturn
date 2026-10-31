/*
 * $Id: bonus.c,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Bonus item.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "SDL.h"
#include <string.h>
#include <srl_memory.hpp> // for memory allocation
#include <srl_log.hpp>    // for logging

#include "noiz2sa.h"
#include "screen.h"
#include "vector.h"
#include "degutil.h"
#include "ship.h"
#include "bonus.h"
#include "soundmanager.h"
#include "attractmanager.h"

Bonus bonus[BONUS_MAX];
int bonusScore;

/** @brief Resets the bonus score counter. */
void resetBonusScore()
{
  bonusScore = 10;
  showScore();
}

/** @brief Awards the current bonus and increments the next threshold. */
static void getBonus()
{
  addScore(bonusScore);
  if (bonusScore < 1000)
    bonusScore += 10;
}

/** @brief Applies the miss penalty to the bonus score counter. */
static void missBonus()
{
  bonusScore /= 20;
  bonusScore *= 10;
  if (bonusScore < 10)
    bonusScore = 10;
  showScore();
}

/** @brief Initialises the bonus entity pool. */
static void prepareStarLayer();
static void runStarLayer();
static void resetStarRecord();

void initBonuses()
{
  int i;
  for (i = 0; i < BONUS_MAX; i++)
  {
    bonus[i].cnt = NOT_EXIST;
  }
  resetBonusScore();
  resetStarRecord();
  setPlayfieldSlaveJob(prepareStarLayer, runStarLayer);
}

static int bonusIdx = BONUS_MAX;

/** @brief Spawns a bonus item. */
void addBonus(Vector *pos, Vector *vel)
{
  int i;
  for (i = 0; i < BONUS_MAX; i++)
  {
    bonusIdx--;
    if (bonusIdx < 0)
      bonusIdx = BONUS_MAX - 1;
    if (bonus[i].cnt == NOT_EXIST)
      break;
  }
  if (i >= BONUS_MAX)
    return;
  bonus[i].pos = *pos;
  bonus[i].vel = *vel;
  bonus[i].cnt = 0;
  bonus[i].down = 1;
}

#define BONUS_SPEED (400 / SCREEN_DIVISOR)
#define BONUS_INHALE_WIDTH (24000 / SCREEN_DIVISOR)
#define BONUS_ACQUIRE_WIDTH (8000 / SCREEN_DIVISOR)

/** @brief Advances active bonuses and handles acquisition/miss logic. */
void moveBonuses()
{
  int i, d;
  Bonus *bn;
  for (i = 0; i < BONUS_MAX; i++)
  {
    if (bonus[i].cnt == NOT_EXIST)
      continue;
    bn = &(bonus[i]);
    bn->pos.x += bn->vel.x;
    bn->pos.y += bn->vel.y;
    bn->vel.x -= bn->vel.x >> 6;
    if (bn->pos.x < SCAN_WIDTH_8 / 8)
    {
      bn->pos.x = SCAN_WIDTH_8 / 8;
      if (bn->vel.x < 0)
        bn->vel.x = -bn->vel.x;
    }
    else if (bn->pos.x > SCAN_WIDTH_8 / 8 * 7)
    {
      bn->pos.x = SCAN_WIDTH_8 / 8 * 7;
      if (bn->vel.x > 0)
        bn->vel.x = -bn->vel.x;
    }
    if (bn->down)
    {
      bn->vel.y += (BONUS_SPEED - bn->vel.y) >> 6;
      if (bn->pos.y > SCAN_HEIGHT_8)
      {
        bn->down = 0;
        bn->pos.y = SCAN_HEIGHT_8;
        bn->vel.y = -bn->vel.y;
      }
    }
    else
    {
      bn->vel.y += (-BONUS_SPEED - bn->vel.y) >> 6;
      if (bn->pos.y < 0)
      {
        missBonus();
        bn->cnt = NOT_EXIST;
        continue;
      }
    }

    d = vctDist(&(ship.pos), &(bn->pos));
    if (d < BONUS_ACQUIRE_WIDTH)
    {
      getBonus();
      playChunk(5);
      bn->cnt = NOT_EXIST;
      continue;
    }
    else if (d < BONUS_INHALE_WIDTH)
    {
      bn->vel.x += (long)(ship.pos.x - bn->pos.x) * (BONUS_INHALE_WIDTH - d) >> 20;
      bn->vel.y += (long)(ship.pos.y - bn->pos.y) * (BONUS_INHALE_WIDTH - d) >> 20;
    }
    bn->cnt++;
  }
}

#define BONUS_COLOR_1 16 * 3 - 3
#define BONUS_COLOR_2 16 * 4 - 7
#define BONUS_DRAW_WIDTH (8 / SCREEN_DIVISOR)

// Bonus stars were ~89% of drawBox calls in heavy scenes. The slave draws them
// into the playfield buffer right after preparing it for the frame (see
// startPlayfieldPrep()), and draw() waits for that before anything else is
// drawn, so everything still covers the stars as before. The slave draws the
// boxes recorded in the previous frame: stars lag the rest by a tick.
struct StarBox
{
  int16_t x, y; ///< box centre in playfield pixels
};
static constexpr int kMaxStarBoxes = BONUS_MAX * 4;
static StarBox sRecordedBoxes[kMaxStarBoxes]; ///< master: filled by drawBonuses()
static int sRecordedCount = 0;
static StarBox sSlaveBoxes[kMaxStarBoxes];    ///< handed to the slave each tick
static int sSlaveCount = 0;
static bool sStarsHanded = false;             ///< master: the slave owes this frame's stars
static volatile bool sStarsDone = false;      ///< slave: set once they are in buf


/** @brief Reads a flag the other CPU writes, bypassing this CPU's cache. */
static inline bool readUncached(const volatile bool &flag)
{
  return *reinterpret_cast<const volatile bool *>(reinterpret_cast<uintptr_t>(&flag) | 0x20000000u);
}

/** @brief Forgets recorded boxes (new game or title: none from before). */
static void resetStarRecord()
{
  sRecordedCount = 0;
}

/** @brief Master, slave idle: hands the last frame's boxes to the slave run. */
static void prepareStarLayer()
{
  sStarsHanded = (status == IN_GAME || status == STAGE_CLEAR);
  sSlaveCount = sStarsHanded ? sRecordedCount : 0;
  memcpy(sSlaveBoxes, sRecordedBoxes, sSlaveCount * sizeof(StarBox));
  sStarsDone = false;
}


/** @brief Draws an unclipped 4x4 star box (top-left p) as drawBox would, with the
 *  widest stores its alignment allows: buf is write-through RAM, so every store
 *  costs a bus write (~17us per box with byte stores on hardware). The 160-byte
 *  pitch keeps all four rows at the same alignment. */
static inline void putStarBox4(uint8_t *p, uint8_t c1, uint8_t c2)
{
  const uintptr_t align = reinterpret_cast<uintptr_t>(p) & 3u;
  for (int row = 0; row < 4; row++, p += LAYER_WIDTH)
  {
    const uint8_t inner = (row == 0 || row == 3) ? c2 : c1;
    if (align == 0u)
    {
      storePixels4(p, c2, inner, inner, c2);
    }
    else if (align == 2u)
    {
      storePixels2(p, c2, inner);
      storePixels2(p + 2, inner, c2);
    }
    else
    {
      p[0] = c2;
      storePixels2(p + 1, inner, inner);
      p[3] = c2;
    }
  }
}

/** @brief Slave: draws the handed-over boxes into buf. */
static void runStarLayer()
{
  uint8_t *const pixels = reinterpret_cast<uint8_t *>(buf);
  for (int i = 0; i < sSlaveCount; i++)
  {
    const int x = sSlaveBoxes[i].x - 2;
    const int y = sSlaveBoxes[i].y - 2;
    // Boxes drawBox would not clip (its right test is >=); edge boxes go through it.
    if (BONUS_DRAW_WIDTH == 4 && x >= 0 && x + 4 < LAYER_WIDTH && y >= 0 && y + 4 <= LAYER_HEIGHT)
    {
      putStarBox4(pixels + x + y * LAYER_WIDTH, BONUS_COLOR_1, BONUS_COLOR_2);
    }
    else
    {
      drawBox(sSlaveBoxes[i].x, sSlaveBoxes[i].y, BONUS_DRAW_WIDTH, BONUS_DRAW_WIDTH,
              BONUS_COLOR_1, BONUS_COLOR_2, buf);
    }
  }
  sStarsDone = true;
}

/** @brief Draws the stars on the slave when it took them this tick, else here;
 *  records their boxes for the next tick's slave run. */
void drawBonuses()
{
  int x, y, ox, oy, d;
  int i;
  Bonus *bn;
  bool onSlave = sStarsHanded;
  sStarsHanded = false;
  // Bounded so a slave that never answers costs one CPU-drawn frame, not a hang.
  for (uint32_t spin = 0; onSlave && !readUncached(sStarsDone); spin++)
  {
    if (spin > 4000000u)
    {
      onSlave = false;
    }
  }
  sRecordedCount = 0;
  for (i = 0; i < BONUS_MAX; i++)
  {
    if (bonus[i].cnt == NOT_EXIST)
      continue;
    bn = &(bonus[i]);
    d = (bn->cnt * 8) & (DIV - 1);
    x = (bn->pos.x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
    y = (bn->pos.y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
    ox = (sctbl[d] >> 5) / SCREEN_DIVISOR;
    oy = (sctbl[d + DIV / 4] >> 5) / SCREEN_DIVISOR;
    const int bx[4] = {x + ox, x - ox, x + oy, x - oy};
    const int by[4] = {y + oy, y - oy, y - ox, y + ox};
    for (int k = 0; k < 4; k++)
    {
      if (!onSlave)
      {
        drawBox(bx[k], by[k], BONUS_DRAW_WIDTH, BONUS_DRAW_WIDTH, BONUS_COLOR_1, BONUS_COLOR_2, buf);
      }
      sRecordedBoxes[sRecordedCount].x = (int16_t)bx[k];
      sRecordedBoxes[sRecordedCount].y = (int16_t)by[k];
      sRecordedCount++;
    }
  }
}

/** @brief Returns the number of active bonuses. */
int getActiveBonusCount()
{
  int active = 0;
  for (int i = 0; i < BONUS_MAX; i++)
  {
    if (bonus[i].cnt != NOT_EXIST)
    {
      active++;
    }
  }
  return active;
}

/** @brief Returns the total bonus pool capacity. */
int getBonusPoolCapacity()
{
  return BONUS_MAX;
}
