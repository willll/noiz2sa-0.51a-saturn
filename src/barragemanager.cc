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

#include "barragemanager.h"
#include "foe.h"
#include <srl_log.hpp>
#include <srl_system.hpp>
#include <srl_cd.hpp>
#include <srl_string.hpp>

#define BARRAGE_PATTERN_MAX 32

static Barrage barragePattern[BARRAGE_TYPE_NUM][BARRAGE_PATTERN_MAX];
static Barrage *barrageQueue[BARRAGE_TYPE_NUM][BARRAGE_PATTERN_MAX];
static int barragePatternNum[BARRAGE_TYPE_NUM];

static Barrage *barrage[BARRAGE_MAX];

static const char *BARRAGE_DIR_NAME[] = {
  "ZAKO", "MIDDLE", "BOSS"
};

static int readBulletMLFiles(const char *dirPath, Barrage brg[]) {
  int i = 0;
  int listEntries = 0;
  int parseFailures = 0;
  char fileName[256];
  const char * listPath = "LIST.TXT";
  char line[32];

  // On-screen debug: show BLB loading start
  char onscreenMsg[64];
  snprintf(onscreenMsg, sizeof(onscreenMsg), "BLB LOAD: %s...", dirPath);
  SRL::Debug::Print(1, 8, onscreenMsg);
  SRL::Core::Synchronize();

  SRL::Logger::LogDebug("[BARRAGE] Reading BulletML files from directory: %s", dirPath);

  // Change to the specified directory on CD
  SRL::Cd::ChangeDir((char *)nullptr);
  int32_t code = SRL::Cd::ChangeDir(dirPath);
  
  if (code < 0) {
    SRL::Logger::LogFatal("[BARRAGE] Can't open directory: %s (error code: %d)", dirPath, code);
    SRL::System::Exit(1);
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Opening list file: %s", listPath);
  SRL::Cd::File listFile(listPath);

  if (!listFile.Exists()) {
    SRL::Logger::LogFatal("[BARRAGE] LIST file does not exist: %s", listPath);
    SRL::System::Exit(1);
  }
  
  SRL::Logger::LogDebug("[BARRAGE] LIST file size: %d bytes", listFile.Size.Bytes);

  // Read the LIST file line by line
  int32_t bytesRead = 0;
  uint8_t buffer[2048];
  int32_t bytesToRead = (int32_t)listFile.Size.Bytes;
  if (bytesToRead > (int32_t)sizeof(buffer)) {
    bytesToRead = (int32_t)sizeof(buffer);
  }
  int32_t bufferSize = listFile.LoadBytes(0, bytesToRead, buffer);
  
  if (bufferSize <= 0) {
    SRL::Logger::LogFatal("[BARRAGE] Failed to read LIST file or file is empty (bytes read: %d)", bufferSize);
    SRL::System::Exit(1);
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Read %d bytes from LIST file", bufferSize);
  SRL::Logger::LogInfo("[BLB-TRACE] Begin directory scan: %s", dirPath);
  
  // Stay in the current directory to load files directly by name
  SRL::Logger::LogTrace("[BARRAGE] Loading files from directory: %s", dirPath);
  
  while (bytesRead < bufferSize && i < BARRAGE_PATTERN_MAX) {
    int lineLen = 0;
    // Extract one line (ensure we stay within buffer bounds)
    while (bytesRead < bufferSize && buffer[bytesRead] != '\n' && buffer[bytesRead] != '\r' && lineLen < 31) {
      if (buffer[bytesRead] < 0x20 || buffer[bytesRead] > 0x7E) {
        SRL::Logger::LogTrace("[BARRAGE] Encountered non-printable character 0x%02X at offset %d, stopping line extraction", buffer[bytesRead], bytesRead);
        bytesRead++;
        break;
      }
      line[lineLen++] = buffer[bytesRead++];
    }
    line[lineLen] = '\0';
    while (bytesRead < bufferSize && (buffer[bytesRead] == '\n' || buffer[bytesRead] == '\r')) {
      bytesRead++;
    }
    while (lineLen > 0 && (line[lineLen-1] == ' ' || line[lineLen-1] == '\t')) {
      lineLen--;
      line[lineLen] = '\0';
    }
    if (lineLen == 0) {
      continue;
    }
    bool hasContent = false;
    for (int j = 0; j < lineLen; j++) {
      if ((line[j] >= 'A' && line[j] <= 'Z') || (line[j] >= 'a' && line[j] <= 'z') || (line[j] >= '0' && line[j] <= '9')) {
        hasContent = true;
        break;
      }
    }
    if (!hasContent) {
      SRL::Logger::LogTrace("[BARRAGE] Skipping invalid line from LIST (no alphanumeric characters)");
      continue;
    }
    listEntries++;

    // On-screen trace: show which BLB is being loaded
    char onscreenTrace[64];
    snprintf(onscreenTrace, sizeof(onscreenTrace), "BLB %s %d: %s", dirPath, listEntries, line);
    SRL::Debug::Print(1, 10, onscreenTrace);
    SRL::Core::Synchronize();

    int phaseBase = 35;
    int phaseSpan = 60;
    if (strcmp(dirPath, "ZAKO") == 0) {
      phaseBase = 35;
      phaseSpan = 20;
    } else if (strcmp(dirPath, "MIDDLE") == 0) {
      phaseBase = 55;
      phaseSpan = 20;
    } else if (strcmp(dirPath, "BOSS") == 0) {
      phaseBase = 75;
      phaseSpan = 20;
    }
    char step[64];
    snprintf(step, sizeof(step), "Loading %s %d/%d", dirPath, listEntries, BARRAGE_PATTERN_MAX);
    updateLoadingProgress(step, phaseBase + (listEntries * phaseSpan) / BARRAGE_PATTERN_MAX);

    // Log and show start of BLB load
    SRL::Logger::LogInfo("[BLB-TRACE] [%s] #%d build-start: %s", dirPath, listEntries, line);
    SRL::Logger::LogDebug("[BARRAGE] Loading BulletML file: %s/%s", dirPath, line);

    brg[i].bulletml = new BulletMLParserBLB(line);
    if (!brg[i].bulletml->build()) {
      parseFailures++;
      // On-screen trace: show failure
      snprintf(onscreenTrace, sizeof(onscreenTrace), "BLB FAIL %s %d", dirPath, listEntries);
      SRL::Debug::Print(1, 11, onscreenTrace);
      SRL::Core::Synchronize();
      SRL::Logger::LogFatal("[BLB-TRACE] [%s] #%d build-failed: %s", dirPath, listEntries, line);
      SRL::Logger::LogFatal("[BARRAGE] Failed to parse BulletML file: %s/%s", dirPath, line);
      delete brg[i].bulletml;
      brg[i].bulletml = nullptr;
      continue;
    } else {
      // On-screen trace: show success
      snprintf(onscreenTrace, sizeof(onscreenTrace), "BLB OK %s %d", dirPath, listEntries);
      SRL::Debug::Print(1, 11, onscreenTrace);
      SRL::Core::Synchronize();
      SRL::Logger::LogInfo("[BLB-TRACE] [%s] #%d build-ok: %s", dirPath, listEntries, line);
    }
    i++;
  }
  
  // Change back to root directory after loading all files
  //SRL::Logger::LogTrace("[BARRAGE] Changing back to root directory");
  SRL::Cd::ChangeDir((char *)nullptr);
  
  SRL::Logger::LogInfo("[BLB-TRACE] End directory scan: %s (list_entries=%d, loaded=%d, failures=%d)",
                       dirPath, listEntries, i, parseFailures);
  // On-screen debug: show BLB loading end/result
  snprintf(onscreenMsg, sizeof(onscreenMsg), "BLB DONE: %s %d/%d", dirPath, i, listEntries);
  SRL::Debug::Print(1, 9, onscreenMsg);
  SRL::Core::Synchronize();
  if (i <= 0) {
    SRL::Logger::LogFatal("[BARRAGE] No valid BLB patterns loaded from %s; aborting startup", dirPath);
    SRL::System::Exit(1);
  }
  if (parseFailures > 0) {
    SRL::Logger::LogWarning("[BARRAGE] %s loaded with %d parse failures (%d valid patterns)", dirPath, parseFailures, i);
  }
  //SRL::Logger::LogDebug("[BARRAGE] Successfully loaded %d BulletML patterns from %s", i, dirPath);
  return i;
}

static unsigned int rnd;

void initBarragemanager() {
  SRL::Logger::LogDebug("[BARRAGE] Initializing barrage manager");
  
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    SRL::Logger::LogDebug("[BARRAGE] Loading barrage type %d: %s", i, BARRAGE_DIR_NAME[i]);
    barragePatternNum[i] = readBulletMLFiles(BARRAGE_DIR_NAME[i], barragePattern[i]);
    SRL::Logger::LogInfo("[BARRAGE] Type %d: Loaded %d patterns", i, barragePatternNum[i]);
    SRL::Logger::LogInfo("--------");
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      barragePattern[i][j].type = i;
    }
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Barrage manager initialization complete");
}

void closeBarragemanager() {
  SRL::Logger::LogDebug("[BARRAGE] Closing barrage manager");
  
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      delete barragePattern[i][j].bulletml;
    }
  }
  
  SRL::Logger::LogDebug("[BARRAGE] Barrage manager closed");
}

int scene;
int endless, insane;
static int sceneCnt;
static float level, levelInc;

void initBarrages(int seed, float startLevel, float li) {
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
      barrageQueue[i][j]->maxRank = (float)(nextRandInt(&rnd)%70)/100 + 0.3f;
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
static void rollBarragePattern(Barrage *br[], int brNum) {
  Barrage *tbr;
  int n = (int)((float)brNum/((float)(nextRandInt(&rnd)%32)/32+1)+0.5f);
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
void setBarrages(float level, int bm, int midMode) {
  int bpn = 0, bn, i;
  int barrageMax, addFrqLoop = 0;

  SRL::Logger::LogDebug("[BARRAGE] Setting barrage pattern: level=%.2f, bossMode=%d, midMode=%d", level, bm, midMode);

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
  SRL::Logger::LogDebug("[BARRAGE] Barrage pattern set for scene %d: %d patterns, position=(%d,%d)", scene, barrageNum, pax, pay);
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

  // Scene time control.
  sceneCnt--;
  if ( sceneCnt < 0 ) {
    SRL::Logger::LogDebug("[BARRAGE] Scene transition: current scene=%d, level=%.2f", scene, level);
    
    if ( !insane ) clearFoes();
    if ( scene >= 0 && !endless ) setClearScore();
    if ( scene%10 == 8 ) {
      sceneCnt = 999999;
      zakoAppCnt = ZAKO_APP_TERM;
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
    if ( status == IN_GAME ) {
      drawRPanel();
    } else {
      sceneCnt = 999999;
    }
  }

  if ( sceneCnt < SCENE_END_TERM ) return;

  for ( i=0 ; i<barrageNum ; i++ ) {
    if ( bossMode ) {
      if ( i > 0 ) break;
      if ( zakoAppCnt <= 0 ) break;
      zakoAppCnt--;
    }
    type = barrage[i]->type;
    // An additional enemy appears when there is no enemy of the same type.
    if ( type == quickAppType && enNum[type] == 0 ) {
      x = pax; y = pay;
      addFoe(x, y, barrage[i]->rank, 512, 0, type, shield[type], barrage[i]->bulletml);
    }

    frq = appFreq[type]/barrage[i]->frq;
    if ( frq < 2 ) frq = 2;
    if ( (nextRandInt(&rnd)%frq) == 0 ) {
      x = nextRandInt(&rnd)%(SCAN_WIDTH_8*2/3) + (SCAN_WIDTH_8/6);
      y = nextRandInt(&rnd)%(SCAN_HEIGHT_8/6) + (SCAN_HEIGHT_8/10);
      if ( type == quickAppType ) {
	pax = x; pay = y;
      }
      addFoe(x, y, barrage[i]->rank, 512, 0, type, shield[type], barrage[i]->bulletml);
    }
  }
}

void bossDestroied() {
  SRL::Logger::LogDebug("[BARRAGE] Boss defeated at scene %d, level=%.2f", scene, level);
  
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

  SRL::Logger::LogDebug("[BARRAGE] Adding boss bullet patterns, count=%d", barrageNum);

  for ( int i=0 ; i<barrageNum ; i++ ) {
    if ( barrage[i]->type != 2 ) continue;
    if ( bossBullet == nullptr ) {
      SRL::Logger::LogDebug("[BARRAGE] Creating primary boss with pattern index %d, rank=%.2f", i, barrage[i]->rank);
      bl = addFoe(SCAN_WIDTH_8/2, SCAN_HEIGHT_8/5, barrage[i]->rank, 512, 0,
		  BOSS_TYPE, BOSS_SHIELD, barrage[i]->bulletml);
      bossBullet = bl;
    } else {
      SRL::Logger::LogDebug("[BARRAGE] Adding additional boss pattern index %d, rank=%.2f", i, barrage[i]->rank);
      bl = addFoeBossActiveBullet(SCAN_WIDTH_8/2, SCAN_HEIGHT_8/5, barrage[i]->rank, 512, 0,
				  barrage[i]->bulletml);
    }
  }
}
