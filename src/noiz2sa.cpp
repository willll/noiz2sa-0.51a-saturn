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
#include <srl_timer.hpp>
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
#include "loading_screen.h"


static int noSound = 0;

// Thin wrapper kept for compatibility with callers in other translation units
// (barragemanager.cc, screen.cpp).  All logic lives in LoadingScreen.
void updateLoadingProgress(const char *step, int percent)
{
  g_loadingScreen.Update(step, percent);
}

// Global random number generator (using SRL::Math namespace which is aliased to SaturnMath)
RandomGenerator *g_random = nullptr;

// Initialize and load preference.
static void initFirst()
{
  const char* steps[] = {
    "Loading preferences",
    "Initializing random generator",
    "Loading barrages",
    "Loading attract manager",
    "Initialization complete"
  };
  const int numSteps = sizeof(steps) / sizeof(steps[0]);
  int stepIdx = 0;

  SRL::Logger::LogInfo("[INIT] First initialization starting");
  updateLoadingProgress(steps[stepIdx], (stepIdx + 1) * 100 / numSteps);

  loadPreference();
  SRL::Logger::LogDebug("[INIT] Preferences loaded");
  stepIdx++;
  updateLoadingProgress(steps[stepIdx], (stepIdx + 1) * 100 / numSteps);

  // Initialize random number generator with current time
  uint32_t seed = SDL_GetTicks();
  g_random = new SRL::Math::Random<unsigned int>(seed);
  SRL::Logger::LogDebug("[INIT] Random generator initialized with seed: %u", seed);
  stepIdx++;
  updateLoadingProgress(steps[stepIdx], (stepIdx + 1) * 100 / numSteps);

  initBarragemanager();
  SRL::Logger::LogDebug("[INIT] Barrage manager initialized");
  stepIdx++;
  updateLoadingProgress(steps[stepIdx], (stepIdx + 1) * 100 / numSteps);

  initAttractManager();
  SRL::Logger::LogDebug("[INIT] Attract manager initialized");
  stepIdx++;
  updateLoadingProgress(steps[stepIdx], (stepIdx + 1) * 100 / numSteps);

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

struct StageParams {
  int seed;
  Fxp startLevel;
  Fxp levelInc;
};

static StageParams stagePrm[STAGE_NUM + ENDLESS_STAGE_NUM + 1] = {
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
  initBarrages(stagePrm[stg].seed, stagePrm[stg].startLevel, stagePrm[stg].levelInc);
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
  SRL::Logger::LogDebug("[CDDA] TITLE: forcing menu BGM playMusic(0)");
  playMusic(0);
  initTitleStage(stg);
  showScore();
  clearRPanel();

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

  initBarrages(stagePrm[stg].seed, stagePrm[stg].startLevel, stagePrm[stg].levelInc);
  initGameState(stg);
  SRL::Logger::LogDebug("[GAME] Stage %d: Barrage initialized (enemies=%d, speed=%d, density=%d)",
                        stg,
                        stagePrm[stg].seed,
                        stagePrm[stg].startLevel.As<int>(),
                        stagePrm[stg].levelInc.As<int>());

  if (stg < STAGE_NUM)
  {
    setStageBackground(stg % 5 + 1);
    SRL::Logger::LogDebug("[CDDA] IN_GAME: playMusic(%d) for stage=%d", stg % 5 + 1, stg);
    playMusic(stg % 5 + 1);
    SRL::Logger::LogDebug("[GAME] Stage %d: Background/Music set to %d", stg, stg % 5 + 1);
  }
  else
  {
    if (!insane)
    {
      setStageBackground(0);
      SRL::Logger::LogDebug("[CDDA] IN_GAME endless-normal: playMusic(0)");
      playMusic(0);
    }
    else
    {
      setStageBackground(6);
      SRL::Logger::LogDebug("[CDDA] IN_GAME endless-insane: playMusic(6)");
      playMusic(6);
      SRL::Logger::LogDebug("[GAME] Endless stage: INSANE mode activated");
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

// Structure to hold move-phase subsystem timings (microseconds)
struct MovePhaseTimings {
  uint32_t titleMenu;
  uint32_t gameOver;
  uint32_t stageClear;
  uint32_t pause;
  uint32_t background;
  uint32_t addBullets;
  uint32_t shots;
  uint32_t ship;
  uint32_t foes;
  uint32_t frags;
  uint32_t bonuses;
};

// Global accumulator for move-phase subsystem timings (reset every 60 frames)
static MovePhaseTimings gMovePhaseTimings{};
static uint32_t gMovePhaseFrameCount = 0;

static void move()
{
  uint32_t phaseStart;
  
  switch (status)
  {
  case TITLE:
    phaseStart = SDL_GetProfileMicros();
    moveTitleMenu();
    gMovePhaseTimings.titleMenu += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    addBullets();
    gMovePhaseTimings.addBullets += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFoes();
    gMovePhaseTimings.foes += SDL_GetProfileMicros() - phaseStart;
    break;
    
  case IN_GAME:
    phaseStart = SDL_GetProfileMicros();
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    addBullets();
    gMovePhaseTimings.addBullets += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveShots();
    gMovePhaseTimings.shots += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveShip();
    gMovePhaseTimings.ship += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFoes();
    gMovePhaseTimings.foes += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFrags();
    gMovePhaseTimings.frags += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveBonuses();
    gMovePhaseTimings.bonuses += SDL_GetProfileMicros() - phaseStart;
    break;
    
  case GAMEOVER:
    phaseStart = SDL_GetProfileMicros();
    moveGameover();
    gMovePhaseTimings.gameOver += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    addBullets();
    gMovePhaseTimings.addBullets += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveShots();
    gMovePhaseTimings.shots += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFoes();
    gMovePhaseTimings.foes += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFrags();
    gMovePhaseTimings.frags += SDL_GetProfileMicros() - phaseStart;
    break;
    
  case STAGE_CLEAR:
    phaseStart = SDL_GetProfileMicros();
    moveStageClear();
    gMovePhaseTimings.stageClear += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveShots();
    gMovePhaseTimings.shots += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveShip();
    gMovePhaseTimings.ship += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveFrags();
    gMovePhaseTimings.frags += SDL_GetProfileMicros() - phaseStart;
    
    phaseStart = SDL_GetProfileMicros();
    moveBonuses();
    gMovePhaseTimings.bonuses += SDL_GetProfileMicros() - phaseStart;
    break;
    
  case PAUSE:
    phaseStart = SDL_GetProfileMicros();
    movePause();
    gMovePhaseTimings.pause += SDL_GetProfileMicros() - phaseStart;
    break;
  }
  
  gMovePhaseFrameCount++;
}

static void draw()
{
  memset(l1buf, 0, lyrSize);
  memset(l2buf, 0, lyrSize);

  switch (status)
  {
  case TITLE:
    // Draw background.
    drawBackground();
    drawFoes();
    drawBulletsWake();
    blendScreen();
    // Draw foreground.
    drawBullets();
    drawScore();
    clearRPanel();
    drawTitle();
    drawTitleMenu();
    break;
  case IN_GAME:
    // Draw background.
    drawBackground();
    drawBonuses();
    drawFoes();
#if NOIZ2SA_PERF_MODE
    if ((tick & 1) == 0)
    {
      drawBulletsWake();
    }
    if ((tick & 1) == 0)
    {
      drawFrags();
    }
#else
    drawBulletsWake();
    drawFrags();
#endif
    blendScreen();
    // Draw forground.
    drawShots();
    drawShip();
    drawBullets();
    drawBulletDebugOverlay();
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
    drawBulletDebugOverlay();
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
#if NOIZ2SA_PERF_MODE
    // Keep pause overlay responsive with lower VDP1 workload.
    if ((tick & 1) == 0)
    {
      drawBulletsWake();
    }
#else
    drawBulletsWake();
#endif
    drawFrags();
    blendScreen();
    // Draw forground.
    drawShots();
    drawShip();
    drawBullets();
    drawBulletDebugOverlay();
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
#if NOIZ2SA_ENABLE_SOUND
  noSound = 0;
#else
  noSound = 1;
#endif

  // Input: standard button configuration
  buttonReversed = 0;

  // Display brightness: default value
  brightness = DEFAULT_BRIGHTNESS;

  // Timing: conservative default for stable presentation.
  nowait = 0;
  accframe = 0;
}

int interval = INTERVAL_BASE;
int tick = 0;
static int pPrsd = 1;
static const uint32_t kInGameRenderDivisor = 1;
static const uint32_t kUiRenderDivisor = 1;
static uint32_t gInGameRenderCounter = 0;
static uint32_t gStalledTickFrames = 0;
static bool gUseFixedFramePacing = false;
static uint32_t gFpsTimes100 = 0;
static uint32_t gLastDisplayedFpsTimes100 = 0xFFFFFFFFu;
static uint32_t gFpsWindowStartMs = 0;
static uint32_t gRenderedFramesInWindow = 0;
static bool gFpsWindowInitialized = false;
static uint32_t gSyncCount = 0;
static uint32_t gLastRenderSyncCount = 0;

#if defined(NOIZ2SA_DEBUG_AUTOSTART_SMOKE) && NOIZ2SA_DEBUG_AUTOSTART_SMOKE
static int gAutoStartTitleFrames = 0;
static bool gAutoStartTriggered = false;
#endif

static void drawFpsCounter()
{
  if (gFpsTimes100 == gLastDisplayedFpsTimes100)
    return;
  gLastDisplayedFpsTimes100 = gFpsTimes100;

  const int32_t fpsWhole = (int32_t)(gFpsTimes100 / 100u);
  const int32_t fpsFrac = (int32_t)(gFpsTimes100 % 100u);

  SRL::Debug::PrintClearLine(2);
  SRL::Debug::Print(1, 2, "FPS: %d.%02d", (int)fpsWhole, (int)fpsFrac);
}

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

  // Define loading steps for main()
  const char* mainSteps[] = {
    "Initializing game config",
    "Initializing math utilities",
    "Initializing screen",
    "Initializing sound",
    "Entering title"
  };
  const int mainNumSteps = sizeof(mainSteps) / sizeof(mainSteps[0]);
  int mainStepIdx = 0;

  SRL::Logger::LogDebug("[MAIN] Initializing game config");
  updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
  initGameConfig();
  mainStepIdx++;

  SRL::Logger::LogDebug("[MAIN] Initializing degree utilities");
  updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
  initDegutil();
  mainStepIdx++;

  SRL::Logger::LogDebug("[MAIN] Initializing SDL");
  updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
  initSDL();
  mainStepIdx++;

  if (!noSound)
  {
    SRL::Logger::LogDebug("[MAIN] Initializing sound");
    updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
    initSound();
    loadSounds();
  }
  else
  {
    SRL::Logger::LogInfo("[MAIN] Sound disabled");
    updateLoadingProgress("Sound disabled", (mainStepIdx + 1) * 100 / mainNumSteps);
  }
  mainStepIdx++;

  initFirst();
  updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
  initTitle();
  g_loadingScreen.Clear();

  initGamepad();
  SRL::Logger::LogDebug("[MAIN] Gamepad initialized");

  SRL::Logger::LogInfo("[MAIN] Main game loop starting");

  gFpsTimes100 = 0u;
  gFpsWindowStartMs = SDL_GetTicks();
  gRenderedFramesInWindow = 0;
  gFpsWindowInitialized = true;
  gSyncCount = 0;
  gLastRenderSyncCount = 0;

  //playMusic(7);

  while (!done)
  {
#if defined(NOIZ2SA_DEBUG_AUTOSTART_SMOKE) && NOIZ2SA_DEBUG_AUTOSTART_SMOKE
    if (!gAutoStartTriggered)
    {
      if (status == TITLE)
      {
        gAutoStartTitleFrames++;
        if (gAutoStartTitleFrames >= 120)
        {
          const int autoStage = 0;
          SRL::Logger::LogWarning("[SMOKE] NOIZ2SA_DEBUG_AUTOSTART_SMOKE enabled - auto starting stage %d", autoStage);
          initGame(autoStage);
          gAutoStartTriggered = true;
        }
      }
      else
      {
        gAutoStartTitleFrames = 0;
      }
    }
#endif

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

    // Some emulator paths can report a stalled SDL_GetTicks() value.
    // Fall back to deterministic fixed-step pacing when this is detected.
    if (nowTick <= prvTickCount)
    {
      gStalledTickFrames++;
      if (gStalledTickFrames > 120u)
      {
        gUseFixedFramePacing = true;
      }
    }
    else
    {
      gStalledTickFrames = 0;
    }

    if (gUseFixedFramePacing)
    {
      frame = 1;
      prvTickCount = nowTick;
    }
    else
    {
      frame = (int)(nowTick - prvTickCount) / interval;
    }
    
    if (!gUseFixedFramePacing && frame <= 0)
    {
      frame = 1;

      // In performance mode, avoid busy waiting when timer behavior is unstable.
      if (!nowait)
      {
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
      else
      {
        prvTickCount = nowTick;
      }
    }
    else if (frame > 1)
    {
      // Default behavior favors consistent responsiveness over catch-up bursts.
      // Large catch-up steps can cause a feedback slowdown on slower emulator hosts.
      if (accframe)
      {
        if (frame > 5)
        {
          SRL::Logger::LogWarning("[TIMING] Frame skip detected: %d frames (tick: %ld)", frame, nowTick);
          frame = 5;
        }
        prvTickCount += frame * interval;
      }
      else
      {
        static uint32_t sBacklogDropCount = 0;
        sBacklogDropCount++;
        if ((sBacklogDropCount % 120u) == 1u)
        {
          SRL::Logger::LogDebug("[TIMING] Dropping backlog: frame=%d tick=%ld drops=%lu", frame, nowTick, (unsigned long)sBacklogDropCount);
        }
        frame = 1;
        prvTickCount = nowTick;
      }
    }
    else
    {
      prvTickCount += frame * interval;
    }

    // Process game logic for calculated frames
    uint32_t phaseStartUs = SDL_GetProfileMicros();
    for (i = 0; i < frame; i++)
    {
      move();
      tick++;
    }
    uint32_t timeMoveUs = SDL_GetProfileMicros() - phaseStartUs;

    uint32_t renderDivisor = 1;
    if (status == IN_GAME)
    {
      renderDivisor = kInGameRenderDivisor;
    }
    else if (status == TITLE || status == GAMEOVER || status == STAGE_CLEAR)
    {
      renderDivisor = kUiRenderDivisor;
    }

    const bool renderThisLoop =
        (renderDivisor <= 1u) || ((gInGameRenderCounter++ % renderDivisor) == 0u);

    uint32_t timeSmokeUs = 0;
    uint32_t timeDrawUs = 0;
    uint32_t timeFlipUs = 0;
    if (renderThisLoop)
    {
      phaseStartUs = SDL_GetProfileMicros();
      smokeScreen();
      timeSmokeUs = SDL_GetProfileMicros() - phaseStartUs;

      phaseStartUs = SDL_GetProfileMicros();
      draw();
      timeDrawUs = SDL_GetProfileMicros() - phaseStartUs;

      phaseStartUs = SDL_GetProfileMicros();
      flipScreen();
      timeFlipUs = SDL_GetProfileMicros() - phaseStartUs;

      // Realtime FPS based on rendered frames and elapsed milliseconds.
      // SDL_GetTicks() has stable behavior across emulator/hardware paths.
      {
        const uint32_t nowMs = SDL_GetTicks();

        if (!gFpsWindowInitialized)
        {
          gFpsWindowStartMs = nowMs;
          gRenderedFramesInWindow = 0;
          gFpsWindowInitialized = true;
        }

        gRenderedFramesInWindow++;
        const uint32_t elapsedMs = nowMs - gFpsWindowStartMs;
        const uint32_t syncElapsed = gSyncCount - gLastRenderSyncCount;
        if (syncElapsed > 0u)
        {
          gLastRenderSyncCount = gSyncCount;
        }

        if (elapsedMs >= 500u)
        {
          gFpsTimes100 = (uint32_t)(((unsigned long long)gRenderedFramesInWindow * 100000ull) / elapsedMs);
          gFpsWindowStartMs = nowMs;
          gRenderedFramesInWindow = 0;
        }
        else if (elapsedMs == 0u && gFpsTimes100 == 0u && syncElapsed > 0u)
        {
          // Fallback when SDL ticks are stalled: infer FPS from vblank sync cadence.
          gFpsTimes100 = 6000u / syncElapsed;
        }
      }

      drawFpsCounter();
    }

    phaseStartUs = SDL_GetProfileMicros();
  #if NOIZ2SA_DISABLE_DOUBLE_BUFFER
    // Bypass synchronized frame swap/wait path for max throughput.
    // Keep input polling active so controls still update.
    SRL::Input::Management::RefreshPeripherals();
  #else
    // Prefer synchronized presentation when timer is healthy (normal emulator/hardware path).
    // Fall back to non-blocking refresh only when ticks are stalled to avoid hard lockups.
    if (!gUseFixedFramePacing && nowTick > 0)
    {
      SRL::Core::Synchronize();
      gSyncCount++;
      soundTick(); // Pump M68K driver every frame (replaces unsafe OnVblank callback)
    }
    else
    {
      SRL::Input::Management::RefreshPeripherals();
      // Keep Ponesound command pump alive when synchronized vblank is bypassed.
      soundTick();
    }
  #endif
    uint32_t timeSyncUs = SDL_GetProfileMicros() - phaseStartUs;

    const uint32_t totalUs = timeMoveUs + timeSmokeUs + timeDrawUs + timeFlipUs + timeSyncUs;

    // Log timing every 60 frames (~10 sec at 6 FPS)
    static uint32_t frameCount = 0;
    frameCount++;
    if ((frameCount % 60) == 0)
    {
      // Reset accumulators every 60-frame window.
      if (gMovePhaseFrameCount > 0)
      {
        gMovePhaseTimings = MovePhaseTimings{};
        gMovePhaseFrameCount = 0;
      }
    }

  }
  quitLast();
}
