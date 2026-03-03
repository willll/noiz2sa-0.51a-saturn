/*
 * $Id: noiz2sa.c,v 1.8 2003/02/12 13:55:13 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Noiz2sa main routine.
 *
 * @version $Revision: 1.8 $
 */
#include "SDL.h"
#include <srl_memory.hpp>  // for malloc/free, atoi
#include <srl_log.hpp>     // for logging
#include <srl_string.hpp>  // for string functions
#include <srl_system.hpp>  // for exit

// Include Random class (must be AFTER SRL headers to avoid macro conflicts)
#include <impl/random.hpp>

#include "noiz2sa.h"
#include "screen.h"
#include "vector.h"
#include "ship.h"
#include "shot.h"
#include "frag.h"
#include "bonus.h"
#include "foe_mtd.h"
#include "brgmng_mtd.h"
#include "background.h"
#include "degutil.h"
#include "soundmanager.h"
#include "attractmanager.h"
#include "gamepad.h"

static int noSound = 0;

// Global random number generator (using SRL::Math namespace which is aliased to SaturnMath)
RandomGenerator* g_random = nullptr;

// Initialize and load preference.
static void initFirst() {
  SRL::Logger::LogInfo("[INIT] First initialization starting");
  
  loadPreference();
  SRL::Logger::LogDebug("[INIT] Preferences loaded");
  
  // Initialize random number generator with current time
  // Note: SaturnMath is #defined as SRL::Math in srl_base.hpp
  uint32_t seed = SDL_GetTicks();
  g_random = new SRL::Math::Random<unsigned int>(seed);
  SRL::Logger::LogDebug("[INIT] Random generator initialized with seed: %u", seed);
  
  initBarragemanager();
  SRL::Logger::LogDebug("[INIT] Barrage manager initialized");
  
  initAttractManager();
  SRL::Logger::LogInfo("[INIT] First initialization complete");
}

// Quit and save preference.
void quitLast() {
  SRL::Logger::LogInfo("[QUIT] Shutdown sequence starting");
  
  if ( !noSound ) {
    closeSound();
    SRL::Logger::LogDebug("[QUIT] Sound closed");
  }
  
  savePreference();
  SRL::Logger::LogDebug("[QUIT] Preferences saved");
  
  closeBarragemanager();
  SRL::Logger::LogDebug("[QUIT] Barrage manager closed");
  
  delete g_random;
  g_random = nullptr;
  SRL::Logger::LogInfo("[QUIT] Random generator cleaned up");
  
  SRL::Logger::LogInfo("[QUIT] Shutdown sequence complete - Exiting");
  SRL::System::Exit(1);
}

int status;

static float stagePrm[STAGE_NUM+ENDLESS_STAGE_NUM+1][3] = {
  {13, 0.5f, 0.12f}, {2, 1.8f, 0.15f}, {3, 3.2f, 0.1f}, {90, 6.0f, 0.3f}, {5, 5.0f, 0.6f},
  {6, 10.0f, 0.6f}, {7, 5.0f, 2.2f}, {98, 12.0f, 1.5f}, {9, 10.0f, 2.0f}, {79, 21.0f, 1.5f},
  {-3, 5.0f, 0.7f}, {-1, 10.0f, 1.2f}, {-4, 15.0f, 1.8f}, {-2, 16.0f, 1.8f},
  {0, -1.0f, 0.0f},
};

void initTitleStage(int stg) {
  initFoes();
  initBarrages(stagePrm[stg][0], stagePrm[stg][1], stagePrm[stg][2]);
}

void initTitle() {
  SRL::Logger::LogInfo("[STATE] Entering TITLE screen");
  
  int stg;
  status = TITLE;

  stg = initTitleAtr();
  SRL::Logger::LogDebug("[TITLE] Title attributes initialized (stage: %d)", stg);
  
  initShip();
  initShots();
  initFrags();
  initBonuses();
  initBackground();
  SRL::Logger::LogDebug("[TITLE] Game objects initialized");
  
  setStageBackground(1);
  initTitleStage(stg);
  
  SRL::Logger::LogInfo("[STATE] TITLE screen ready");
}

void initGame(int stg) {
  SRL::Logger::LogInfo("[STATE] Entering IN_GAME (stage %d)", stg);
  
  status = IN_GAME;

  initShip();
  initShots();
  initFoes();
  initFrags();
  initBonuses();
  initBackground();
  SRL::Logger::LogDebug("[GAME] Stage %d: Game objects initialized", stg);

  initBarrages(stagePrm[stg][0], stagePrm[stg][1], stagePrm[stg][2]);
  initGameState(stg);
  SRL::Logger::LogDebug("[GAME] Stage %d: Barrage initialized", stg);
  
  if ( stg < STAGE_NUM ) {
    setStageBackground(stg%5+1);
    playMusic(stg%5+1);
    SRL::Logger::LogDebug("[GAME] Stage %d: Background/Music set to %d", stg, stg%5+1);
  } else {
    if ( !insane ) {
      setStageBackground(0);
      playMusic(0);
    } else {
      setStageBackground(6);
      playMusic(6);
      SRL::Logger::LogDebug("[GAME] Endless stage: INSANE mode");
    }
  }
  
  SRL::Logger::LogInfo("[STATE] IN_GAME (stage %d) ready", stg);
}

void initGameover() {
  SRL::Logger::LogInfo("[STATE] Entering GAMEOVER");
  
  status = GAMEOVER;
  initGameoverAtr();
  
  SRL::Logger::LogInfo("[STATE] GAMEOVER screen ready");
}

void initStageClear() {
  SRL::Logger::LogInfo("[STATE] Entering STAGE_CLEAR");
  
  status = STAGE_CLEAR;
  initStageClearAtr();
  
  SRL::Logger::LogInfo("[STATE] STAGE_CLEAR screen ready");
}

static void move() {
  switch ( status ) {
  case TITLE:
    moveTitleMenu();
    moveBackground();
    addBullets();
    moveFoes();
    break;
  case IN_GAME:
    moveBackground();
    addBullets();
    moveShots();
    moveShip();
    moveFoes();
    moveFrags();
    moveBonuses();
    break;
  case GAMEOVER:
    moveGameover();
    moveBackground();
    addBullets();
    moveShots();
    moveFoes();
    moveFrags();
    break;
  case STAGE_CLEAR:
    moveStageClear();
    moveBackground();
    moveShots();
    moveShip();
    moveFrags();
    moveBonuses();
    break;
  case PAUSE:
    movePause();
    break;
  }
}

static void draw() {
  switch ( status ) {
  case TITLE:
    // Draw background.
    drawBackground();
    drawFoes();
    drawBulletsWake();
    blendScreen();
    // Draw forground.
    drawBullets();
    drawScore();
    drawTitleMenu();
    break;
  case IN_GAME:
    // Draw background.
    drawBackground();
    drawBonuses();
    drawFoes();
    drawBulletsWake();
    drawFrags();
    blendScreen();
    // Draw forground.
    drawShots();
    drawShip();
    drawBullets();
    drawScore();
    break;
  case GAMEOVER:
    // Draw background.
    drawBackground();
    drawFoes();
    drawBulletsWake();
    drawFrags();
    blendScreen();
    // Draw forground.
    drawShots();
    drawBullets();
    drawScore();
    drawGameover();
    break;
  case STAGE_CLEAR:
    // Draw background.
    drawBackground();
    drawBonuses();
    drawFrags();
    blendScreen();
    // Draw forground.
    drawShots();
    drawShip();
    drawScore();
    drawStageClear();
    break;
  case PAUSE:
    // Draw background.
    drawBackground();
    drawBonuses();
    drawFoes();
    drawBulletsWake();
    drawFrags();
    blendScreen();
    // Draw forground.
    drawShots();
    drawShip();
    drawBullets();
    drawScore();
    drawPause();
    break;
  }
}


static int accframe = 0;

// Initialize game configuration with Saturn-optimal defaults.
// Saturn doesn't support command-line arguments, so we set optimal settings directly.
static void initGameConfig() {
  // Sound: enabled by default (Saturn has good audio capabilities)
  noSound = 1;
  
  // Input: standard button configuration
  buttonReversed = 0;
  
  // Display brightness: default value
  brightness = DEFAULT_BRIGHTNESS;
  
  // Timing: no wait, standard framerate
  nowait = 0;
  accframe = 0;
}

int interval = INTERVAL_BASE;
int tick = 0;
static int pPrsd = 1;

int main() {
  SRL::Logger::LogInfo("[MAIN] Noiz2sa startup (v%d)", VERSION_NUM);
  
  int done = 0;
  long prvTickCount = 0;
  int i;
  SDL_Event event;
  long nowTick;
  int frame;

  SRL::Logger::LogDebug("[MAIN] Initializing game config");
  initGameConfig();

  SRL::Logger::LogDebug("[MAIN] Initializing degree utilities");
  initDegutil();
  
  SRL::Logger::LogDebug("[MAIN] Initializing SDL");
  initSDL();
  
  if ( !noSound ) {
    SRL::Logger::LogDebug("[MAIN] Initializing sound");
    initSound();
  } else {
    SRL::Logger::LogInfo("[MAIN] Sound disabled");
  }
  
  initFirst();
  initTitle();
  
  SRL::Logger::LogInfo("[MAIN] Main game loop starting");

  while ( !done ) {
    int startBtn = 0, backBtn = 0;
    SDL_PollEvent(&event);
    keys = (Uint8*)SDL_GetKeyState(NULL);
    startBtn = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_START);
    backBtn = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_BACK);
    if ( keys[SDLK_ESCAPE] == SDL_PRESSED || event.type == SDL_QUIT || backBtn ) done = 1;
    if ( keys[SDLK_p] == SDL_PRESSED || startBtn ) {
      if ( !pPrsd ) {
	if ( status == IN_GAME ) {
	  status = PAUSE;
	} else if ( status == PAUSE ) {
	  status = IN_GAME;
	}
      }
      pPrsd = 1;
    } else {
      pPrsd = 0;
    }

    nowTick = SDL_GetTicks();
    frame = (int)(nowTick-prvTickCount) / interval;
    if ( frame <= 0 ) {
      frame = 1;
      SDL_Delay(prvTickCount+interval-nowTick);
      if ( accframe ) {
	prvTickCount = SDL_GetTicks();
      } else {
	prvTickCount += interval;
      }
    } else if ( frame > 5 ) {
      frame = 5;
      prvTickCount = nowTick;
    } else {
      prvTickCount += frame*interval;
    }
    for ( i=0 ; i<frame ; i++ ) {
      move();
      tick++;
    }
    smokeScreen();
    draw();
    flipScreen();
  }
  quitLast();
}
