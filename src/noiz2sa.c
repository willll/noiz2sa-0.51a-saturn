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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static int noSound = 1;
static int accframe = 0;

// Initialize and load preference.
static void initFirst() {
  //loadPreference(); TODO : Create default !!
  srand(SDL_GetTicks());
  initBarragemanager();
  initAttractManager();
}

// Quit and save preference.
void quitLast() {
  if ( !noSound ) closeSound();
  savePreference();
  closeBarragemanager();
  closeSDL();
  SDL_Quit();
  exit(1);
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
  int stg;
  status = TITLE;

  stg = initTitleAtr();
  initShip();
  initShots();
  initFrags();
  initBonuses();
  initBackground();
  setStageBackground(1);

  initTitleStage(stg);
}

void initGame(int stg) {
  status = IN_GAME;

  initShip();
  initShots();
  initFoes();
  initFrags();
  initBonuses();
  initBackground();

  initBarrages(stagePrm[stg][0], stagePrm[stg][1], stagePrm[stg][2]);
  initGameState(stg);
  if ( stg < STAGE_NUM ) {
    setStageBackground(stg%5+1);
    playMusic(stg%5+1);
  } else {
    if ( !insane ) {
      setStageBackground(0);
      playMusic(0);
    } else {
      setStageBackground(6);
      playMusic(6);
    }
  }
}

void initGameover() {
  status = GAMEOVER;
  initGameoverAtr();
}

void initStageClear() {
  status = STAGE_CLEAR;
  initStageClearAtr();
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


static void parseArgs(int argc, char *argv[]) {

    noSound = 1;
    windowMode = 1;
    buttonReversed = 1;
    brightness = DEFAULT_BRIGHTNESS;
    nowait = 0;
    accframe = 0;
}

int interval = INTERVAL_BASE;
int tick = 0;
static int pPrsd = 1;

int main(int argc, char *argv[]) {
  int done = 0;
  long prvTickCount = 0;
  int i;
  SDL_Event event;
  long nowTick;
  int frame;

  parseArgs(argc, argv);

  // initDegutil();
  // initSDL(windowMode);
  // if ( !noSound ) initSound();
  // initFirst();
  // initTitle();
  //
  // while ( !done ) {
  //   int startBtn = 0, backBtn = 0;
  //   SDL_PollEvent(&event);
  //   keys = SDL_GetKeyState(NULL);
  //   startBtn = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_START);
  //   backBtn = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_BACK);
  //   if ( keys[SDLK_ESCAPE] == SDL_PRESSED || event.type == SDL_QUIT || backBtn ) done = 1;
  //   if ( keys[SDLK_p] == SDL_PRESSED || startBtn ) {
  //     if ( !pPrsd ) {
	// if ( status == IN_GAME ) {
	//   status = PAUSE;
	// } else if ( status == PAUSE ) {
	//   status = IN_GAME;
	// }
  //     }
  //     pPrsd = 1;
  //   } else {
  //     pPrsd = 0;
  //   }
  //
  //   nowTick = SDL_GetTicks();
  //   frame = (int)(nowTick-prvTickCount) / interval;
  //   if ( frame <= 0 ) {
  //     frame = 1;
  //     SDL_Delay(prvTickCount+interval-nowTick);
  //     if ( accframe ) {
	// prvTickCount = SDL_GetTicks();
  //     } else {
	// prvTickCount += interval;
  //     }
  //   } else if ( frame > 5 ) {
  //     frame = 5;
  //     prvTickCount = nowTick;
  //   } else {
  //     prvTickCount += frame*interval;
  //   }
  //   for ( i=0 ; i<frame ; i++ ) {
  //     move();
  //     tick++;
  //   }
  //   smokeScreen();
  //   draw();
  //   flipScreen();
  // }
  quitLast();
}
