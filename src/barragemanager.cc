/*
 * $Id: barragemanager.cc,v 1.4 2003/02/09 07:34:15 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Handle stage data.
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include "noiz2sa.h"
#include "degutil.h"
#include "vector.h"
#include "screen.h"
#include "rand.h"
#include "brgmng_mtd.h"
#include "soundmanager.h"
#include "attractmanager.h"
#include "bulletml_factory.h"

#include "barragemanager.h"
#include "foe.h"
#include <srl_log.hpp>
#include <srl_system.hpp>
#include <srl_cd.hpp>
#include <srl_memory.hpp>
#include <srl_string.hpp>
#include <ctype.h>

#if HW_DEBUG
#include "hw_debug_bulletml_embedded.hpp"
#endif

#define BARRAGE_PATTERN_MAX 32

static Barrage barragePattern[BARRAGE_TYPE_NUM][BARRAGE_PATTERN_MAX];
static Barrage *barrageQueue[BARRAGE_TYPE_NUM][BARRAGE_PATTERN_MAX];
static int barragePatternNum[BARRAGE_TYPE_NUM];

static Barrage *barrage[BARRAGE_MAX];

static const char *BARRAGE_DIR_NAME[] = {
  "ZAKO", "MIDDLE", "BOSS"
};

static const char *const kZakoBlbNames[] = {
  "248SHOT.BLB",
  "3WAYCH.BLB",
  "6GT.BLB",
  "ACCEL.BLB",
  "ACCUM.BLB",
  "BAR.BLB",
  "BEE.BLB",
  "BITMOVE.BLB",
  "IKAR5VR.BLB",
  "KETR1BI.BLB",
  "NWAY.BLB",
  "RNDWAY.BLB",
  "SLOWDWN.BLB",
  "SPREAD.BLB",
  "STAY.BLB",
  "THRUST.BLB",
  "TWIN.BLB",
  "TWITEXT.BLB",
};

static const char *const kMiddleBlbNames[] = {
  "1TONWAY.BLB",
  "22WAY.BLB",
  "23ACCEL.BLB",
  "2ROUND.BLB",
  "4WACCEL.BLB",
  "4WAY.BLB",
  "ACCEL16.BLB",
  "DAIRBS2.BLB",
  "DAIRBS3.BLB",
  "GROW.BLB",
  "GROWRND.BLB",
  "IKAR3MD.BLB",
  "IKARFL.BLB",
  "KETR25W.BLB",
  "KETR4LR.BLB",
  "MNWAY.BLB",
  "NWROLL.BLB",
  "SHOTGUN.BLB",
  "SPRE2BL.BLB",
  "SPREABF.BLB",
  "SPREADN.BLB",
  "VRTLAS.BLB",
  "XFIRE.BLB",
};

static const char *const kBossBlbNames[] = {
  "57WAY.BLB",
  "88WAY.BLB",
  "BEEWND.BLB",
  "BIT.BLB",
  "DAIHIB2B.BLB",
  "DAIHIB2R.BLB",
  "DAIRBS1.BLB",
  "DAIRBS4.BLB",
  "DAIRBS5.BLB",
  "DAIRBSL.BLB",
  "DBLROL.BLB",
  "FWD3WAY.BLB",
  "GUWB2CF.BLB",
  "GUWB3F3.BLB",
  "GUWB4EB.BLB",
  "IKADRC2.BLB",
  "IKAR1MD.BLB",
  "KETR2RK.BLB",
  "KETR3FS.BLB",
  "KETR4RB.BLB",
  "KETR5AC.BLB",
  "KETR5MS.BLB",
  "PROGR1G.BLB",
  "PROGR2S.BLB",
  "PROGR3B.BLB",
  "PROGR3W.BLB",
  "PROGR5R.BLB",
  "PROGR5W.BLB",
  "PSYXABO.BLB",
  "ROLL3PS.BLB",
  "ROLLBAR.BLB",
  "SIDCRK.BLB",
};

/** @brief Loads BulletML files for a barrage category from disc or embedded data. */
static int readBulletMLFiles(const char *dirPath, Barrage brg[]) {
#if HW_DEBUG
  int i = 0;
  const HWDebugBulletML::EmbeddedPattern* patterns = nullptr;
  uint32_t patternCount = 0;

  SRL::Logger::LogInfo("[INIT-BARRAGE] begin type=%s hwfree=%lu",
                       dirPath,
                       (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());

  if (strcmp(dirPath, "ZAKO") == 0)
  {
    patterns = HWDebugBulletML::kZakoPatterns;
    patternCount = HWDebugBulletML::kZakoPatternCount;
  }
  else if (strcmp(dirPath, "MIDDLE") == 0)
  {
    patterns = HWDebugBulletML::kMiddlePatterns;
    patternCount = HWDebugBulletML::kMiddlePatternCount;
  }
  else if (strcmp(dirPath, "BOSS") == 0)
  {
    patterns = HWDebugBulletML::kBossPatterns;
    patternCount = HWDebugBulletML::kBossPatternCount;
  }

  if (patterns == nullptr || patternCount == 0)
  {
    SRL::Logger::LogFatal("[HW_DEBUG] No embedded BulletML patterns available for %s", dirPath);
    SRL::System::Exit(1);
  }

  for (uint32_t patternIndex = 0; patternIndex < patternCount && i < BARRAGE_PATTERN_MAX; ++patternIndex)
  {
    const bool isBoss = (strcmp(dirPath, "BOSS") == 0);
    const bool detailedTrace = isBoss && (patternIndex + 1u >= 14u);

    SRL::Logger::LogInfo("[INIT-BARRAGE] parse-begin type=%s idx=%lu/%lu hwfree=%lu",
                         dirPath,
                         (unsigned long)(patternIndex + 1u),
                         (unsigned long)patternCount,
                         (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());

    if (detailedTrace)
    {
      SRL::Logger::LogInfo("[INIT-BARRAGE] parser-create-begin type=%s idx=%lu name=%s size=%lu",
                           dirPath,
                           (unsigned long)(patternIndex + 1u),
                           patterns[patternIndex].name,
                           (unsigned long)patterns[patternIndex].size);
    }
    brg[i].bulletml = createEmbeddedBulletMlParser(patterns[patternIndex].name, patterns[patternIndex].data, patterns[patternIndex].size);
    if (detailedTrace)
    {
      SRL::Logger::LogInfo("[INIT-BARRAGE] parser-create-end type=%s idx=%lu ptr=%p hwfree=%lu",
                           dirPath,
                           (unsigned long)(patternIndex + 1u),
                           brg[i].bulletml,
                           (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());
    }

    if (detailedTrace)
    {
      SRL::Logger::LogInfo("[INIT-BARRAGE] build-begin type=%s idx=%lu name=%s",
                           dirPath,
                           (unsigned long)(patternIndex + 1u),
                           patterns[patternIndex].name);
    }
    if (!brg[i].bulletml->build())
    {
      SRL::Logger::LogFatal("[HW_DEBUG] Failed to parse embedded BulletML file: %s/%s", dirPath, patterns[patternIndex].name);
      destroyBulletMlParser(brg[i].bulletml);
      SRL::System::Exit(1);
    }
    if (detailedTrace)
    {
      SRL::Logger::LogInfo("[INIT-BARRAGE] build-end type=%s idx=%lu name=%s hwfree=%lu",
                           dirPath,
                           (unsigned long)(patternIndex + 1u),
                           patterns[patternIndex].name,
                           (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());
    }

    SRL::Logger::LogInfo("[INIT-BARRAGE] parse-end type=%s idx=%lu/%lu name=%s hwfree=%lu",
                         dirPath,
                         (unsigned long)(patternIndex + 1u),
                         (unsigned long)patternCount,
                         patterns[patternIndex].name,
                         (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());
    i++;
  }

  SRL::Logger::LogInfo("[INIT-BARRAGE] done type=%s loaded=%d hwfree=%lu",
                       dirPath,
                       i,
                       (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());

  return i;
#else
  int i = 0;
  int parseFailures = 0;
  const char *const *knownNames = nullptr;
  int knownNameCount = 0;

  SRL::Logger::LogDebug("[BARRAGE] Reading BulletML files from directory: %s", dirPath);

  if (strcmp(dirPath, "ZAKO") == 0) {
    knownNames = kZakoBlbNames;
    knownNameCount = (int)(sizeof(kZakoBlbNames) / sizeof(kZakoBlbNames[0]));
  } else if (strcmp(dirPath, "MIDDLE") == 0) {
    knownNames = kMiddleBlbNames;
    knownNameCount = (int)(sizeof(kMiddleBlbNames) / sizeof(kMiddleBlbNames[0]));
  } else if (strcmp(dirPath, "BOSS") == 0) {
    knownNames = kBossBlbNames;
    knownNameCount = (int)(sizeof(kBossBlbNames) / sizeof(kBossBlbNames[0]));
  }

  if (knownNames == nullptr || knownNameCount <= 0) {
    SRL::Logger::LogFatal("[BARRAGE] Unknown barrage directory: %s", dirPath);
    SRL::System::Exit(1);
  }

  int phaseBase = 35;
  int phaseSpan = 20;
  if (strcmp(dirPath, "MIDDLE") == 0) {
    phaseBase = 55;
  } else if (strcmp(dirPath, "BOSS") == 0) {
    phaseBase = 75;
  }

  SRL::Logger::LogInfo("[BARRAGE] ChangeDir root begin: %s", dirPath);
  SRL::Cd::ChangeDir((char *)nullptr);
  int32_t code = SRL::Cd::ChangeDir(dirPath);
  SRL::Logger::LogInfo("[BARRAGE] ChangeDir target end: %s code=%d", dirPath, code);
  if (code < 0) {
    SRL::Logger::LogFatal("[BARRAGE] Can't open directory: %s (error code: %d)", dirPath, code);
    SRL::System::Exit(1);
  }
  SRL::Logger::LogInfo("[BARRAGE] Directory ready: %s", dirPath);

  for (int entryIndex = 0; entryIndex < knownNameCount && i < BARRAGE_PATTERN_MAX; ++entryIndex) {
    const char *resolvedName = knownNames[entryIndex];
    char step[64];
    snprintf(step, sizeof(step), "Loading %s %d/%d", dirPath, entryIndex + 1, knownNameCount);
    updateLoadingProgress(step, phaseBase + ((entryIndex + 1) * phaseSpan) / knownNameCount);

    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Loading late entry: %s/%s (%d/%d)", dirPath, resolvedName, entryIndex + 1, knownNameCount);
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry begin: %s/%s", dirPath, resolvedName);
    }

    SRL::Cd::File patternFile(resolvedName);
    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Late entry exists check begin: %s/%s", dirPath, resolvedName);
    }
    if (!patternFile.Exists()) {
      SRL::Logger::LogWarning("[BARRAGE] Missing pattern on disc: %s/%s", dirPath, resolvedName);
      continue;
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry exists ok: %s/%s", dirPath, resolvedName);
    }
    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Late entry exists check end: %s/%s", dirPath, resolvedName);
    }

    int32_t patternSize = (int32_t)patternFile.Size.Bytes;
    if (patternSize <= 0) {
      SRL::Logger::LogWarning("[BARRAGE] Empty pattern on disc: %s/%s", dirPath, resolvedName);
      continue;
    }

    const int32_t sectorCount = (int32_t)patternFile.Size.Sectors;
    const int32_t sectorBytes = (int32_t)patternFile.Size.SectorSize * sectorCount;

    uint8_t *patternData = lwnew uint8_t[sectorBytes];
    if (patternData == nullptr) {
      SRL::Logger::LogFatal("[BARRAGE] Failed to allocate %d bytes for %s/%s", sectorBytes, dirPath, resolvedName);
      SRL::System::Exit(1);
    }

    if (!patternFile.Open()) {
      SRL::Logger::LogWarning("[BARRAGE] Failed to open pattern file: %s/%s", dirPath, resolvedName);
      delete[] patternData;
      continue;
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry open ok: %s/%s", dirPath, resolvedName);
    }
    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Late entry open end: %s/%s", dirPath, resolvedName);
    }

    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Late entry read begin: %s/%s sectors=%d", dirPath, resolvedName, sectorCount);
    }
    int32_t bytesRead = patternFile.ReadSectors(sectorCount, patternData);
    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Late entry read end: %s/%s bytes=%d", dirPath, resolvedName, bytesRead);
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry read ok: %s/%s bytes=%d", dirPath, resolvedName, bytesRead);
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry close begin: %s/%s", dirPath, resolvedName);
    }
    patternFile.Close();
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry close end: %s/%s", dirPath, resolvedName);
    }
    if (bytesRead <= 0) {
      SRL::Logger::LogWarning("[BARRAGE] Failed to read pattern bytes: %s/%s", dirPath, resolvedName);
      delete[] patternData;
      continue;
    }

    SRL::Logger::LogDebug("[BARRAGE] Loading BulletML file: %s/%s", dirPath, resolvedName);
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry parser create begin: %s/%s", dirPath, resolvedName);
    }
    brg[i].bulletml = createEmbeddedBulletMlParser(resolvedName, patternData, (std::size_t)bytesRead);
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry parser create end: %s/%s", dirPath, resolvedName);
      SRL::Logger::LogInfo("[BARRAGE] First entry build begin: %s/%s", dirPath, resolvedName);
    }
    if (!brg[i].bulletml->build()) {
      parseFailures++;
      SRL::Logger::LogFatal("[BARRAGE] Failed to parse BulletML file: %s/%s", dirPath, resolvedName);
      destroyBulletMlParser(brg[i].bulletml);
      delete[] patternData;
      continue;
    }
    if (entryIndex == 0) {
      SRL::Logger::LogInfo("[BARRAGE] First entry build ok: %s/%s", dirPath, resolvedName);
    }
    if (entryIndex >= 13) {
      SRL::Logger::LogInfo("[BARRAGE] Built late entry: %s/%s (%d/%d)", dirPath, resolvedName, entryIndex + 1, knownNameCount);
    }
    delete[] patternData;
    i++;
  }

  if (i <= 0) {
    SRL::Logger::LogFatal("[BARRAGE] No valid BLB patterns loaded from %s; aborting startup", dirPath);
    SRL::System::Exit(1);
  }
  if (parseFailures > 0) {
    SRL::Logger::LogWarning("[BARRAGE] %s loaded with %d parse failures (%d valid patterns)", dirPath, parseFailures, i);
  }
  
  return i;
#endif
}

static unsigned int rnd;

/** @brief Initialises the barrage manager and loads all barrage sets. */
void initBarragemanager() {
  SRL::Logger::LogDebug("[BARRAGE] Initializing barrage manager");
  
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    SRL::Logger::LogInfo("[INIT-BARRAGE] type-begin idx=%d name=%s hwfree=%lu",
                         i,
                         BARRAGE_DIR_NAME[i],
                         (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());
    SRL::Logger::LogDebug("[BARRAGE] Loading barrage type %d: %s", i, BARRAGE_DIR_NAME[i]);
    barragePatternNum[i] = readBulletMLFiles(BARRAGE_DIR_NAME[i], barragePattern[i]);
    SRL::Logger::LogInfo("[INIT-BARRAGE] type-end idx=%d name=%s loaded=%d hwfree=%lu",
                         i,
                         BARRAGE_DIR_NAME[i],
                         barragePatternNum[i],
                         (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace());
    SRL::Logger::LogInfo("[BARRAGE] Type %d: Loaded %d patterns", i, barragePatternNum[i]);
    SRL::Logger::LogInfo("--------");
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      barragePattern[i][j].type = i;
    }
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Barrage manager initialization complete");
}

/** @brief Releases barrage manager resources. */
void closeBarragemanager() {
  SRL::Logger::LogDebug("[BARRAGE] Closing barrage manager");
  
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      destroyBulletMlParser(barragePattern[i][j].bulletml);
    }
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Barrage manager closed");
}

int scene;
int endless, insane;
static int sceneCnt;
static Fxp level, levelInc;

/** @brief Initialises the active barrage sequence for a stage. */
void initBarrages(int seed, Fxp startLevel, Fxp li) {
  int n1, n2, rn;

  SRL::Logger::LogDebug("[BARRAGE] Initializing barrage sequences: seed=%d, startLevel=%.2f, levelInc=%.2f", seed, startLevel, li);

  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      barrageQueue[i][j] = &(barragePattern[i][j]);
    }
  }

  processSpeedDownBulletsNum = DEFAULT_SPEED_DOWN_BULLETS_NUM;
  if ( seed >= 0 ) {
    rnd = seed;
    endless = 0;
    insane = 0;
    SRL::Logger::LogDebug("[BARRAGE] Seeded mode: seed=%d (endless=false, insane=false)", seed);
  } else {
    rnd = (unsigned int)SDL_GetTicks();
    endless = 1;
    if ( seed == -2 ) insane = 1;
    else insane = 0;
    if ( seed == -3 ) processSpeedDownBulletsNum = EASY_SPEED_DOWN_BULLETS_NUM;
    else if ( seed == -4 ) processSpeedDownBulletsNum = HARD_SPEED_DOWN_BULLETS_NUM;
    SRL::Logger::LogDebug("[BARRAGE] Random mode: insane=%d, speedDownBullets=%d", insane, processSpeedDownBulletsNum);
  }
  // Shuffle.
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    int bn = barragePatternNum[i];
    rn = 60+nextRandInt(&rnd)%4;
    for ( int j=0 ; j<rn ; j++ ) {
      n1 = nextRandInt(&rnd)%bn; n2 = nextRandInt(&rnd)%bn;
      Barrage* tb = barrageQueue[i][n1];
      barrageQueue[i][n1] = barrageQueue[i][n2];
      barrageQueue[i][n2] = tb;
    }
    for ( int j=0 ; j<bn ; j++ ) {
      barrageQueue[i][j]->maxRank = Fxp::Convert((int)(nextRandInt(&rnd) % 70)) / 100 + 0.3f;
      barrageQueue[i][j]->frq = 1;
    }
  }

  scene = -1;
  sceneCnt = 0;
  level = startLevel;
  levelInc = li;
  
  SRL::Logger::LogDebug("[BARRAGE] Barrage sequences initialized: scene=%d, sceneCnt=%d, level=%.2f", scene, sceneCnt, level);
}

/**
 * Roll the barrage queue after the new barrage pattern is set.
 */
/** @brief Shuffles the active barrage pattern queue. */
static void rollBarragePattern(Barrage *br[], int brNum) {
  Barrage *tbr;
  int n = (Fxp::Convert(brNum) / (Fxp::Convert((int)(nextRandInt(&rnd) % 32)) / 32 + 1) + 0.5f).As<int>();
  if ( n == 0 ) return;
  if ( n > brNum ) n = brNum;
  tbr = br[0];
  for ( int i=0 ; i<n-1 ; i++ ) {
    br[i] = br[i+1];
  }
  br[n-1] = tbr;
  br[0]->maxRank *= 2;
  while ( br[0]->maxRank > 1 ) br[0]->maxRank -= 0.7f;
}

static int barrageNum;
static int bossMode;

static int pax;
static int pay;
static int quickAppType;

/**
 * Make the barrage pattern of this scene.
 */
void setBarrages(Fxp level, int bm, int midMode) {
  int bpn = 0, bn, i;
  int barrageMax, addFrqLoop = 0;

  barrageNum = 0;
  barrageMax = nextRandInt(&rnd)%3+4;
  bossMode = bm;
  if ( !midMode ) {
    bpn = 0;
  } else {
    bpn = 1;
  }
  quickAppType = bpn;
  for ( bn = 0 ;  ; bn++ ) {
    if ( bn == 0 && level < 0 ) break;
    if ( bossMode ) {
      if ( bn == 0 ) bpn = 0;
      else bpn = 2;
      if ( bn >= BARRAGE_MAX ) break;
    } else {
      if ( bn >= barrageMax ) {
	bn = 0;
	addFrqLoop = 1;
      }
    }
    if ( addFrqLoop ) {
      barrage[bn]->frq++;
      level -= 1+barrage[bn]->rank;
      if ( level < 0 ) break;
    } else {
      barrageNum++;
      rollBarragePattern(barrageQueue[bpn], barragePatternNum[bpn]);
      barrage[bn] = barrageQueue[bpn][0];
      barrage[bn]->frq = 1;
      if ( level < barrageQueue[bpn][0]->maxRank ) {
	if ( level < 0 ) level = 0;
	barrage[bn]->rank = level;
	if ( !bossMode || bn > 0 ) break;
      }
      barrage[bn]->rank = barrageQueue[bpn][0]->maxRank;
      if ( !bossMode ) {
	level -= 1+barrageQueue[bpn][0]->maxRank;
      } else {
	if ( bn > 0 ) level -= 4+(barrageQueue[bpn][0]->maxRank*6);
      }

      bpn++;
      if ( bpn >= BARRAGE_TYPE_NUM ) {
	if ( !midMode ) {
	  bpn = 0;
	} else {
	  bpn = 1;
	}
      }
    }
  }

  pax = (nextRandInt(&rnd)%(SCAN_WIDTH_8*2/3) + (SCAN_WIDTH_8/6));
  pay = (nextRandInt(&rnd)%(SCAN_HEIGHT_8/6) + (SCAN_HEIGHT_8/10));

  scene++;
}

#define SCENE_TERM 1000
#define SCENE_END_TERM 100
#define ZAKO_APP_TERM 1500
static int zakoAppCnt;

static int appFreq[] = {90, 360, 800};
static int shield[] = {3, 6, 9};

static Foe *bossBullet;

/**
 * Add enemies.
 */
void addBullets() {
  int x, y, i;
  int type, frq;
  int spawnBudget = NOIZ2SA_SPAWN_BUDGET_PER_TICK;

  // Scene time control.
  sceneCnt--;
  if ( sceneCnt < 0 ) {
    if ( !insane ) clearFoes();
    if ( scene >= 0 && !endless ) setClearScore();
    if ( scene%10 == 8 ) {
      sceneCnt = 999999;
      // In HW_DEBUG, suppress all zako spawning during boss fights so entity
      // pressure stays bounded regardless of how high the level has grown.
      // Only the boss entity fires bullets, keeping total entities manageable.
#if HW_DEBUG
      zakoAppCnt = 0;
#else
      zakoAppCnt = ZAKO_APP_TERM;
#endif
      setBarrages(level, true, false);
      addBossBullet();
    } else {
      sceneCnt = SCENE_TERM;
      if ( scene%10 == 3 ) {
	setBarrages(level, false, true);
      } else {
	setBarrages(level, false, false);
      }
    }
    level += levelInc;
    if ( status == IN_GAME || status == TITLE ) {
      drawRPanel();
    } else {
      sceneCnt = 999999;
    }
  }

  if ( sceneCnt < SCENE_END_TERM ) return;

  for ( i=0 ; i<barrageNum ; i++ ) {
    if (spawnBudget <= 0)
    {
      break;
    }

    if ( bossMode ) {
      if ( i > 0 ) break;
      if ( zakoAppCnt <= 0 ) break;
      zakoAppCnt--;
#if HW_DEBUG
      // Suppress new zako spawning when entity pressure is already high.
      // Prevents runaway accumulation that collapses FPS on real hardware.
      if ( getActiveFoeCount() >= 150 ) continue;
#endif
    }
    type = barrage[i]->type;
    // An additional enemy appears when there is no enemy of the same type.
    if ( type == quickAppType && enNum[type] == 0 ) {
      x = pax; y = pay;
      if (addFoe(x, y, barrage[i]->rank, 512, 0, type, shield[type], barrage[i]->bulletml) != nullptr)
      {
        spawnBudget--;
      }
    }

    frq = appFreq[type]/barrage[i]->frq;
    if ( frq < 2 ) frq = 2;
    if ( spawnBudget > 0 && (nextRandInt(&rnd)%frq) == 0 ) {
      x = nextRandInt(&rnd)%(SCAN_WIDTH_8*2/3) + (SCAN_WIDTH_8/6);
      y = nextRandInt(&rnd)%(SCAN_HEIGHT_8/6) + (SCAN_HEIGHT_8/10);
      if ( type == quickAppType ) {
	pax = x; pay = y;
      }
      if (addFoe(x, y, barrage[i]->rank, 512, 0, type, shield[type], barrage[i]->bulletml) != nullptr)
      {
        spawnBudget--;
      }
    }
  }
}

void bossDestroyed() {
  if ( !endless ) {
    setClearScore();
    addLeftBonus();
    initStageClear();
  }
  clearFoes();
  sceneCnt = 180;
  zakoAppCnt = 0;
}

#define BOSS_SHIELD 128

void addBossBullet() {
  Foe *bl;
  bossBullet = nullptr;
#if HW_DEBUG
  // At high level (cycle 4+), setBarrages() produces 5-6 boss-active bullets
  // each running expensive 9-task BOSS BulletML.  That fills kMaxTotalProjectiles
  // within 60 frames, driving moveFoes to ~424ms/frame on the SH-2.  Limit to
  // one boss-active bullet in HW_DEBUG to keep the entity cascade manageable.
  static constexpr int kHwDebugMaxBossActive = 1;
  int hwDebugBossActiveCount = 0;
#endif
  for ( int i=0 ; i<barrageNum ; i++ ) {
    if ( barrage[i]->type != 2 ) continue;
    if ( bossBullet == nullptr ) {
      bl = addFoe(SCAN_WIDTH_8/2, SCAN_HEIGHT_8/5, barrage[i]->rank, 512, 0,
		  BOSS_TYPE, BOSS_SHIELD, barrage[i]->bulletml);
      bossBullet = bl;
    } else {
#if HW_DEBUG
      if (hwDebugBossActiveCount >= kHwDebugMaxBossActive) continue;
      ++hwDebugBossActiveCount;
#endif
      bl = addFoeBossActiveBullet(SCAN_WIDTH_8/2, SCAN_HEIGHT_8/5, barrage[i]->rank, 512, 0,
				  barrage[i]->bulletml);
    }
  }
}
