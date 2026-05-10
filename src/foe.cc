/*
 * $Id: foe.cc,v 1.5 2003/01/03 05:24:21 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Handle enemy.
 *
 * @version $Revision: 1.5 $
 */
#include "barragemanager.h"
#include "foe.h"

#include "SDL.h"
#include <stdlib.h>
#include <srl_memory.hpp>

#include "noiz2sa.h"
#include "screen.h"
#include "vector.h"
#include "degutil.h"
#include "ship.h"
#include "shot.h"
#include "frag.h"
#include "bonus.h"
#include "soundmanager.h"
#include "attractmanager.h"
#include "brgmng_mtd.h"
#include "letterrender.h"

#define FOE_MAX 1024
#define FOE_TYPE_MAX 4
#define SHIP_HIT_WIDTH (512 * 512)

static Foe foe[FOE_MAX];
static int foeActiveIndices[FOE_MAX];
static int foeActivePos[FOE_MAX];
static int foeActiveCount = 0;
int foeCnt, enNum[FOE_TYPE_MAX];

static unsigned long bulletSpawnedActive = 0;
static unsigned long bulletSpawnedNormal = 0;
static unsigned long bulletSpawnFailed = 0;
static unsigned long bulletRemovedOffscreen = 0;
static unsigned long bulletRemovedHitShip = 0;

// Per-frame pressure snapshot — read and zeroed by consumeFoePressureStats() each loop.
static uint32_t sFrameFoeCount = 0;
static uint32_t sFrameBulletCount = 0;
static uint32_t sFrameDrawnBullets = 0;
static uint32_t sFrameCulledBullets = 0;
static uint32_t sFrameFoeBudget = 0;

// Runtime foe update budget.  0 = uncapped (process all foes every frame).
// Adjusted dynamically by noiz2sa.cpp's adaptive controller each tick.
static int sRuntimeFoeBudgetLimit = 0;

void setFoeBudgetLimit(int limit)
{
  sRuntimeFoeBudgetLimit = limit;
}

// Hard projectile budget for Saturn hardware stability.
// This keeps moveFoes bounded so ship/foe updates stay responsive under stress.
static constexpr int kMaxActiveBullets = 160;
static constexpr int kMaxNormalBullets = 256;
static constexpr int kMaxBossActiveBullets = 64;
static constexpr int kMaxTotalProjectiles = 420;

static int sLiveActiveBullets = 0;
static int sLiveNormalBullets = 0;
static int sLiveBossActiveBullets = 0;

int getLiveProjectileCount()
{
  return sLiveActiveBullets + sLiveNormalBullets + sLiveBossActiveBullets;
}

static inline int getFoeIndex(const Foe *fe)
{
  return (int)(fe - foe);
}

static inline void markFoeSlotActive(const Foe *fe)
{
  const int idx = getFoeIndex(fe);
  if (foeActivePos[idx] >= 0)
  {
    return;
  }

  foeActivePos[idx] = foeActiveCount;
  foeActiveIndices[foeActiveCount++] = idx;
}

static void removeFoeSlotActive(const Foe *fe)
{
  const int idx = getFoeIndex(fe);
  const int activePos = foeActivePos[idx];
  if (activePos < 0)
  {
    return;
  }

  const int lastPos = foeActiveCount - 1;
  const int lastIdx = foeActiveIndices[lastPos];
  foeActiveIndices[activePos] = lastIdx;
  foeActivePos[lastIdx] = activePos;
  foeActivePos[idx] = -1;
  foeActiveCount = lastPos;
}

static bool bulletHitsShip(const Foe *fe)
{
  // Use closest-point distance to the swept segment [ppos, pos].
  // This prevents tunneling for fast bullets and includes endpoint contacts.
  const int startX = fe->ppos.x;
  const int startY = fe->ppos.y;
  const int deltaX = fe->pos.x - fe->ppos.x;
  const int deltaY = fe->pos.y - fe->ppos.y;
  const int shipX = ship.pos.x;
  const int shipY = ship.pos.y;

  const long long lengthSquared = (long long)deltaX * (long long)deltaX +
                                  (long long)deltaY * (long long)deltaY;

  if (lengthSquared <= 1)
  {
    const long long pointX = (long long)shipX - (long long)startX;
    const long long pointY = (long long)shipY - (long long)startY;
    const long long distSq = pointX * pointX + pointY * pointY;
    return distSq <= (long long)SHIP_HIT_WIDTH;
  }

  const long long dot = ((long long)shipX - (long long)startX) * (long long)deltaX +
                        ((long long)shipY - (long long)startY) * (long long)deltaY;

  const long long tFpMax = 1LL << 16;
  long long tFp = (dot << 16) / lengthSquared;
  if (tFp < 0)
  {
    tFp = 0;
  }
  else if (tFp > tFpMax)
  {
    tFp = tFpMax;
  }

  const long long nearestX = (long long)startX + (((long long)deltaX * tFp) >> 16);
  const long long nearestY = (long long)startY + (((long long)deltaY * tFp) >> 16);
  const long long distX = (long long)shipX - nearestX;
  const long long distY = (long long)shipY - nearestY;
  const long long distSq = distX * distX + distY * distY;

  return distSq <= (long long)SHIP_HIT_WIDTH;
}

static void removeFoeForcedNoDeleteCmd(Foe *fe)
{
  if (fe->spc == ACTIVE_BULLET)
  {
    if (sLiveActiveBullets > 0)
      sLiveActiveBullets--;
  }
  else if (fe->spc == BULLET)
  {
    if (sLiveNormalBullets > 0)
      sLiveNormalBullets--;
  }
  else if (fe->spc == BOSS_ACTIVE_BULLET)
  {
    if (sLiveBossActiveBullets > 0)
      sLiveBossActiveBullets--;
  }

  if (fe->spc == FOE)
  {
    foeCnt--;
    enNum[fe->type]--;
  }
  fe->spc = NOT_EXIST;
  removeFoeSlotActive(fe);
}

static void removeFoeForced(Foe *fe)
{
  removeFoeForcedNoDeleteCmd(fe);
  if (fe->cmd)
  {
    delete fe->cmd;
    fe->cmd = nullptr;
  }
}

void removeFoe(Foe *fe)
{
  if (fe->type == BOSS_TYPE)
    return;
  removeFoeForcedNoDeleteCmd(fe);
}

void initFoes()
{
  int i;
  foeActiveCount = 0;
  for (i = 0; i < FOE_MAX; i++)
  {
    foeActivePos[i] = -1;
    removeFoeForced(&(foe[i]));
  }
  foeCnt = 0;
  for (i = 0; i < FOE_TYPE_MAX; i++)
  {
    enNum[i] = 0;
  }
  bulletSpawnedActive = 0;
  bulletSpawnedNormal = 0;
  bulletSpawnFailed = 0;
  bulletRemovedOffscreen = 0;
  bulletRemovedHitShip = 0;
  sLiveActiveBullets = 0;
  sLiveNormalBullets = 0;
  sLiveBossActiveBullets = 0;
}

void closeFoes()
{
  int i;
  for (i = 0; i < foeActiveCount; i++)
  {
    Foe *fe = &(foe[foeActiveIndices[i]]);
    if (fe->cmd)
      delete fe->cmd;
  }
}

static int foeIdx = FOE_MAX;

static Foe *getNextFoe()
{
  int i;
  for (i = 0; i < FOE_MAX; i++)
  {
    foeIdx--;
    if (foeIdx < 0)
      foeIdx = FOE_MAX - 1;
    if (foe[foeIdx].spc == NOT_EXIST)
      break;
  }
  if (i >= FOE_MAX)
    return nullptr;
  return &(foe[foeIdx]);
}

Foe *addFoe(int x, int y, Fxp rank, int d, int spd, int type, int shield,
            BulletMLParserBLB *parser)
{
  int i;
  Foe *fe = getNextFoe();
  if (!fe)
    return nullptr;

  fe->parser = parser;

  fe->cmd = createFoeCommand(parser, fe);
  if (!fe->cmd)
  {
    fe->spc = NOT_EXIST;
    return nullptr;
  }

  fe->pos.x = x;
  fe->pos.y = y;
  fe->spos = fe->ppos = fe->pos;
  fe->vel.x = fe->vel.y = 0;
  fe->rank = rank;
  fe->d = d;
  fe->spd = spd;
  fe->spc = FOE;
  fe->type = type;
  fe->shield = shield;
  fe->cnt = 0;
  fe->color = 0;
  fe->hit = 0;

  markFoeSlotActive(fe);

  foeCnt++;
  enNum[type]++;

  return fe;
}

Foe *addFoeBossActiveBullet(int x, int y, Fxp rank,
                            int d, int spd, BulletMLParserBLB *parser)
{
  const int totalProjectiles = sLiveActiveBullets + sLiveNormalBullets + sLiveBossActiveBullets;
  if (sLiveBossActiveBullets >= kMaxBossActiveBullets ||
      totalProjectiles >= kMaxTotalProjectiles)
  {
    bulletSpawnFailed++;
    return nullptr;
  }

  Foe *fe = addFoe(x, y, rank, d, spd, BOSS_TYPE, 0, parser);
  if (!fe)
    return nullptr;
  foeCnt--;
  enNum[BOSS_TYPE]--;
  fe->spc = BOSS_ACTIVE_BULLET;
  sLiveBossActiveBullets++;
  return fe;
}

void addFoeActiveBullet(Vector *pos, Fxp rank,
                        int d, int spd, int color, BulletMLState *state)
{
  const int totalProjectiles = sLiveActiveBullets + sLiveNormalBullets + sLiveBossActiveBullets;
  if (sLiveActiveBullets >= kMaxActiveBullets ||
      totalProjectiles >= kMaxTotalProjectiles)
  {
    bulletSpawnFailed++;
    return;
  }

  Foe *fe = getNextFoe();
  if (!fe)
  {
    bulletSpawnFailed++;
    return;
  }
  fe->cmd = createFoeCommand(state, fe);
  if (!fe->cmd)
  {
    bulletSpawnFailed++;
    return;
  }
  fe->spos = fe->ppos = fe->pos = *pos;
  fe->vel.x = fe->vel.y = 0;
  fe->rank = rank;
  fe->d = d;
  fe->spd = spd;
  fe->spc = ACTIVE_BULLET;
  fe->type = 0;
  fe->cnt = 0;
  fe->color = color;
  markFoeSlotActive(fe);
  bulletSpawnedActive++;
  sLiveActiveBullets++;
}

void addFoeNormalBullet(Vector *pos, Fxp rank, int d, int spd, int color)
{
  const int totalProjectiles = sLiveActiveBullets + sLiveNormalBullets + sLiveBossActiveBullets;
  if (sLiveNormalBullets >= kMaxNormalBullets ||
      totalProjectiles >= kMaxTotalProjectiles)
  {
    bulletSpawnFailed++;
    return;
  }

  Foe *fe = getNextFoe();
  if (!fe)
  {
    bulletSpawnFailed++;
    return;
  }
  fe->cmd = nullptr;
  fe->spos = fe->ppos = fe->pos = *pos;
  fe->vel.x = fe->vel.y = 0;
  fe->rank = rank;
  fe->d = d;
  fe->spd = spd;
  fe->spc = BULLET;
  fe->type = 0;
  fe->cnt = 0;
  fe->color = color;
  markFoeSlotActive(fe);
  bulletSpawnedNormal++;
  sLiveNormalBullets++;
}

#define BULLET_WIPE_WIDTH 7200

static void wipeBullets(Vector *pos, int width)
{
  int i;
  Foe *fe;
  for (i = 0; i < foeActiveCount;)
  {
    fe = &(foe[foeActiveIndices[i]]);
    if (fe->spc != ACTIVE_BULLET && fe->spc != BULLET)
    {
      i++;
      continue;
    }
    if (vctDist(pos, &(fe->pos)) < width)
    {
      addBonus(&(fe->pos), &(fe->mv));
      removeFoeForced(fe);
      continue;
    }
    i++;
  }
}

static int foeSize[] = {30 / SCREEN_DIVISOR, 40 / SCREEN_DIVISOR, 56 / SCREEN_DIVISOR, 96 / SCREEN_DIVISOR};
static int foeScanSize[] = {
    foeSize[0] * 256 * SCAN_WIDTH / LAYER_WIDTH / 4 * 3,
    foeSize[1] * 256 * SCAN_WIDTH / LAYER_WIDTH / 4 * 3,
    foeSize[2] * 256 * SCAN_WIDTH / LAYER_WIDTH / 4 * 3,
    foeSize[3] * 256 * SCAN_WIDTH / LAYER_WIDTH / 4 * 3,
};
#define SHOT_SCAN_HEIGHT (SHOT_HEIGHT * 256 * SCAN_HEIGHT / LAYER_HEIGHT / 2)
static int enemyScore[] = {500, 1000, 5000, 50000};

int processSpeedDownBulletsNum = DEFAULT_SPEED_DOWN_BULLETS_NUM;
int insanespeed = 0;
int nowait = 0;
static int sFoeUpdateCursor = 0;

void moveFoes()
{
  int j;
  Foe *fe;
  int foeNum = 0;
  int activeBulletNum = 0;
  int normalBulletNum = 0;
  int bossActiveBulletNum = 0;
  int mx, my;
  int wl;

#if HW_DEBUG
  static uint32_t sFoeProbeFrames = 0u;
  const bool probeFoe = (sFoeProbeFrames < 3u);
  if (probeFoe)
  {
    SRL::Logger::LogInfo("[FOE] begin frame=%lu active=%d cursor=%d", (unsigned long)sFoeProbeFrames, foeActiveCount, sFoeUpdateCursor);
  }
#endif

  int updateBudget = foeActiveCount;

  // Snapshot entity pressure for this tick before any removals.
  sFrameFoeCount = (uint32_t)foeActiveCount;
  sFrameBulletCount = (uint32_t)(sLiveActiveBullets + sLiveNormalBullets + sLiveBossActiveBullets);

#if NOIZ2SA_PERF_MODE
  if (status == IN_GAME || status == GAMEOVER || status == PAUSE)
  {
    if (updateBudget > NOIZ2SA_FOE_UPDATE_BUDGET)
    {
      updateBudget = NOIZ2SA_FOE_UPDATE_BUDGET;
    }
  }
#endif

  // Adaptive runtime budget (applied on top of compile-time PERF_MODE limit).
  // 0 means uncapped. When set, the cursor round-robin ensures all foes are
  // visited across consecutive frames even when the budget clips a single tick.
  if (sRuntimeFoeBudgetLimit > 0 && updateBudget > sRuntimeFoeBudgetLimit)
  {
    updateBudget = sRuntimeFoeBudgetLimit;
  }
  sFrameFoeBudget = (uint32_t)updateBudget;

  if (foeActiveCount > 0 && sFoeUpdateCursor >= foeActiveCount)
  {
    sFoeUpdateCursor = 0;
  }

  int i = sFoeUpdateCursor;
  int processed = 0;
  while (processed < updateBudget && foeActiveCount > 0)
  {
    if (i >= foeActiveCount)
    {
      i = 0;
    }

    processed++;
    fe = &(foe[foeActiveIndices[i]]);

#if HW_DEBUG
    if (probeFoe && processed <= 20)
    {
      SRL::Logger::LogInfo("[FOE] step p=%d i=%d type=%d spc=%d cmd=%d", processed, i, fe->type, fe->spc, fe->cmd ? 1 : 0);
    }
#endif

    if (fe->type < 0 || fe->type >= FOE_TYPE_MAX)
    {
      removeFoeForced(fe);
      continue;
    }

    if (fe->cmd)
    {
      if (fe->type == BOSS_TYPE)
      {
        if (fe->cmd->isEnd())
        {
          delete fe->cmd;
          fe->cmd = createFoeCommand(fe->parser, fe);
          if (!fe->cmd)
          {
            removeFoeForced(fe);
            continue;
          }
          // fe->cmd->reset();
        }
      }
      // Iter J: Skip BulletML step for ACTIVE_BULLET and BOSS_ACTIVE_BULLET on odd ticks.
      // FOE entities always run to ensure correct bullet-spawn timing.
      const bool skipBulletML = ((fe->spc == ACTIVE_BULLET || fe->spc == BOSS_ACTIVE_BULLET) &&
                                 (tick & 1) != 0);
      if (!skipBulletML)
      {
#if HW_DEBUG
        if (probeFoe && processed <= 20)
        {
          SRL::Logger::LogInfo("[FOE] cmd-run p=%d hwfree=%u", processed, (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
        }
#endif
        fe->cmd->run();
#if HW_DEBUG
        if (probeFoe && processed <= 20)
        {
          SRL::Logger::LogInfo("[FOE] cmd-done p=%d", processed);
        }
#endif
      }
      if (fe->spc == NOT_EXIST)
      {
        if (fe->cmd)
        {
          delete fe->cmd;
          fe->cmd = nullptr;
        }
        continue;
      }
    }
    const int dir = fe->d & (DIV - 1);
    mx = ((sctbl[dir] * fe->spd) >> 8) + fe->vel.x;
    my = -((sctbl[(dir + 256) & (DIV - 1)] * fe->spd) >> 8) + fe->vel.y;
    fe->pos.x += mx;
    fe->pos.y += my;
    fe->mv.x = mx;
    fe->mv.y = my;
    wl = 2;
    if (fe->cnt < 4)
      wl = 0;
    else if (fe->cnt < 8)
      wl = 1;
    fe->ppos.x = fe->pos.x - (mx << wl);
    fe->ppos.y = fe->pos.y - (my << wl);
    fe->cnt++;

#if HW_DEBUG
    if (probeFoe && processed <= 20)
    {
      SRL::Logger::LogInfo("[FOE] moved p=%d cnt=%d", processed, fe->cnt);
    }
#endif

    if (fe->spc == FOE)
    {
      fe->hit = 0;
      // Check if the shot hits the foe.
      for (j = 0; j < SHOT_MAX; j++)
      {
        if (shot[j].cnt != NOT_EXIST)
        {
          if (absN(fe->pos.x - shot[j].pos.x) < foeScanSize[fe->type] &&
              absN(fe->pos.y - shot[j].pos.y) < foeScanSize[fe->type] + SHOT_SCAN_HEIGHT)
          {
            shot[j].cnt = NOT_EXIST;
            fe->shield--;
            fe->hit = 1;
            addShotFrag(&shot[j].pos);

            if (fe->shield <= 0)
            {
              addScore(enemyScore[fe->type]);
              wipeBullets(&(fe->pos), BULLET_WIPE_WIDTH * (fe->type + 1));
              addEnemyFrag(&(fe->pos), mx, my, fe->type);
              if (fe->type == BOSS_TYPE)
              {
                bossDestroied();
                playChunk(3);
              }
              else
              {
                playChunk(2);
              }
              removeFoeForced(fe);
              continue;
            }
            playChunk(1);
          }
        }
      }
    }
    else
    {
      if (fe->spc == ACTIVE_BULLET)
      {
        activeBulletNum++;
      }
      else if (fe->spc == BULLET)
      {
        normalBulletNum++;
      }
      else if (fe->spc == BOSS_ACTIVE_BULLET)
      {
        bossActiveBulletNum++;
      }

      const bool didHit = bulletHitsShip(fe);

    #if HW_DEBUG
      if (probeFoe && processed <= 20)
      {
        SRL::Logger::LogInfo("[FOE] shipcheck p=%d hit=%d", processed, didHit ? 1 : 0);
      }
    #endif

      if (didHit)
      {
        bulletRemovedHitShip++;
        destroyShip();
      }
    }
    if (fe->ppos.x < 0 || fe->ppos.x >= SCAN_WIDTH_8 ||
        fe->ppos.y < 0 || fe->ppos.y >= SCAN_HEIGHT_8)
    {
      if (fe->spc == ACTIVE_BULLET || fe->spc == BULLET || fe->spc == BOSS_ACTIVE_BULLET)
      {
        bulletRemovedOffscreen++;
      }
      removeFoeForced(fe);
      continue;
    }
    foeNum++;
    i++;
  }

  sFoeUpdateCursor = (foeActiveCount > 0) ? (i % foeActiveCount) : 0;

#if HW_DEBUG
  if (probeFoe)
  {
    SRL::Logger::LogInfo("[FOE] end frame=%lu processed=%d foeNum=%d active=%d cursor=%d", (unsigned long)sFoeProbeFrames, processed, foeNum, foeActiveCount, sFoeUpdateCursor);
    sFoeProbeFrames++;
  }
#endif

  // A game speed becomes slow as many bullets appears.
  interval = INTERVAL_BASE;
#if !NOIZ2SA_DISABLE_BULLET_SLOWDOWN
  if (!insane && !nowait && foeNum > processSpeedDownBulletsNum)
  {
    interval += (foeNum - processSpeedDownBulletsNum) * INTERVAL_BASE /
                processSpeedDownBulletsNum;
    if (interval > INTERVAL_BASE * 2)
      interval = INTERVAL_BASE * 2;
  }
#endif
}

void clearFoes()
{
  int i;
  Foe *fe;
  for (i = 0; i < foeActiveCount;)
  {
    fe = &(foe[foeActiveIndices[i]]);
    addClearFrag(&(fe->pos), &(fe->mv));
    removeFoeForced(fe);
  }
}

void clearFoesZako()
{
  int i;
  Foe *fe;
  for (i = 0; i < foeActiveCount;)
  {
    fe = &(foe[foeActiveIndices[i]]);
    if (fe->spc == NOT_EXIST ||
        fe->type == BOSS_TYPE || fe->spc == BOSS_ACTIVE_BULLET)
    {
      i++;
      continue;
    }
    addClearFrag(&(fe->pos), &(fe->mv));
    removeFoeForced(fe);
  }
}

void drawBulletsWake()
{
  int i;
  Foe *fe;
  int x, y, sx, sy;
  for (i = 0; i < foeActiveCount; i++)
  {
    fe = &(foe[foeActiveIndices[i]]);
    if (fe->spc == NOT_EXIST || fe->spc == FOE || fe->cnt >= 64)
      continue;
    x = (fe->pos.x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
    y = (fe->pos.y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
    sx = (fe->spos.x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
    sy = (fe->spos.y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;

    // Skip wake lines fully outside the playfield on the same side.
    if ((x < 0 && sx < 0) ||
        (x >= LAYER_WIDTH && sx >= LAYER_WIDTH) ||
        (y < 0 && sy < 0) ||
        (y >= LAYER_HEIGHT && sy >= LAYER_HEIGHT))
    {
      continue;
    }

    drawLine(x, y, sx, sy, 13 - fe->cnt / 5, 1, buf);
  }
}

static int foeColor[][2] = {
    {16 * 4 - 1, 16 * 10 - 7}, {16 * 2 - 1, 16 * 8 - 7}, {16 * 6 - 1, 16 * 12 - 7}, {16 * 1 - 11, 16 * 1 - 4}};

#define FOE_HIT_COLOR 16 * 3 - 1

void drawFoes()
{
  int i, j;
  Foe *fe;
  int x, y, px, py;
  int sz, cl1, cl2;
  int d, md, di;
  for (i = 0; i < foeActiveCount; i++)
  {
    fe = &(foe[foeActiveIndices[i]]);
    if (fe->spc != FOE)
      continue;
    x = (fe->pos.x / SCAN_WIDTH * LAYER_WIDTH) >> 8;
    y = (fe->pos.y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8;
    if (fe->cnt < 16)
    {
      sz = (foeSize[fe->type] * fe->cnt) >> 4;
    }
    else
    {
      sz = foeSize[fe->type];
    }
    cl1 = foeColor[fe->type][0];
    cl2 = foeColor[fe->type][1];
    if (fe->hit)
    {
      cl1 = FOE_HIT_COLOR;
    }
    drawBox(x, y, sz, sz, cl1, cl2, buf);
    d = (fe->cnt * 8) << 4;
    md = (DIV << 4) / fe->shield;
    sz /= 3;
    for (j = 0; j < fe->shield; j++, d += md)
    {
      di = (d >> 4) & (DIV - 1);
      drawBox(x + ((sctbl[di] * sz) >> 7), y + ((sctbl[di + DIV / 4] * sz) >> 7), sz, sz, cl1, cl2, buf);
    }
  }
}

#define BULLET_COLOR_NUM 3

static int bulletColor[BULLET_COLOR_NUM][2] = {
    {16 * 14 - 1, 16 * 2 - 1},
    {16 * 16 - 1, 16 * 4 - 1},
    {16 * 12 - 1, 16 * 6 - 1},
};

#define BULLET_WIDTH (6 / SCREEN_DIVISOR)

static void drawBox3x3(int x, int y,
                       Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *target)
{
  int ptr;
  int width = 3;
  int height = 3;

  x--;
  y--;
  if (x < 0)
  {
    width += x;
    x = 0;
  }
  if (x + width >= LAYER_WIDTH)
  {
    width = LAYER_WIDTH - x;
  }
  if (width <= 1)
    return;
  if (y < 0)
  {
    height += y;
    y = 0;
  }
  if (y + height > LAYER_HEIGHT)
  {
    height = LAYER_HEIGHT - y;
  }
  if (height <= 1)
    return;

  const int dirtyX = x;
  const int dirtyY = y;
  const int dirtyWidth = width;
  const int dirtyHeight = height;

  ptr = x + y * LAYER_WIDTH;
  target[ptr] = color2;
  target[ptr + 1] = color2;
  if (width > 2)
    target[ptr + 2] = color2;

  ptr += LAYER_WIDTH;
  target[ptr] = color2;
  target[ptr + 1] = color1;
  if (width > 2)
    target[ptr + 2] = color2;

  if (height > 2)
  {
    ptr += LAYER_WIDTH;
    target[ptr] = color2;
    target[ptr + 1] = color1;
    if (width > 2)
      target[ptr + 2] = color2;
  }

  if (target == buf)
  {
    markPlayfieldDirtyRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
  }
}

void drawBullets()
{
  int i;
  Foe *fe;
  int x, y;
  int bc;
  int drawnBulletNum = 0;
  int culledBulletNum = 0;
  for (i = 0; i < foeActiveCount; i++)
  {
    fe = &(foe[foeActiveIndices[i]]);
    if (fe->spc == NOT_EXIST || fe->spc == FOE)
      continue;
    bc = fe->color % BULLET_COLOR_NUM;
    x = ((fe->pos.x / SCAN_WIDTH * LAYER_WIDTH) >> 8) - (BULLET_WIDTH / 2);
    y = ((fe->pos.y / SCAN_HEIGHT * LAYER_HEIGHT) >> 8) - (BULLET_WIDTH / 2);

    // Cull bullets whose bounding box is entirely off-screen.
    // Their simulation (position, BulletML) already ran in moveFoes(); only the draw is skipped.
    if (x + BULLET_WIDTH <= 0 || x >= LAYER_WIDTH ||
        y + BULLET_WIDTH <= 0 || y >= LAYER_HEIGHT)
    {
      culledBulletNum++;
      continue;
    }

    drawBox3x3(x, y, bulletColor[bc][0], bulletColor[bc][1], buf);
    drawnBulletNum++;
  }
  sFrameDrawnBullets = (uint32_t)drawnBulletNum;
  sFrameCulledBullets = (uint32_t)culledBulletNum;
}

void consumeFoePressureStats(FoePressureStats *outStats)
{
  outStats->foeCount      = sFrameFoeCount;
  outStats->bulletCount   = sFrameBulletCount;
  outStats->drawnBullets  = sFrameDrawnBullets;
  outStats->culledBullets = sFrameCulledBullets;
  outStats->foeBudget     = sFrameFoeBudget;
  sFrameFoeCount = sFrameBulletCount = sFrameDrawnBullets = sFrameCulledBullets = 0;
  sFrameFoeBudget = 0;
}

void drawBulletDebugOverlay()
{
  // Keep hook available for future diagnostics, disabled by default.
}
