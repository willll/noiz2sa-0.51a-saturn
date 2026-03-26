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
#define randN(N) (g_random->GetNumber(0, (N)-1))
#define randNS(N) (g_random->GetNumber(-(N), (N)-1))
#define randNS2(N) ((g_random->GetNumber(0, (N)-1) - ((N)>>1)) + (g_random->GetNumber(0, (N)-1) - ((N)>>1)))
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

void quitLast();
void initTitleStage(int stg);
void initTitle();
void initGame(int stg);
void initGameover();
void initStageClear();
void updateLoadingProgress(const char *step, int percent);

#endif // NOIZ2SA_H_
