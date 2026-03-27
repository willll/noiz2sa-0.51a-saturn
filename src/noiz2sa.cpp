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
#include <stdio.h>
#include <srl.hpp> // for malloc/free, atoi
#include <srl_log.hpp>    // for logging
#include <srl_system.hpp> // for exit

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

static void renderLoadingScreen(const char *step, int percent)
{
  if (percent < 0)
  {
    percent = 0;
  }
  else if (percent > 100)
  {
    percent = 100;
  }

  char bar[21];
  char percentLine[32];
  char barLine[32];
  char stepLine[48];
  const int filled = (percent * 20) / 100;
  for (int i = 0; i < 20; i++)
  {
    bar[i] = (i < filled) ? '#' : '-';
  }
  bar[20] = '\0';

  snprintf(percentLine, sizeof(percentLine), "Loading... %d%%", percent);
  snprintf(barLine, sizeof(barLine), "[%s]", bar);
  if (step != nullptr)
  {
    snprintf(stepLine, sizeof(stepLine), "%s", step);
  }
  else
  {
    stepLine[0] = '\0';
  }

  SRL::Debug::PrintColorSet(1);
  SRL::Debug::PrintClearScreen();
  SRL::Debug::Print(1, 1, "NOIZ2SA");
  SRL::Debug::Print(1, 3, percentLine);
  SRL::Debug::Print(1, 4, barLine);
  SRL::Debug::Print(1, 6, stepLine);
  SRL::Core::Synchronize();
}

void updateLoadingProgress(const char *step, int percent)
{
  renderLoadingScreen(step, percent);
}

static void clearLoadingOverlay()
{
  SRL::Debug::PrintClearScreen();
  SRL::Debug::PrintColorRestore();
}

// Global random number generator (using SRL::Math namespace which is aliased to SaturnMath)
RandomGenerator *g_random = nullptr;

// Initialize and load preference.
static void initFirst()
{
  SRL::Logger::LogInfo("[INIT] First initialization starting");
  updateLoadingProgress("Loading preferences", 30);

  loadPreference();
  SRL::Logger::LogDebug("[INIT] Preferences loaded");

  // Initialize random number generator with current time
  // Note: SaturnMath is #defined as SRL::Math in srl_base.hpp
  uint32_t seed = SDL_GetTicks();
  g_random = new SRL::Math::Random<unsigned int>(seed);
  SRL::Logger::LogDebug("[INIT] Random generator initialized with seed: %u", seed);

  updateLoadingProgress("Loading barrages", 35);
  initBarragemanager();
  SRL::Logger::LogDebug("[INIT] Barrage manager initialized");

  updateLoadingProgress("Loading attract manager", 96);
  initAttractManager();
  SRL::Logger::LogDebug("[INIT] Attract manager initialized");
  updateLoadingProgress("Initialization complete", 100);
  SRL::Logger::LogInfo("[INIT] First initialization complete");
}

// Quit and save preference.
void quitLast()
{
  SRL::Logger::LogInfo("[QUIT] Shutdown sequence starting");

  if (!noSound)
  {
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

static float stagePrm[STAGE_NUM + ENDLESS_STAGE_NUM + 1][3] = {
    {13, 0.5f, 0.12f},
    {2, 1.8f, 0.15f},
    {3, 3.2f, 0.1f},
    {90, 6.0f, 0.3f},
    {5, 5.0f, 0.6f},
    {6, 10.0f, 0.6f},
    {7, 5.0f, 2.2f},
    {98, 12.0f, 1.5f},
    {9, 10.0f, 2.0f},
    {79, 21.0f, 1.5f},
    {-3, 5.0f, 0.7f},
    {-1, 10.0f, 1.2f},
    {-4, 15.0f, 1.8f},
    {-2, 16.0f, 1.8f},
    {0, -1.0f, 0.0f},
};

void initTitleStage(int stg)
{
  initFoes();
  initBarrages(stagePrm[stg][0], stagePrm[stg][1], stagePrm[stg][2]);
}

void initTitle()
{
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

void initGame(int stg)
{
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
  SRL::Logger::LogDebug("[GAME] Stage %d: Barrage initialized (enemies=%d, speed=%.2f, density=%.2f)", stg, (int)stagePrm[stg][0], stagePrm[stg][1], stagePrm[stg][2]);

  if (stg < STAGE_NUM)
  {
    setStageBackground(stg % 5 + 1);
    playMusic(stg % 5 + 1);
    SRL::Logger::LogDebug("[GAME] Stage %d: Background/Music set to %d", stg, stg % 5 + 1);
  }
  else
  {
    if (!insane)
    {
      setStageBackground(0);
      playMusic(0);
    }
    else
    {
      setStageBackground(6);
      playMusic(6);
      SRL::Logger::LogInfo("[GAME] Endless stage: INSANE mode activated");
    }
  }

  SRL::Logger::LogInfo("[STATE] IN_GAME (stage %d) ready - Starting gameplay", stg);
}

void initGameover()
{
  SRL::Logger::LogInfo("[STATE] Entering GAMEOVER");

  status = GAMEOVER;
  initGameoverAtr();

  SRL::Logger::LogInfo("[STATE] GAMEOVER screen ready - Game Over!");
}

void initStageClear()
{
  SRL::Logger::LogInfo("[STATE] Entering STAGE_CLEAR");

  status = STAGE_CLEAR;
  initStageClearAtr();

  SRL::Logger::LogInfo("[STATE] STAGE_CLEAR screen ready - Stage Completed!");
}

static void move()
{
  // Periodic debug logging to track game state execution
  // Log every 180 frames (~3 seconds at 60Hz) to avoid spam
  static int move_frame_counter = 0;
  move_frame_counter++;
  
  if ((move_frame_counter % 180) == 0)
  {
    const char* state_name = "UNKNOWN";
    switch (status)
    {
      case TITLE: state_name = "TITLE"; break;
      case IN_GAME: state_name = "IN_GAME"; break;
      case GAMEOVER: state_name = "GAMEOVER"; break;
      case STAGE_CLEAR: state_name = "STAGE_CLEAR"; break;
      case PAUSE: state_name = "PAUSE"; break;
    }
    SRL::Logger::LogDebug("[MOVE] Processing state: %s (frame: %d)", state_name, move_frame_counter);
  }
  
  switch (status)
  {
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

static void draw()
{
  memset(l1buf, 0, lyrSize);
  memset(l2buf, 0, lyrSize);

  // Periodic debug logging to track rendering execution
  // Log every 180 frames (~3 seconds at 60Hz) to avoid spam
  static int draw_frame_counter = 0;
  
  if ((draw_frame_counter % 180) == 0)
  {
    const char* state_name = "UNKNOWN";
    switch (status)
    {
      case TITLE: state_name = "TITLE"; break;
      case IN_GAME: state_name = "IN_GAME"; break;
      case GAMEOVER: state_name = "GAMEOVER"; break;
      case STAGE_CLEAR: state_name = "STAGE_CLEAR"; break;
      case PAUSE: state_name = "PAUSE"; break;
    }
    SRL::Logger::LogDebug("[DRAW] Rendering state: %s (frame: %d)", state_name, draw_frame_counter);
  }
  
  draw_frame_counter++;

  switch (status)
  {
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
static void initGameConfig()
{
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

int main()
{
  SRL::Logger::LogInfo("[MAIN] Noiz2sa startup (v%d)", VERSION_NUM);

  int done = 0;
  long prvTickCount = 0;
  int i;
  SDL_Event event;
  long nowTick;
  int frame;

  // Initialize the SRL core (graphics/video setup?)
  // HighColor(20,10,50) likely sets background color in high-color mode (5-5-5 RGB?).
  SRL::Core::Initialize(SRL::Types::HighColor(20, 10, 50));

  // SRL::CRAM::Palette palette(SRL::CRAM::TextureColorMode::Paletted16, 1);

  // //palette.GetData()[0] = SRL::Types::HighColor(255, 0, 0); // Red
  // palette.GetData()[1] = SRL::Types::HighColor(255, 0, 0); // Red
  // palette.GetData()[2] = SRL::Types::HighColor(255, 0, 0); // Red

  // SRL::CRAM::SetBankUsedState(1, SRL::CRAM::TextureColorMode::Paletted16, true);

  // SRL::Debug::PrintColorSet(1);

  // SRL::Debug::Print(1,1,"LOADING");

  // SRL::Core::Synchronize();

  SRL::Logger::LogDebug("[MAIN] Initializing game config");
  updateLoadingProgress("Initializing game config", 2);
  initGameConfig();

  SRL::Logger::LogDebug("[MAIN] Initializing degree utilities");
  updateLoadingProgress("Initializing math utilities", 5);
  initDegutil();

  SRL::Logger::LogDebug("[MAIN] Initializing SDL");
  updateLoadingProgress("Initializing screen", 10);
  initSDL();

  if (!noSound)
  {
    SRL::Logger::LogDebug("[MAIN] Initializing sound");
    updateLoadingProgress("Initializing sound", 26);
    initSound();
  }
  else
  {
    SRL::Logger::LogInfo("[MAIN] Sound disabled");
    updateLoadingProgress("Sound disabled", 26);
  }

  initFirst();
  updateLoadingProgress("Entering title", 100);
  initTitle();
  clearLoadingOverlay();

  initGamepad();
  SRL::Logger::LogDebug("[MAIN] Gamepad initialized");

  SRL::Logger::LogInfo("[MAIN] Main game loop starting");

  while (!done)
  {
    // Handle pause/unpause input
    if (gamepad && gamepad->IsConnected() && gamepad->WasPressed(SRL::Input::Digital::Button::START))
    {
      if (!pPrsd)
      {
        if (status == IN_GAME)
        {
          status = PAUSE;
          SRL::Logger::LogDebug("[INPUT] START pressed - Game paused (frame: %d)", tick);
        }
        else if (status == PAUSE)
        {
          status = IN_GAME;
          SRL::Logger::LogDebug("[INPUT] START pressed - Game resumed (frame: %d)", tick);
        }
      }
      pPrsd = 1;
    }
    else
    {
      pPrsd = 0;
    }

    // Calculate frames to process based on time elapsed
    nowTick = SDL_GetTicks();
    frame = (int)(nowTick - prvTickCount) / interval;
    
    if (frame <= 0)
    {
      frame = 1;
      SDL_Delay(prvTickCount + interval - nowTick);
      if (accframe)
      {
        prvTickCount = SDL_GetTicks();
      }
      else
      {
        prvTickCount += interval;
      }
    }
    else if (frame > 5)
    {
      SRL::Logger::LogWarning("[TIMING] Frame skip detected: %d frames (tick: %ld)", frame, nowTick);
      frame = 5;
      prvTickCount = nowTick;
    }
    else
    {
      prvTickCount += frame * interval;
    }

    // Process game logic for calculated frames
    for (i = 0; i < frame; i++)
    {
      move();
      tick++;
    }

    smokeScreen();

    draw();

    flipScreen();
    
    // Refresh screen
    SRL::Core::Synchronize();

    // Periodic logging for frame rate monitoring
    if ((tick % 300) == 0)  // Every ~5 seconds at 60Hz
    {
      SRL::Logger::LogDebug("[FRAME] Game tick: %d, Status: %d, Time: %ums", tick, status, SDL_GetTicks());
    }
  }
  quitLast();
}
