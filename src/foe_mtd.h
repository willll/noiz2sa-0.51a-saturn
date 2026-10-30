/*
 * $Id: foe_mtd.h,v 1.4 2003/02/09 07:34:15 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Foe methods.
 *
 * @version $Revision: 1.4 $
 */
#pragma once
#include <stdint.h>

#define FOE_ENEMY_POS_RATIO 1024

#define DEFAULT_SPEED_DOWN_BULLETS_NUM 100
#define EASY_SPEED_DOWN_BULLETS_NUM 80
#define HARD_SPEED_DOWN_BULLETS_NUM 120

extern int processSpeedDownBulletsNum;
extern int nowait;

/** @brief Allocates the foe[] pool from LWRAM. Call exactly once at startup,
 *  before the first initFoes() call. Returns false (and logs fatal) on
 *  allocation failure. */
bool allocateFoePool();
/** @brief Initialises foe state and allocators. */
void initFoes();
/** @brief Releases all foe state. */
void closeFoes();
/** @brief Advances all active foes and bullets. */
void moveFoes();
/** @brief Clears every foe and bullet from the field. */
void clearFoes();
/** @brief Clears only zako enemies. */
void clearFoesZako();
/** @brief Draws bullet wake effects. */
void drawBulletsWake();
/** @brief Draws all active foes. */
void drawFoes();
/** @brief Draws all active bullets. */
void drawBullets();
/** @brief Draws the debug overlay for foe bullets. */
void drawBulletDebugOverlay();

struct FoePressureStats {
  uint32_t foeCount;         // total active entities (foes + bullets)
  uint32_t bulletCount;      // live projectile count (active + normal + boss)
  uint32_t drawnBullets;     // bullets drawn (passed screen bounds test)
  uint32_t culledBullets;    // bullets skipped (entirely off-screen)
  uint32_t foeBudget;        // effective foe update budget used this tick
  uint32_t nonBulletFoeCount; // foeCount - bulletCount: actual ZAKO/MIDDLE/BOSS
                              // enemies, uncapped unlike bullets (see foe.cc
                              // kMaxTotalProjectiles comment) - the suspected
                              // driver of the real-hardware move-phase FPS collapse
  uint32_t bossTypeFoeCount;  // enNum[BOSS_TYPE]: BOSS-pattern entities specifically,
                              // documented at ~5ms/tick each on SH-2 (no FPU)
};
/** @brief Copies the latest foe pressure statistics into the output structure. */
void consumeFoePressureStats(FoePressureStats *outStats);

// Set the maximum number of foe slots updated per moveFoes() tick.
// Pass 0 to remove the cap (process all active foes every frame).
/** @brief Sets the maximum number of foe slots updated per frame. */
void setFoeBudgetLimit(int limit);

// Returns the total number of live projectiles (active + normal + boss active bullets).
/** @brief Returns the total number of live projectiles. */
int getLiveProjectileCount();

/** @brief Returns the number of active foe slots. */
int getActiveFoeCount();
/** @brief Returns the total foe pool capacity. */
int getFoePoolCapacity();
/** @brief Returns the cumulative count of spawns rejected by the non-bullet foe cap. */
unsigned long getFoeSpawnCappedCount();

/** @brief Returns the number of cached FoeCommand nodes kept for reuse. */
int getFoeCommandPoolCachedCount();
