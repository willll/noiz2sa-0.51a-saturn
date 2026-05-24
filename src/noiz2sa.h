/*
 * $Id: noiz2sa.h,v 1.4 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Noiz2sa header file.
 *
 * @version $Revision: 1.4 $
 */
#ifndef NOIZ2SA_H_
#define NOIZ2SA_H_

#include <stdint.h>

// Forward declarations
namespace SRL { namespace Math { template<typename T> class Random; } }

// Type alias for random generator (avoids macro conflicts)
typedef SRL::Math::Random<unsigned int> RandomGenerator;

// Global random number generator (initialized in initFirst)
extern RandomGenerator* g_random;

// Random number generation macros
#define randN(N) (((N) > 0) ? (int)g_random->GetNumber(0u, (unsigned int)((N) - 1)) : 0)
#define randNS(N) (((N) > 0) ? ((int)g_random->GetNumber(0u, (unsigned int)((N) * 2 - 1)) - (N)) : 0)
#define randNS2(N) ((randNS((N)) + randNS((N))))
#define absN(a) ((a) < 0 ? - (a) : (a))

#define INTERVAL_BASE 16

#define CAPTION "Noiz2sa"
#define VERSION_NUM 50

#define NOT_EXIST -999999

extern int status;
extern int interval;
extern int tick;

#define TITLE 0
#define IN_GAME 1
#define GAMEOVER 2
#define STAGE_CLEAR 3
#define PAUSE 4

/**
 * @brief Cleans up the current scene before exiting or switching modes.
 */
void quitLast();

/**
 * @brief Initialises the title flow for a specific stage.
 * @param stg Stage index to display on the title screen.
 */
void initTitleStage(int stg);

/**
 * @brief Initialises the title screen state.
 */
void initTitle();

/**
 * @brief Initialises a new game session for the specified stage.
 * @param stg Starting stage index.
 */
void initGame(int stg);

/**
 * @brief Initialises the game-over scene.
 */
void initGameover();

/**
 * @brief Initialises the stage-clear scene.
 */
void initStageClear();

/**
 * @brief Updates the loading screen message and percentage.
 * @param step   Current loading step text.
 * @param percent Progress percentage in the range 0-100.
 */
void updateLoadingProgress(const char *step, int percent);

#endif // NOIZ2SA_H_
