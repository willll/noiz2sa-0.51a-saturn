/*
 * $Id: brgmng_mtd.h,v 1.2 2002/12/31 09:34:34 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Stage data.
 *
 * @version $Revision: 1.2 $
 */

#include <srl.hpp>

using SRL::Math::Types::Fxp;

/** @brief Initialises the barrage manager. */
void initBarragemanager();
/** @brief Releases barrage-manager resources. */
void closeBarragemanager();
/** @brief Initialises barrage patterns for the supplied seed and level. */
void initBarrages(int seed, Fxp startLevel, Fxp li);
/** @brief Selects the active barrages for a stage/mid-stage state. */
void setBarrages(Fxp level, int bm, int midMode);
/** @brief Spawns regular bullets for the current barrage state. */
void addBullets();
/** @brief Spawns boss bullets for the current barrage state. */
void addBossBullet();
/** @brief Handles boss destruction cleanup and transitions. */
void bossDestroied();

extern int scene;
extern int endless, insane;
