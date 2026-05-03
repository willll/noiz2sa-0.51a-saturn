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
extern int insanespeed;
extern int nowait;

void initFoes();
void closeFoes();
void moveFoes();
void clearFoes();
void clearFoesZako();
void drawBulletsWake();
void drawFoes();
void drawBullets();
void drawBulletDebugOverlay();

struct FoePressureStats {
  uint32_t foeCount;      // total active entities (foes + bullets)
  uint32_t bulletCount;   // live projectile count (active + normal + boss)
  uint32_t drawnBullets;  // bullets drawn (passed screen bounds test)
  uint32_t culledBullets; // bullets skipped (entirely off-screen)
  uint32_t foeBudget;     // effective foe update budget used this tick
};
void consumeFoePressureStats(FoePressureStats *outStats);

// Set the maximum number of foe slots updated per moveFoes() tick.
// Pass 0 to remove the cap (process all active foes every frame).
void setFoeBudgetLimit(int limit);

// Returns the total number of live projectiles (active + normal + boss active bullets).
int getLiveProjectileCount();
