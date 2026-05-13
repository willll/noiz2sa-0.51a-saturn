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
#include <srl_memory.hpp>
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
#include "system_factory.h"
#include "bulletml_binary/bulletmlrunner.hpp"


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
/** @brief Performs one-time startup initialisation. */
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
  g_random = createRandomGenerator(seed);
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
/** @brief Runs shutdown cleanup and exits the program. */
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

  destroyObject(g_random);
  closeGamepad();
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

/** @brief Initialises title-stage data for the supplied stage index. */
void initTitleStage(int stg)
{
  initFoes();
  initBarrages(stagePrm[stg].seed, stagePrm[stg].startLevel, stagePrm[stg].levelInc);
}

/** @brief Initialises the title scene. */
void initTitle()
{
  SRL::Logger::LogInfo("[STATE] Entering TITLE screen");

  // Start each title/game flow with BulletML fail-safe cleared.
  // This prevents a previous run's alloc latch from suppressing bullets forever.
  resetBulletMlAllocFailureState();

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

/** @brief Initialises a gameplay session for the supplied stage. */
void initGame(int stg)
{
  SRL::Logger::LogInfo("[STATE] Entering IN_GAME (stage %d)", stg);

  // Clear any previous BulletML alloc-failure latch before new gameplay init.
  resetBulletMlAllocFailureState();

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

/** @brief Initialises the game-over scene. */
void initGameover()
{
#if HW_DEBUG
  if (status == IN_GAME)
  {
    SRL::Logger::LogWarning("[HW_DEBUG] Suppressed transition IN_GAME -> GAMEOVER in initGameover()");
    return;
  }
#endif
  SRL::Logger::LogInfo("[STATE] Entering GAMEOVER");

  status = GAMEOVER;
  initGameoverAtr();

  SRL::Logger::LogInfo("[STATE] GAMEOVER screen ready - Game Over!");
}

/** @brief Initialises the stage-clear scene. */
void initStageClear()
{
#if HW_DEBUG
  if (status == IN_GAME)
  {
    SRL::Logger::LogWarning("[HW_DEBUG] Suppressed transition IN_GAME -> STAGE_CLEAR in initStageClear()");
    return;
  }
#endif
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

struct DrawPhaseTimings {
  uint32_t memset;
  uint32_t background;
  uint32_t bonuses;
  uint32_t foes;
  uint32_t bulletsWake;
  uint32_t frags;
  uint32_t blend;
  uint32_t shots;
  uint32_t ship;
  uint32_t bullets;
  uint32_t bulletOverlay;
  uint32_t score;
  uint32_t clearRPanel;
  uint32_t title;
  uint32_t titleMenu;
  uint32_t gameover;
  uint32_t stageClear;
  uint32_t pause;
};

// Global accumulator for move-phase subsystem timings (reset every 60 frames)
static MovePhaseTimings gMovePhaseTimings{};
static uint32_t gMovePhaseFrameCount = 0;
static DrawPhaseTimings gDrawPhaseTimings{};
static uint32_t gDrawPhaseFrameCount = 0;

#if HW_DEBUG || NOIZ2SA_ENABLE_PERF_LOGS
static inline uint32_t profileMicrosNow()
{
  return SDL_GetProfileMicros();
}
#else
// Keep profiling callsites in code for debug readability, but compile them to zero
// cost on default builds where sustained frame rate is critical.
static inline uint32_t profileMicrosNow()
{
  return 0u;
}
#endif

#undef SDL_GetProfileMicros
#define SDL_GetProfileMicros() profileMicrosNow()

static inline void synchronizeFrame()
{
#if defined(NOIZ2SA_ENABLE_GUN_SYNC) && NOIZ2SA_ENABLE_GUN_SYNC
  SRL::Core::Synchronize();
#else
  // Avoid SRL::Input::Gun::Synchronize() per-frame slGetStatus() polling.
  // noiz2sa does not use light-gun input.
  slSynch();
  SRL::Input::Management::RefreshPeripherals();
#endif
}

static inline bool shouldThrottleSpawnsForLoad(int gameStatus, int liveProjectiles, uint32_t hwFree)
{
  if (gameStatus == TITLE)
  {
    // Title attract mode does not need full-density spawning.
    return (liveProjectiles >= 96) || (hwFree > 0u && hwFree < 48000u);
  }

  // In gameplay states, start shedding new spawns before allocator pressure builds.
  return (liveProjectiles >= 220) || (hwFree > 0u && hwFree < 24000u);
}

#if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
static volatile uint8_t gDiagPhaseTag = 0u;
static inline void setDiagPhase(uint8_t tag)
{
  gDiagPhaseTag = tag;
}
#else
static inline void setDiagPhase(uint8_t)
{
}
#endif

static void move()
{
  uint32_t phaseStart;
  const int liveProjectiles = getLiveProjectileCount();
  const uint32_t hwFree = (uint32_t)SRL::Memory::HighWorkRam::GetFreeSpace();
  const bool throttleSpawns = shouldThrottleSpawnsForLoad(status, liveProjectiles, hwFree);
  const int titleSpawnMask = (liveProjectiles >= 220) ? 31 : (throttleSpawns ? 15 : 3);
  const bool allowAddBulletsThisTick = (status == TITLE)
      ? ((tick & titleSpawnMask) == 0)
      : (!throttleSpawns || ((tick & 1) == 0));

  // BulletML allocations can fail transiently under pressure. Keep failure counts
  // for diagnostics, but clear the one-way latch each tick so spawning can recover.
  if (hasBulletMlAllocFailureLatched())
  {
    clearBulletMlAllocFailureLatch();
  }
  
  switch (status)
  {
  case TITLE:
    // Keep title sim responsive under sustained attract-mode runtime.
    setFoeBudgetLimit(NOIZ2SA_FOE_UPDATE_BUDGET / 3);
    if (liveProjectiles >= 240 && (tick & 31) == 0)
    {
      // Attract mode can saturate with lingering projectiles; periodically reset
      // combat objects to recover frame time and keep the title interactive.
      clearFoes();
    }
    if (hwFree > 0u && hwFree < 12000u)
    {
      // Hard recovery path for title attract mode when allocator pressure spikes.
      clearFoes();
    }

    setDiagPhase(11u);
    phaseStart = SDL_GetProfileMicros();
    moveTitleMenu();
    gMovePhaseTimings.titleMenu += SDL_GetProfileMicros() - phaseStart;
    
    setDiagPhase(12u);
    phaseStart = SDL_GetProfileMicros();
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    
    setDiagPhase(13u);
    phaseStart = SDL_GetProfileMicros();
    if (allowAddBulletsThisTick)
    {
      addBullets();
    }
    gMovePhaseTimings.addBullets += SDL_GetProfileMicros() - phaseStart;
    
    setDiagPhase(14u);
    phaseStart = SDL_GetProfileMicros();
    moveFoes();
    gMovePhaseTimings.foes += SDL_GetProfileMicros() - phaseStart;
    break;
    
  case IN_GAME:
#if HW_DEBUG
    {
      static uint32_t sMoveProbeCount = 0u;
      const bool probeMove = (sMoveProbeCount < 4u);
      if (probeMove)
      {
        SRL::Logger::LogInfo("[MOVE] begin idx=%lu", (unsigned long)sMoveProbeCount);
      }
#endif
  setDiagPhase(21u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-bg");
    #endif
    moveBackground();
    gMovePhaseTimings.background += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-bg");
    #endif
    
    setDiagPhase(22u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-addBullets");
    #endif
    if (allowAddBulletsThisTick)
    {
      addBullets();
    }
    gMovePhaseTimings.addBullets += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-addBullets");
    #endif
    
    setDiagPhase(23u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-shots");
    #endif
    moveShots();
    gMovePhaseTimings.shots += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-shots");
    #endif
    
    setDiagPhase(24u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-ship");
    #endif
    moveShip();
    gMovePhaseTimings.ship += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-ship");
    #endif
    
    setDiagPhase(25u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-foes");
    #endif
    moveFoes();
    gMovePhaseTimings.foes += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-foes");
    #endif
    
    setDiagPhase(26u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-frags");
    #endif
    moveFrags();
    gMovePhaseTimings.frags += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-frags");
    #endif
    
    setDiagPhase(27u);
    phaseStart = SDL_GetProfileMicros();
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] pre-bonuses");
    #endif
    moveBonuses();
    gMovePhaseTimings.bonuses += SDL_GetProfileMicros() - phaseStart;
    #if HW_DEBUG
    if (probeMove) SRL::Logger::LogInfo("[MOVE] post-bonuses");
    if (probeMove)
    {
      sMoveProbeCount++;
    }
    }
    #endif
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
  const auto traceDraw = [](uint32_t &accumUs, auto drawCall)
  {
    const uint32_t startUs = SDL_GetProfileMicros();
    drawCall();
    accumUs += SDL_GetProfileMicros() - startUs;
  };

  // Iter E: Collapse two-layer blend architecture.  l2buf was always zero
  // (nothing writes it) so blendScreen() was just copying l1buf→buf.
  // Draw everything directly into buf; saves 2×memset(38400B on LWRAM) ≈ 20ms
  // and eliminates the 21ms blend copy entirely (~31ms total saving).
#if HW_DEBUG
  if (gDrawPhaseFrameCount < 1)
    SRL::Logger::LogInfo("[BUF] buf=%p lyrSize=%u pre-memset hwfree=%u",
                         (void*)buf, (unsigned)lyrSize,
                         (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif
  traceDraw(gDrawPhaseTimings.memset, [&]() { memset(buf, 0, lyrSize); });
#if HW_DEBUG
  if (gDrawPhaseFrameCount < 3) SRL::Logger::LogInfo("[DRAW] post-memset hwfree=%u", (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif

  switch (status)
  {
  case TITLE:
    // Draw background.
    setDiagPhase(41u);
    traceDraw(gDrawPhaseTimings.background, []() { drawBackground(); });
    setDiagPhase(42u);
    traceDraw(gDrawPhaseTimings.foes, []() { drawFoes(); });
    setDiagPhase(43u);
    traceDraw(gDrawPhaseTimings.bulletsWake, []() { drawBulletsWake(); });
    setDiagPhase(44u);
    traceDraw(gDrawPhaseTimings.blend, []() { markPlayfieldDirty(); });
    // Draw foreground.
    setDiagPhase(45u);
    traceDraw(gDrawPhaseTimings.bullets, []() { drawBullets(); });
    setDiagPhase(46u);
    traceDraw(gDrawPhaseTimings.score, []() { drawScore(); });
    setDiagPhase(47u);
    traceDraw(gDrawPhaseTimings.clearRPanel, []() { clearRPanel(); });
    setDiagPhase(48u);
    traceDraw(gDrawPhaseTimings.title, []() { drawTitle(); });
    setDiagPhase(49u);
    traceDraw(gDrawPhaseTimings.titleMenu, []() { drawTitleMenu(); });
    break;
  case IN_GAME:
    // Draw background.
    traceDraw(gDrawPhaseTimings.background, []() { drawBackground(); });
#if HW_DEBUG
    if (gDrawPhaseFrameCount < 3) SRL::Logger::LogInfo("[DRAW] post-bg hwfree=%u", (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif
    traceDraw(gDrawPhaseTimings.bonuses, []() { drawBonuses(); });
    traceDraw(gDrawPhaseTimings.foes, []() { drawFoes(); });
#if HW_DEBUG
    if (gDrawPhaseFrameCount < 3) SRL::Logger::LogInfo("[DRAW] post-foes hwfree=%u", (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif
    // Iter G: Alternate wake/frag draws every other frame to save ~2-4ms rend.
    // Iter M: At high bullet counts (>100), skip wake every 4th frame (75% skip).
    // Iter N: At very high bullet counts (>200), disable wake draws entirely (screen saturated).
    {
      const int liveBullets = getLiveProjectileCount();
      const bool doWake = (liveBullets > 200) ? false :
                          (liveBullets > 100) ? ((tick & 3) == 0) : ((tick & 1) == 0);
      if (doWake)
      {
        traceDraw(gDrawPhaseTimings.bulletsWake, []() { drawBulletsWake(); });
        traceDraw(gDrawPhaseTimings.frags, []() { drawFrags(); });
      }
    }
    traceDraw(gDrawPhaseTimings.blend, []() { markPlayfieldDirty(); });
    // Draw forground.
    traceDraw(gDrawPhaseTimings.shots, []() { drawShots(); });
    traceDraw(gDrawPhaseTimings.ship, []() { drawShip(); });
    traceDraw(gDrawPhaseTimings.bullets, []() { drawBullets(); });
#if HW_DEBUG
    if (gDrawPhaseFrameCount < 3) SRL::Logger::LogInfo("[DRAW] post-bullets hwfree=%u", (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif
    traceDraw(gDrawPhaseTimings.bulletOverlay, []() { drawBulletDebugOverlay(); });
    traceDraw(gDrawPhaseTimings.score, []() { drawScore(); });
#if HW_DEBUG
    if (gDrawPhaseFrameCount < 3) SRL::Logger::LogInfo("[DRAW] post-score hwfree=%u", (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
#endif
    break;
  case GAMEOVER:
    // Draw background.
    traceDraw(gDrawPhaseTimings.background, []() { drawBackground(); });
    traceDraw(gDrawPhaseTimings.foes, []() { drawFoes(); });
    // Iter G: Alternate wake/frag draws every other frame.
    if ((tick & 1) == 0)
    {
      traceDraw(gDrawPhaseTimings.bulletsWake, []() { drawBulletsWake(); });
      traceDraw(gDrawPhaseTimings.frags, []() { drawFrags(); });
    }
    traceDraw(gDrawPhaseTimings.blend, []() { markPlayfieldDirty(); });
    // Draw forground.
    traceDraw(gDrawPhaseTimings.shots, []() { drawShots(); });
    traceDraw(gDrawPhaseTimings.bullets, []() { drawBullets(); });
    traceDraw(gDrawPhaseTimings.bulletOverlay, []() { drawBulletDebugOverlay(); });
    traceDraw(gDrawPhaseTimings.score, []() { drawScore(); });
    traceDraw(gDrawPhaseTimings.gameover, []() { drawGameover(); });
    break;
  case STAGE_CLEAR:
    // Draw background.
    traceDraw(gDrawPhaseTimings.background, []() { drawBackground(); });
    traceDraw(gDrawPhaseTimings.bonuses, []() { drawBonuses(); });
    traceDraw(gDrawPhaseTimings.frags, []() { drawFrags(); });
    traceDraw(gDrawPhaseTimings.blend, []() { markPlayfieldDirty(); });
    // Draw forground.
    traceDraw(gDrawPhaseTimings.shots, []() { drawShots(); });
    traceDraw(gDrawPhaseTimings.ship, []() { drawShip(); });
    traceDraw(gDrawPhaseTimings.score, []() { drawScore(); });
    traceDraw(gDrawPhaseTimings.stageClear, []() { drawStageClear(); });
    break;
  case PAUSE:
    // Draw background.
    traceDraw(gDrawPhaseTimings.background, []() { drawBackground(); });
    traceDraw(gDrawPhaseTimings.bonuses, []() { drawBonuses(); });
    traceDraw(gDrawPhaseTimings.foes, []() { drawFoes(); });
    // Iter G: Alternate wake draw every other frame (same as IN_GAME/GAMEOVER).
    if ((tick & 1) == 0)
    {
      traceDraw(gDrawPhaseTimings.bulletsWake, []() { drawBulletsWake(); });
    }
    traceDraw(gDrawPhaseTimings.frags, []() { drawFrags(); });
    traceDraw(gDrawPhaseTimings.blend, []() { markPlayfieldDirty(); });
    // Draw forground.
    traceDraw(gDrawPhaseTimings.shots, []() { drawShots(); });
    traceDraw(gDrawPhaseTimings.ship, []() { drawShip(); });
    traceDraw(gDrawPhaseTimings.bullets, []() { drawBullets(); });
    traceDraw(gDrawPhaseTimings.bulletOverlay, []() { drawBulletDebugOverlay(); });
    traceDraw(gDrawPhaseTimings.score, []() { drawScore(); });
    traceDraw(gDrawPhaseTimings.pause, []() { drawPause(); });
    break;
  }

  gDrawPhaseFrameCount++;

}

static int accframe = 0;

// Initialize game configuration with Saturn-optimal defaults.
// Saturn doesn't support command-line arguments, so we set optimal settings directly.
static void initGameConfig()
{
#if HW_DEBUG
  noSound = 1;
#elif NOIZ2SA_ENABLE_SOUND
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
static uint32_t gLastSerialFpsLogMs = 0;
static uint32_t gFpsWindowStartMs = 0;
static uint32_t gRenderedFramesInWindow = 0;
static bool gFpsWindowInitialized = false;
static uint32_t gSyncCount = 0;
static uint32_t gLastRenderSyncCount = 0;

struct PerfTraceWindow {
  uint64_t totalUs;
  uint64_t moveUs;
  uint64_t smokeUs;
  uint64_t drawUs;
  uint64_t flipUs;
  uint64_t syncUs;
  uint64_t vdpPanelUploadUs;
  uint64_t vdpPlayfieldUploadUs;
  uint64_t vdpFlipPresentUs;
  uint64_t vdpHwLineUs;
  uint64_t vdpPanelUploadBytes;
  uint64_t vdpPlayfieldUploadBytes;
  uint64_t blendCopyUs;
  uint64_t blendAlphaUs;
  uint64_t vdp1BlitUploadUs;
  uint64_t vdp1BlitDrawUs;
  uint64_t foeTotalCount;
  uint64_t bulletTotalCount;
  uint64_t drawnBulletTotal;
  uint64_t culledBulletTotal;
  uint64_t foeBudgetTotal;
  uint32_t peakFoeCount;
  uint32_t peakBulletCount;
  uint32_t loopCount;
  uint32_t renderCount;
  uint32_t overBudgetCount;
  uint32_t overDoubleBudgetCount;
  uint32_t worstTotalUs;
  uint32_t worstMoveUs;
  uint32_t worstDrawUs;
  uint32_t worstSyncUs;
};

static PerfTraceWindow gPerfTraceWindow{};
static constexpr uint32_t kPerfTraceWindowFrames = 60u;
static constexpr uint32_t kTargetFps = 30u;
static constexpr uint32_t kTargetFrameBudgetUs = 1000000u / kTargetFps;

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

static void logFpsToSerialIfDue()
{
#if HW_DEBUG
  const uint32_t nowMs = SDL_GetTicks();
  if (nowMs - gLastSerialFpsLogMs < 1000u)
    return;

  gLastSerialFpsLogMs = nowMs;
  const int32_t fpsWhole = (int32_t)(gFpsTimes100 / 100u);
  const int32_t fpsFrac = (int32_t)(gFpsTimes100 % 100u);
  SRL::Logger::LogInfo("[FPS] %d.%02d", (int)fpsWhole, (int)fpsFrac);
#endif
}

static uint32_t maxU32(uint32_t a, uint32_t b)
{
  return (a > b) ? a : b;
}

static void logPerfTraceWindowAndReset()
{
#if HW_DEBUG
  if (gPerfTraceWindow.loopCount == 0u)
    return;

  const uint32_t loops = gPerfTraceWindow.loopCount;
  const uint32_t avgTotalUs = (uint32_t)(gPerfTraceWindow.totalUs / loops);
  const uint32_t avgMoveUs = (uint32_t)(gPerfTraceWindow.moveUs / loops);
  const uint32_t avgSmokeUs = (uint32_t)(gPerfTraceWindow.smokeUs / loops);
  const uint32_t avgDrawUs = (uint32_t)(gPerfTraceWindow.drawUs / loops);
  const uint32_t avgFlipUs = (uint32_t)(gPerfTraceWindow.flipUs / loops);
  const uint32_t avgSyncUs = (uint32_t)(gPerfTraceWindow.syncUs / loops);
  const uint32_t avgRenderUs = avgSmokeUs + avgDrawUs + avgFlipUs;
  const uint32_t avgVdpPanelUploadUs = (uint32_t)(gPerfTraceWindow.vdpPanelUploadUs / loops);
  const uint32_t avgVdpPlayfieldUploadUs = (uint32_t)(gPerfTraceWindow.vdpPlayfieldUploadUs / loops);
  const uint32_t avgVdpFlipPresentUs = (uint32_t)(gPerfTraceWindow.vdpFlipPresentUs / loops);
  const uint32_t avgVdpHwLineUs = (uint32_t)(gPerfTraceWindow.vdpHwLineUs / loops);
  const uint32_t avgVdpPanelUploadBytes = (uint32_t)(gPerfTraceWindow.vdpPanelUploadBytes / loops);
  const uint32_t avgVdpPlayfieldUploadBytes = (uint32_t)(gPerfTraceWindow.vdpPlayfieldUploadBytes / loops);
  const uint32_t avgBlendCopyUs = (uint32_t)(gPerfTraceWindow.blendCopyUs / loops);
  const uint32_t avgBlendAlphaUs = (uint32_t)(gPerfTraceWindow.blendAlphaUs / loops);
  const uint32_t avgFoeCount = (uint32_t)(gPerfTraceWindow.foeTotalCount / loops);
  const uint32_t avgBulletCount = (uint32_t)(gPerfTraceWindow.bulletTotalCount / loops);
  const uint32_t avgDrawnBullets = (uint32_t)(gPerfTraceWindow.drawnBulletTotal / loops);
  const uint32_t avgCulledBullets = (uint32_t)(gPerfTraceWindow.culledBulletTotal / loops);
  const uint32_t avgFoeBudget = (uint32_t)(gPerfTraceWindow.foeBudgetTotal / loops);
  const uint32_t cullPct = (avgDrawnBullets + avgCulledBullets > 0u) ?
      (avgCulledBullets * 100u) / (avgDrawnBullets + avgCulledBullets) : 0u;
  const uint32_t budgetUsePct = (avgTotalUs * 100u) / kTargetFrameBudgetUs;
  const uint32_t overBudgetPct = (gPerfTraceWindow.overBudgetCount * 100u) / loops;
  const uint32_t overDoubleBudgetPct = (gPerfTraceWindow.overDoubleBudgetCount * 100u) / loops;
  const uint32_t headroomUs = (avgTotalUs >= kTargetFrameBudgetUs) ?
      (avgTotalUs - kTargetFrameBudgetUs) : (kTargetFrameBudgetUs - avgTotalUs);

  // Approximate move-phase hotspot by total cost over the profiling window.
  uint32_t hotspotUs = gMovePhaseTimings.background;
  const char* hotspotName = "moveBackground";
  if (gMovePhaseTimings.addBullets > hotspotUs) { hotspotUs = gMovePhaseTimings.addBullets; hotspotName = "addBullets"; }
  if (gMovePhaseTimings.shots > hotspotUs) { hotspotUs = gMovePhaseTimings.shots; hotspotName = "moveShots"; }
  if (gMovePhaseTimings.ship > hotspotUs) { hotspotUs = gMovePhaseTimings.ship; hotspotName = "moveShip"; }
  if (gMovePhaseTimings.foes > hotspotUs) { hotspotUs = gMovePhaseTimings.foes; hotspotName = "moveFoes"; }
  if (gMovePhaseTimings.frags > hotspotUs) { hotspotUs = gMovePhaseTimings.frags; hotspotName = "moveFrags"; }
  if (gMovePhaseTimings.bonuses > hotspotUs) { hotspotUs = gMovePhaseTimings.bonuses; hotspotName = "moveBonuses"; }
  if (gMovePhaseTimings.titleMenu > hotspotUs) { hotspotUs = gMovePhaseTimings.titleMenu; hotspotName = "moveTitleMenu"; }
  if (gMovePhaseTimings.gameOver > hotspotUs) { hotspotUs = gMovePhaseTimings.gameOver; hotspotName = "moveGameover"; }
  if (gMovePhaseTimings.stageClear > hotspotUs) { hotspotUs = gMovePhaseTimings.stageClear; hotspotName = "moveStageClear"; }
  if (gMovePhaseTimings.pause > hotspotUs) { hotspotUs = gMovePhaseTimings.pause; hotspotName = "movePause"; }

  const uint32_t moveFrames = maxU32(gMovePhaseFrameCount, 1u);
  const uint32_t avgHotspotUs = hotspotUs / moveFrames;

  // Approximate draw-phase hotspot by total cost over the profiling window.
  uint32_t drawHotspotUs = gDrawPhaseTimings.memset;
  const char *drawHotspotName = "clearBuf";
  if (gDrawPhaseTimings.background > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.background; drawHotspotName = "drawBackground"; }
  if (gDrawPhaseTimings.bonuses > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bonuses; drawHotspotName = "drawBonuses"; }
  if (gDrawPhaseTimings.foes > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.foes; drawHotspotName = "drawFoes"; }
  if (gDrawPhaseTimings.bulletsWake > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bulletsWake; drawHotspotName = "drawBulletsWake"; }
  if (gDrawPhaseTimings.frags > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.frags; drawHotspotName = "drawFrags"; }
  if (gDrawPhaseTimings.blend > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.blend; drawHotspotName = "blendScreen"; }
  if (gDrawPhaseTimings.shots > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.shots; drawHotspotName = "drawShots"; }
  if (gDrawPhaseTimings.ship > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.ship; drawHotspotName = "drawShip"; }
  if (gDrawPhaseTimings.bullets > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bullets; drawHotspotName = "drawBullets"; }
  if (gDrawPhaseTimings.bulletOverlay > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bulletOverlay; drawHotspotName = "drawBulletOverlay"; }
  if (gDrawPhaseTimings.score > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.score; drawHotspotName = "drawScore"; }
  if (gDrawPhaseTimings.clearRPanel > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.clearRPanel; drawHotspotName = "clearRPanel"; }
  if (gDrawPhaseTimings.title > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.title; drawHotspotName = "drawTitle"; }
  if (gDrawPhaseTimings.titleMenu > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.titleMenu; drawHotspotName = "drawTitleMenu"; }
  if (gDrawPhaseTimings.gameover > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.gameover; drawHotspotName = "drawGameover"; }
  if (gDrawPhaseTimings.stageClear > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.stageClear; drawHotspotName = "drawStageClear"; }
  if (gDrawPhaseTimings.pause > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.pause; drawHotspotName = "drawPause"; }

  const uint32_t drawFrames = maxU32(gDrawPhaseFrameCount, 1u);
  const uint32_t avgDrawHotspotUs = drawHotspotUs / drawFrames;

  uint32_t bottleneckUs = avgMoveUs;
  const char* bottleneckName = "move";
  if (avgRenderUs > bottleneckUs)
  {
    bottleneckUs = avgRenderUs;
    bottleneckName = "render";
  }
  if (avgSyncUs > bottleneckUs)
  {
    bottleneckUs = avgSyncUs;
    bottleneckName = "sync";
  }

    const uint32_t avgSmk = (uint32_t)(gPerfTraceWindow.smokeUs / loops);
    const uint32_t avgDrawUs2 = (uint32_t)(gPerfTraceWindow.drawUs / loops);
    const uint32_t avgFlipUs2 = (uint32_t)(gPerfTraceWindow.flipUs / loops);
    SRL::Logger::LogInfo(
      "[PERF] fps=%u.%02u target=%u avg(total=%u move=%u rend=%u smoke=%u sync=%u draw=%u flip=%u)",
      (unsigned int)(gFpsTimes100 / 100u),
      (unsigned int)(gFpsTimes100 % 100u),
      (unsigned int)kTargetFps,
      (unsigned int)avgTotalUs,
      (unsigned int)avgMoveUs,
      (unsigned int)avgRenderUs,
      (unsigned int)avgSmk,
      (unsigned int)avgSyncUs,
      (unsigned int)avgDrawUs2,
      (unsigned int)avgFlipUs2);

    SRL::Logger::LogInfo(
      "[PERF] budget=%uus use=%u%% over=%u%% over2x=%u%%",
      (unsigned int)kTargetFrameBudgetUs,
      (unsigned int)budgetUsePct,
      (unsigned int)overBudgetPct,
      (unsigned int)overDoubleBudgetPct);

  SRL::Logger::LogInfo(
      "[PERF] worst(total=%u move=%u draw=%u sync=%u) rendered=%u/%u",
      (unsigned int)gPerfTraceWindow.worstTotalUs,
      (unsigned int)gPerfTraceWindow.worstMoveUs,
      (unsigned int)gPerfTraceWindow.worstDrawUs,
      (unsigned int)gPerfTraceWindow.worstSyncUs,
      (unsigned int)gPerfTraceWindow.renderCount,
      (unsigned int)loops);

  SRL::Logger::LogInfo(
      "[PERF] move_hotspot=%s avg_us=%u (window_frames=%u sim_frames=%u)",
      hotspotName,
      (unsigned int)avgHotspotUs,
      (unsigned int)loops,
      (unsigned int)gMovePhaseFrameCount);

    SRL::Logger::LogInfo(
      "[PERF_DRAW] hotspot=%s avg_us=%u clr=%u bg=%u bon=%u foes=%u frags=%u wake=%u blend=%u shots=%u ship=%u bullets=%u score=%u title=%u tmenu=%u",
      drawHotspotName,
      (unsigned int)avgDrawHotspotUs,
      (unsigned int)(gDrawPhaseTimings.memset / drawFrames),
      (unsigned int)(gDrawPhaseTimings.background / drawFrames),
      (unsigned int)(gDrawPhaseTimings.bonuses / drawFrames),
      (unsigned int)(gDrawPhaseTimings.foes / drawFrames),
      (unsigned int)(gDrawPhaseTimings.frags / drawFrames),
      (unsigned int)(gDrawPhaseTimings.bulletsWake / drawFrames),
      (unsigned int)(gDrawPhaseTimings.blend / drawFrames),
      (unsigned int)(gDrawPhaseTimings.shots / drawFrames),
      (unsigned int)(gDrawPhaseTimings.ship / drawFrames),
      (unsigned int)(gDrawPhaseTimings.bullets / drawFrames),
      (unsigned int)(gDrawPhaseTimings.score / drawFrames),
      (unsigned int)(gDrawPhaseTimings.title / drawFrames),
      (unsigned int)(gDrawPhaseTimings.titleMenu / drawFrames));

    SRL::Logger::LogInfo(
      "[PERF_VDP] up_pf=%uus(%uB) up_pan=%uus(%uB) flip=%u hwline=%u",
      (unsigned int)avgVdpPlayfieldUploadUs,
      (unsigned int)avgVdpPlayfieldUploadBytes,
      (unsigned int)avgVdpPanelUploadUs,
      (unsigned int)avgVdpPanelUploadBytes,
      (unsigned int)avgVdpFlipPresentUs,
      (unsigned int)avgVdpHwLineUs);

    SRL::Logger::LogInfo(
      "[PERF_DRAW2] blend_copy=%uus blend_alpha=%uus skip=%d vdp1_upload=%uus vdp1_draw=%uus",
      (unsigned int)avgBlendCopyUs,
      (unsigned int)avgBlendAlphaUs,
      (int)NOIZ2SA_BLEND_SKIP,
      (unsigned int)(gPerfTraceWindow.vdp1BlitUploadUs / loops),
      (unsigned int)(gPerfTraceWindow.vdp1BlitDrawUs / loops));

    SRL::Logger::LogInfo(
      "[PERF_FOE] avg_entities=%u peak=%u avg_bullets=%u peak=%u budget=%u",
      (unsigned int)avgFoeCount,
      (unsigned int)gPerfTraceWindow.peakFoeCount,
      (unsigned int)avgBulletCount,
      (unsigned int)gPerfTraceWindow.peakBulletCount,
      (unsigned int)avgFoeBudget);

    SRL::Logger::LogInfo(
      "[PERF_CULL] drawn=%u culled=%u cull_pct=%u%%",
      (unsigned int)avgDrawnBullets,
      (unsigned int)avgCulledBullets,
      (unsigned int)cullPct);

  if (avgTotalUs > kTargetFrameBudgetUs)
  {
    SRL::Logger::LogInfo(
        "[PERF_HINT] bottleneck=%s avg_us=%u need_reduction_us=%u for %uFPS",
        bottleneckName,
        (unsigned int)bottleneckUs,
        (unsigned int)headroomUs,
        (unsigned int)kTargetFps);
  }
  else
  {
    SRL::Logger::LogInfo(
        "[PERF_HINT] target met: spare_budget_us=%u vs %uFPS",
        (unsigned int)headroomUs,
        (unsigned int)kTargetFps);
  }
#endif

  gPerfTraceWindow = PerfTraceWindow{};
  gMovePhaseTimings = MovePhaseTimings{};
  gMovePhaseFrameCount = 0;
  gDrawPhaseTimings = DrawPhaseTimings{};
  gDrawPhaseFrameCount = 0;
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
#if HW_DEBUG
  updateLoadingProgress("Entering HW_DEBUG endless", (mainStepIdx + 1) * 100 / mainNumSteps);
  insane = 1;
  const int hwDebugStage = (HW_DEBUG_ENDLESS_STAGE < STAGE_NUM) ? STAGE_NUM : HW_DEBUG_ENDLESS_STAGE;
  if (hwDebugStage != HW_DEBUG_ENDLESS_STAGE)
  {
    SRL::Logger::LogWarning("[HW_DEBUG] Requested stage %d is not endless; forcing stage %d", HW_DEBUG_ENDLESS_STAGE, hwDebugStage);
  }
  SRL::Logger::LogInfo("[HW_DEBUG] Skipping title/menu and booting directly into endless INSANE stage %d", hwDebugStage);
  SRL::Logger::LogInfo("[HW_DEBUG] Entering initGame() for stage %d", hwDebugStage);
  initGame(hwDebugStage);
  SRL::Logger::LogInfo("[HW_DEBUG] initGame() returned for stage %d", hwDebugStage);
#else
  updateLoadingProgress(mainSteps[mainStepIdx], (mainStepIdx + 1) * 100 / mainNumSteps);
  initTitle();
#endif
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

  while (!done)
  {
#if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
    static uint32_t sPhaseTraceCounter = 0u;
#endif
#if HW_DEBUG
    static uint32_t sPhaseProbeLoops = 0u;
    const bool probePhase = (sPhaseProbeLoops < 8u);
    if (probePhase)
    {
      SRL::Logger::LogInfo("[PHASE] loop-start loop=%lu status=%d", (unsigned long)sPhaseProbeLoops, status);
    }
#endif
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
  #if HW_DEBUG
    // Perf-campaign debug probe: bypass START polling to isolate loop-1 stall source.
  #else
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
#if HW_DEBUG
    if (probePhase)
    {
      SRL::Logger::LogInfo("[PHASE] post-input loop=%lu", (unsigned long)sPhaseProbeLoops);
    }
#endif
    else
    {
      pPrsd = 0;
    }
#endif

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
#if HW_DEBUG
    if (probePhase)
    {
      SRL::Logger::LogInfo("[PHASE] post-framecalc loop=%lu now=%ld prev=%ld frame=%d", (unsigned long)sPhaseProbeLoops, nowTick, prvTickCount, frame);
    }
#endif
    
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

    // Adaptive foe update budget controller.
    // Tightens the per-tick foe update cap when the previous frame was over-budget,
    // relaxes it when we have headroom. The round-robin cursor in moveFoes() ensures
    // every foe is visited across consecutive frames even when the budget clips one tick.
#if HW_DEBUG
    {
      static uint32_t sLastFrameTotalUs = 0u;
      static int sAdaptiveFoeBudget = 0; // 0 = uncapped
      // Track per-frame instantaneous timing (delta from previous loop iteration),
      // not the windowed average. This gives 1-frame reaction instead of 60-frame lag.
      static uint64_t sPrevWindowTotalUs = 0u;
      static uint32_t sPrevWindowLoopCount = 0u;
      const int kFoeBudgetMin  = NOIZ2SA_FOE_UPDATE_BUDGET / 2;
      const int kBudgetTightenStep = 8;
      const int kBudgetRelaxStep   = 4;

      if (status == IN_GAME || status == GAMEOVER || status == PAUSE)
      {
        if (sLastFrameTotalUs > kTargetFrameBudgetUs)
        {
          if (sAdaptiveFoeBudget == 0)
            sAdaptiveFoeBudget = kFoeBudgetMin; // jump directly to floor on first overrun
          else
            sAdaptiveFoeBudget -= kBudgetTightenStep;
          if (sAdaptiveFoeBudget < kFoeBudgetMin)
            sAdaptiveFoeBudget = kFoeBudgetMin;
        }
        else if (sAdaptiveFoeBudget > 0)
        {
          sAdaptiveFoeBudget += kBudgetRelaxStep;
          if (sAdaptiveFoeBudget >= NOIZ2SA_FOE_UPDATE_BUDGET)
            sAdaptiveFoeBudget = 0; // back to uncapped
        }
        setFoeBudgetLimit(sAdaptiveFoeBudget);
      }
      else
      {
        setFoeBudgetLimit(0);
        sAdaptiveFoeBudget = 0;
      }
      // Compute instantaneous per-frame time (delta between consecutive readings).
      // gPerfTraceWindow is updated at end of loop body, so this gives the
      // PREVIOUS frame's actual total — reacts in 1 frame instead of ~60.
      {
        uint32_t curCount = gPerfTraceWindow.loopCount;
        uint64_t curTotal = gPerfTraceWindow.totalUs;
        uint32_t deltaCount = curCount - sPrevWindowLoopCount;
        if (deltaCount > 0u)
        {
          sLastFrameTotalUs = (uint32_t)((curTotal - sPrevWindowTotalUs) / deltaCount);
        }
        sPrevWindowTotalUs   = curTotal;
        sPrevWindowLoopCount = curCount;
      }
    }
#endif

    // Process game logic for calculated frames
    uint32_t phaseStartUs = SDL_GetProfileMicros();
  #if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
    setDiagPhase(1u); // move/update phase
  #endif
#if HW_DEBUG
    if (probePhase)
    {
      SRL::Logger::LogInfo("[PHASE] pre-move loop=%lu frame=%d", (unsigned long)sPhaseProbeLoops, frame);
    }
#endif
    for (i = 0; i < frame; i++)
    {
#if HW_DEBUG
      if (probePhase && i == 0)
      {
        SRL::Logger::LogInfo("[PHASE] pre-move-call loop=%lu i=%d", (unsigned long)sPhaseProbeLoops, i);
      }
#endif
      move();
#if HW_DEBUG
      if (probePhase && i == 0)
      {
        SRL::Logger::LogInfo("[PHASE] post-move-call loop=%lu i=%d", (unsigned long)sPhaseProbeLoops, i);
      }
#endif
      tick++;
    }
    uint32_t timeMoveUs = SDL_GetProfileMicros() - phaseStartUs;
#if HW_DEBUG
    if (probePhase)
    {
      SRL::Logger::LogInfo("[PHASE] post-move loop=%lu us=%lu frame=%d", (unsigned long)sPhaseProbeLoops, (unsigned long)timeMoveUs, frame);
    }
#endif

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
    ScreenVdpPerfStats frameVdpStats{};
    if (renderThisLoop)
    {
    #if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
      setDiagPhase(2u); // render phase
    #endif
#if HW_DEBUG
      if (probePhase)
      {
        SRL::Logger::LogInfo("[PHASE] pre-draw loop=%lu hwfree=%u", (unsigned long)sPhaseProbeLoops, (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
      }
#endif
      phaseStartUs = SDL_GetProfileMicros();
      smokeScreen();
      timeSmokeUs = SDL_GetProfileMicros() - phaseStartUs;

      phaseStartUs = SDL_GetProfileMicros();
      draw();
      timeDrawUs = SDL_GetProfileMicros() - phaseStartUs;
#if HW_DEBUG
      if (probePhase)
      {
        SRL::Logger::LogInfo("[PHASE] post-draw loop=%lu us=%lu hwfree=%u", (unsigned long)sPhaseProbeLoops, (unsigned long)timeDrawUs, (unsigned)SRL::Memory::HighWorkRam::GetFreeSpace());
      }
#endif

      phaseStartUs = SDL_GetProfileMicros();
      flipScreen();
      timeFlipUs = SDL_GetProfileMicros() - phaseStartUs;
#if HW_DEBUG
      if (probePhase)
      {
        SRL::Logger::LogInfo("[PHASE] post-flip loop=%lu us=%lu", (unsigned long)sPhaseProbeLoops, (unsigned long)timeFlipUs);
      }
#endif

      consumeScreenVdpPerfStats(&frameVdpStats);

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
      logFpsToSerialIfDue();
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
    #if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
      setDiagPhase(3u); // pre-sync wait
    #if NOIZ2SA_LOG_TO_DEV_CART && !HW_DEBUG
      if ((sPhaseTraceCounter % 120u) == 0u)
      {
        SRL::Logger::LogInfo("[PHASE] pre-sync tick=%lu status=%d phase=%u",
                 (unsigned long)tick,
                 status,
                 (unsigned)gDiagPhaseTag);
      }
    #endif
    #endif
#if HW_DEBUG
      if (probePhase)
      {
        SRL::Logger::LogInfo("[PHASE] pre-sync loop=%lu", (unsigned long)sPhaseProbeLoops);
      }
#endif
      synchronizeFrame();
      gSyncCount++;
    #if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
      setDiagPhase(4u); // post-sync
    #if NOIZ2SA_LOG_TO_DEV_CART && !HW_DEBUG
      if ((sPhaseTraceCounter % 120u) == 0u)
      {
        SRL::Logger::LogInfo("[PHASE] post-sync tick=%lu status=%d phase=%u",
                 (unsigned long)tick,
                 status,
                 (unsigned)gDiagPhaseTag);
      }
    #endif
    #endif
#if HW_DEBUG
      if (probePhase)
      {
        SRL::Logger::LogInfo("[PHASE] post-sync loop=%lu", (unsigned long)sPhaseProbeLoops);
      }
#endif
      soundTick(); // Pump M68K driver every frame
    }
    else
    {
#if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
  setDiagPhase(5u); // fixed-frame input refresh path
#endif
      SRL::Input::Management::RefreshPeripherals();
      // Keep Ponesound command pump alive when synchronized vblank is bypassed.
      soundTick();
    }
  #endif
    uint32_t timeSyncUs = SDL_GetProfileMicros() - phaseStartUs;

#if HW_DEBUG || NOIZ2SA_LOG_TO_DEV_CART
    {
      static uint32_t sLastHeartbeatMs = 0;
      const uint32_t hbNowMs = SDL_GetTicks();
#if NOIZ2SA_LOG_TO_DEV_CART && !HW_DEBUG
  const uint32_t heartbeatIntervalMs = 250u;
#else
  const uint32_t heartbeatIntervalMs = 1000u;
#endif
  if (sLastHeartbeatMs == 0 || (hbNowMs - sLastHeartbeatMs) >= heartbeatIntervalMs)
      {
        sLastHeartbeatMs = hbNowMs;
        const uint32_t loops = maxU32(gPerfTraceWindow.loopCount, 1u);
        const uint32_t moveFrames = maxU32(gMovePhaseFrameCount, 1u);
        const uint32_t drawFrames = maxU32(gDrawPhaseFrameCount, 1u);
        const uint32_t avgTotalUs = (uint32_t)(gPerfTraceWindow.totalUs / loops);
        const uint32_t avgMoveUs = (uint32_t)(gPerfTraceWindow.moveUs / loops);
        const uint32_t avgDrawUs = (uint32_t)((gPerfTraceWindow.drawUs + gPerfTraceWindow.smokeUs + gPerfTraceWindow.flipUs) / loops);
        const uint32_t avgSyncUs = (uint32_t)(gPerfTraceWindow.syncUs / loops);
        const uint32_t avgBulletCount = (uint32_t)(gPerfTraceWindow.bulletTotalCount / loops);
        const uint32_t avgFoeCount = (uint32_t)(gPerfTraceWindow.foeTotalCount / loops);
        const uint32_t avgFoeBudget = (uint32_t)(gPerfTraceWindow.foeBudgetTotal / loops);
        uint32_t moveHotspotUs = gMovePhaseTimings.background;
        const char* moveHotspotName = "moveBackground";
        if (gMovePhaseTimings.addBullets > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.addBullets; moveHotspotName = "addBullets"; }
        if (gMovePhaseTimings.shots > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.shots; moveHotspotName = "moveShots"; }
        if (gMovePhaseTimings.ship > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.ship; moveHotspotName = "moveShip"; }
        if (gMovePhaseTimings.foes > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.foes; moveHotspotName = "moveFoes"; }
        if (gMovePhaseTimings.frags > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.frags; moveHotspotName = "moveFrags"; }
        if (gMovePhaseTimings.bonuses > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.bonuses; moveHotspotName = "moveBonuses"; }
        if (gMovePhaseTimings.titleMenu > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.titleMenu; moveHotspotName = "moveTitleMenu"; }
        if (gMovePhaseTimings.gameOver > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.gameOver; moveHotspotName = "moveGameover"; }
        if (gMovePhaseTimings.stageClear > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.stageClear; moveHotspotName = "moveStageClear"; }
        if (gMovePhaseTimings.pause > moveHotspotUs) { moveHotspotUs = gMovePhaseTimings.pause; moveHotspotName = "movePause"; }
        uint32_t drawHotspotUs = gDrawPhaseTimings.memset;
        const char* drawHotspotName = "clearBuf";
        if (gDrawPhaseTimings.background > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.background; drawHotspotName = "drawBackground"; }
        if (gDrawPhaseTimings.bonuses > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bonuses; drawHotspotName = "drawBonuses"; }
        if (gDrawPhaseTimings.foes > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.foes; drawHotspotName = "drawFoes"; }
        if (gDrawPhaseTimings.bulletsWake > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bulletsWake; drawHotspotName = "drawBulletsWake"; }
        if (gDrawPhaseTimings.frags > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.frags; drawHotspotName = "drawFrags"; }
        if (gDrawPhaseTimings.blend > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.blend; drawHotspotName = "blendScreen"; }
        if (gDrawPhaseTimings.shots > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.shots; drawHotspotName = "drawShots"; }
        if (gDrawPhaseTimings.ship > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.ship; drawHotspotName = "drawShip"; }
        if (gDrawPhaseTimings.bullets > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bullets; drawHotspotName = "drawBullets"; }
        if (gDrawPhaseTimings.bulletOverlay > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.bulletOverlay; drawHotspotName = "drawBulletOverlay"; }
        if (gDrawPhaseTimings.score > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.score; drawHotspotName = "drawScore"; }
        if (gDrawPhaseTimings.clearRPanel > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.clearRPanel; drawHotspotName = "clearRPanel"; }
        if (gDrawPhaseTimings.title > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.title; drawHotspotName = "drawTitle"; }
        if (gDrawPhaseTimings.titleMenu > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.titleMenu; drawHotspotName = "drawTitleMenu"; }
        if (gDrawPhaseTimings.gameover > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.gameover; drawHotspotName = "drawGameover"; }
        if (gDrawPhaseTimings.stageClear > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.stageClear; drawHotspotName = "drawStageClear"; }
        if (gDrawPhaseTimings.pause > drawHotspotUs) { drawHotspotUs = gDrawPhaseTimings.pause; drawHotspotName = "drawPause"; }
#if HW_DEBUG
        SRL::Logger::LogInfo(
          "[HEARTBEAT] ms=%lu tick=%lu status=%d fps=%u.%02u loops=%lu phase=%u trace=%lu sync=%lu bml=%u/%u hot=%s:%u/%s:%u perf(total=%u move=%u draw=%u sync=%u) worst(total=%u move=%u draw=%u sync=%u) entities(avg=%u peak=%u) bullets(avg=%u peak=%u live=%d)",
          (unsigned long)hbNowMs,
          (unsigned long)tick,
          status,
          (unsigned)(gFpsTimes100 / 100u),
          (unsigned)(gFpsTimes100 % 100u),
          (unsigned long)gPerfTraceWindow.loopCount,
          (unsigned)gDiagPhaseTag,
          (unsigned long)sPhaseTraceCounter,
          (unsigned long)gSyncCount,
          hasBulletMlAllocFailureLatched() ? 1u : 0u,
          (unsigned)getBulletMlAllocFailureCount(),
          moveHotspotName,
          (unsigned)(moveHotspotUs / moveFrames),
          drawHotspotName,
          (unsigned)(drawHotspotUs / drawFrames),
          (unsigned)avgTotalUs,
          (unsigned)avgMoveUs,
          (unsigned)avgDrawUs,
          (unsigned)avgSyncUs,
          (unsigned)gPerfTraceWindow.worstTotalUs,
          (unsigned)gPerfTraceWindow.worstMoveUs,
          (unsigned)gPerfTraceWindow.worstDrawUs,
          (unsigned)gPerfTraceWindow.worstSyncUs,
          (unsigned)avgFoeCount,
          (unsigned)gPerfTraceWindow.peakFoeCount,
          (unsigned)avgBulletCount,
          (unsigned)gPerfTraceWindow.peakBulletCount,
          getLiveProjectileCount());
#else
        SRL::Logger::LogInfo("[HEARTBEAT] ms=%lu tick=%lu status=%d fps=%u.%02u loops=%lu",
                             (unsigned long)hbNowMs,
                             (unsigned long)tick,
                             status,
                             (unsigned)(gFpsTimes100 / 100u),
                             (unsigned)(gFpsTimes100 % 100u),
                             (unsigned long)gPerfTraceWindow.loopCount);
        SRL::Logger::LogInfo("[HEARTBEAT-PHASE] phase=%u trace=%lu sync=%lu",
                             (unsigned)gDiagPhaseTag,
                             (unsigned long)sPhaseTraceCounter,
                             (unsigned long)gSyncCount);
        SRL::Logger::LogInfo("[HEARTBEAT-BML] latch=%u fails=%u",
                 hasBulletMlAllocFailureLatched() ? 1u : 0u,
                 (unsigned)getBulletMlAllocFailureCount());
        SRL::Logger::LogInfo("[HEARTBEAT-HOT] move=%s:%u draw=%s:%u budget=%u",
           moveHotspotName,
           (unsigned)(moveHotspotUs / moveFrames),
           drawHotspotName,
           (unsigned)(drawHotspotUs / drawFrames),
           (unsigned)avgFoeBudget);
        SRL::Logger::LogInfo("[HEARTBEAT-PERF] avg_us(total=%u move=%u draw=%u sync=%u)",
                 (unsigned)avgTotalUs,
                 (unsigned)avgMoveUs,
                 (unsigned)avgDrawUs,
                 (unsigned)avgSyncUs);
        SRL::Logger::LogInfo("[HEARTBEAT-PERF] worst_us(total=%u move=%u draw=%u sync=%u)",
                 (unsigned)gPerfTraceWindow.worstTotalUs,
                 (unsigned)gPerfTraceWindow.worstMoveUs,
                 (unsigned)gPerfTraceWindow.worstDrawUs,
                 (unsigned)gPerfTraceWindow.worstSyncUs);
        SRL::Logger::LogInfo("[HEARTBEAT-PERF] entities(avg=%u peak=%u) bullets(avg=%u peak=%u live=%d)",
                 (unsigned)avgFoeCount,
                 (unsigned)gPerfTraceWindow.peakFoeCount,
                 (unsigned)avgBulletCount,
                 (unsigned)gPerfTraceWindow.peakBulletCount,
                 getLiveProjectileCount());
#endif
      }
      sPhaseTraceCounter++;
    }
#endif

    const uint32_t totalUs = timeMoveUs + timeSmokeUs + timeDrawUs + timeFlipUs + timeSyncUs;

    gPerfTraceWindow.totalUs += totalUs;
    gPerfTraceWindow.moveUs += timeMoveUs;
    gPerfTraceWindow.smokeUs += timeSmokeUs;
    gPerfTraceWindow.drawUs += timeDrawUs;
    gPerfTraceWindow.flipUs += timeFlipUs;
    gPerfTraceWindow.syncUs += timeSyncUs;
    gPerfTraceWindow.vdpPanelUploadUs += frameVdpStats.panelUploadUs;
    gPerfTraceWindow.vdpPlayfieldUploadUs += frameVdpStats.playfieldUploadUs;
    gPerfTraceWindow.vdpFlipPresentUs += frameVdpStats.flipPresentUs;
    gPerfTraceWindow.vdpHwLineUs += frameVdpStats.hwLineUs;
    gPerfTraceWindow.vdpPanelUploadBytes += frameVdpStats.panelUploadBytes;
    gPerfTraceWindow.vdpPlayfieldUploadBytes += frameVdpStats.playfieldUploadBytes;
    gPerfTraceWindow.blendCopyUs += frameVdpStats.blendCopyUs;
    gPerfTraceWindow.blendAlphaUs += frameVdpStats.blendAlphaUs;
    gPerfTraceWindow.vdp1BlitUploadUs += frameVdpStats.vdp1BlitUploadUs;
    gPerfTraceWindow.vdp1BlitDrawUs += frameVdpStats.vdp1BlitDrawUs;

    FoePressureStats frameFoeStats{};
    consumeFoePressureStats(&frameFoeStats);
    gPerfTraceWindow.foeTotalCount += frameFoeStats.foeCount;
    gPerfTraceWindow.bulletTotalCount += frameFoeStats.bulletCount;
    gPerfTraceWindow.drawnBulletTotal += frameFoeStats.drawnBullets;
    gPerfTraceWindow.culledBulletTotal += frameFoeStats.culledBullets;
    gPerfTraceWindow.foeBudgetTotal += frameFoeStats.foeBudget;
    gPerfTraceWindow.peakFoeCount = maxU32(gPerfTraceWindow.peakFoeCount, frameFoeStats.foeCount);
    gPerfTraceWindow.peakBulletCount = maxU32(gPerfTraceWindow.peakBulletCount, frameFoeStats.bulletCount);
    gPerfTraceWindow.loopCount++;
    if (renderThisLoop)
    {
      gPerfTraceWindow.renderCount++;
    }
    if (totalUs > kTargetFrameBudgetUs)
    {
      gPerfTraceWindow.overBudgetCount++;
    }
    if (totalUs > (kTargetFrameBudgetUs * 2u))
    {
      gPerfTraceWindow.overDoubleBudgetCount++;
    }
    gPerfTraceWindow.worstTotalUs = maxU32(gPerfTraceWindow.worstTotalUs, totalUs);
    gPerfTraceWindow.worstMoveUs = maxU32(gPerfTraceWindow.worstMoveUs, timeMoveUs);
    gPerfTraceWindow.worstDrawUs = maxU32(gPerfTraceWindow.worstDrawUs, timeDrawUs + timeSmokeUs + timeFlipUs);
    gPerfTraceWindow.worstSyncUs = maxU32(gPerfTraceWindow.worstSyncUs, timeSyncUs);

    // Windowed profiling logs for iterative optimization on Saturn HW.
    static uint32_t frameCount = 0;
    frameCount++;
    if ((frameCount % kPerfTraceWindowFrames) == 0)
    {
      logPerfTraceWindowAndReset();
    }

#if HW_DEBUG
    if (probePhase)
    {
      sPhaseProbeLoops++;
    }
#endif

  }
  quitLast();
}
