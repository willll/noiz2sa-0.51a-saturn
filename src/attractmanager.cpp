/*
 * $Id: attractmanager.c,v 1.4 2003/02/09 07:34:15 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Attraction(score/title/gameover) manager.
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include <srl_cd.hpp>
#include <srl_memory.hpp> // for malloc/free
#include <srl_log.hpp>    // for file I/O logging
#include <srl_string.hpp> // for string functions

#include "noiz2sa.h"
#include "screen.h"
#include "letterrender.h"
#include "attractmanager.h"
#include "bonus.h"
#include "brgmng_mtd.h"
#include "soundmanager.h"
#include "degutil.h"

int score;
static int nextExtend, neAdd;
static int dsc;
int left, stage;
static int ssSc;
static int hsScene, hsScSc, hsOfs;

static HiScore hiScore;

#define PREF_FILE "NOIZ2SA.PRF"
#define DEFAULT_HISCORE 100000
#define DEFAULT_SCENE_HISCORE 10000

static bool readInt32FromCd(SRL::Cd::File &file, int *value)
{
  if (!value)
    return false;
  return file.Read((int32_t)sizeof(int), value) == (int32_t)sizeof(int);
}

static void initHiScore()
{
  int i, j;
  for (i = 0; i < STAGE_NUM; i++)
  {
    hiScore.stageScore[i] = DEFAULT_HISCORE;
    for (j = 0; j < SCENE_NUM; j++)
    {
      hiScore.sceneScore[i][j] = DEFAULT_SCENE_HISCORE;
    }
  }
  for (i = 0; i < ENDLESS_STAGE_NUM; i++)
  {
    hiScore.stageScore[i + STAGE_NUM] = DEFAULT_HISCORE;
  }
  hiScore.stage = 0;
}

// Load preference.
void loadPreference()
{
  int i, j;
  int version;
  SRL::Cd::File prefFile(PREF_FILE);

  initHiScore();

  // Preferences are read-only on Saturn (from CD image).
  if (!prefFile.Exists() || !prefFile.Open())
  {
    return;
  }

  if (!readInt32FromCd(prefFile, &version) || version != VERSION_NUM)
  {
    prefFile.Close();
    initHiScore();
    return;
  }

  for (i = 0; i < STAGE_NUM; i++)
  {
    if (!readInt32FromCd(prefFile, &hiScore.stageScore[i]))
    {
      prefFile.Close();
      initHiScore();
      return;
    }
    for (j = 0; j < SCENE_NUM; j++)
    {
      if (!readInt32FromCd(prefFile, &hiScore.sceneScore[i][j]))
      {
        prefFile.Close();
        initHiScore();
        return;
      }
    }
  }
  for (i = 0; i < ENDLESS_STAGE_NUM; i++)
  {
    if (!readInt32FromCd(prefFile, &hiScore.stageScore[i + STAGE_NUM]))
    {
      prefFile.Close();
      initHiScore();
      return;
    }
  }
  if (!readInt32FromCd(prefFile, &hiScore.stage))
  {
    prefFile.Close();
    initHiScore();
    return;
  }
  prefFile.Close();
}

// Save preference.
void savePreference()
{
  // CD media is read-only on Saturn; keep in-memory highscores only.
}

void initGameState(int stg)
{
  score = 0;
  ssSc = 0;
  nextExtend = 200000;
  neAdd = 300000;
  dsc = -1;
  left = 2;
  hiScore.stage = stage = stg;
  hsScene = -1;
  drawRPanel();
}

void addScore(int s)
{
  score += s;
  if (score >= nextExtend)
  {
    nextExtend += neAdd;
    neAdd = 500000;
    if (extendShip())
    {
      playChunk(6);
    }
  }
}

int extendShip()
{
  if (left > 8)
    return 0;
  left++;
  drawRPanel();
  return 1;
}

int decrementShip()
{
  left--;
  drawRPanel();
  if (left < 0)
    return 1;
  return 0;
}

void addLeftBonus()
{
  nextExtend = 999999999;
  addScore(left * 100000);
}

void setClearScore()
{
  int rss = hiScore.sceneScore[stage][scene];
  int ss = score - ssSc;
  hsScene = scene;
  hsOfs = ss - rss;
  hsScSc = ss;
  if (ss > rss)
  {
    hiScore.sceneScore[stage][scene] = ss;
  }
  drawRPanel();
  ssSc = score;
}

void setHiScore()
{
  if (score > hiScore.stageScore[stage])
  {
    hiScore.stageScore[stage] = score;
  }
}

void showScore()
{
  dsc = -1;
}

void drawScore()
{
  if (dsc == score)
    return;
  dsc = score;
  clearLPanel();
  drawNum(score, 118 / SCREEN_DIVISOR, 24 / SCREEN_DIVISOR, 28, 16 * 1 - 12, 16 * 1 - 3);
  drawNum(bonusScore, 24 / SCREEN_DIVISOR, 14 / SCREEN_DIVISOR, 16, 16 * 1 - 12, 16 * 1 - 3);
}

#define SCENE_STAT_X (77 / SCREEN_DIVISOR)
#define SCENE_STAT_SIZE (9 / SCREEN_DIVISOR)

void drawRPanel()
{
  int y;
  const char *str = "LEFT";
  clearRPanel();
  if (left >= 0)
  {
    drawString(str, 34 / SCREEN_DIVISOR, 272 / SCREEN_DIVISOR, 24, 3, 16 * 1 - 12, 16 * 1 - 3, rpbuf);
    drawLetter(left, 34 / SCREEN_DIVISOR, 450 / SCREEN_DIVISOR, 24, 3, 16 * 2 - 10, 16 * 2 - 1, rpbuf);
  }
  y = 24 / SCREEN_DIVISOR;
  if (!endless)
  {
    y = drawNumRight(stage + 1, 124 / SCREEN_DIVISOR, y, 24, 16 * 1 - 12, 16 * 1 - 3);
    drawLetter(38, 124 / SCREEN_DIVISOR, y, 24, 3, 16 * 1 - 12, 16 * 1 - 3, rpbuf);
    y += ((Fxp::Convert(24) * 1.7f) / Fxp::Convert(SCREEN_DIVISOR)).As<int>();
    if (scene >= 10)
    {
      drawLetter('E' - 'A' + 10, 124 / SCREEN_DIVISOR, y, 24, 3, 16 * 1 - 12, 16 * 1 - 3, rpbuf);
      return;
    }
  }
  drawNumRight(scene + 1, 124 / SCREEN_DIVISOR, y, 24, 16 * 1 - 12, 16 * 1 - 3);
  if (hsScene >= 0)
  {
    y = SCENE_STAT_SIZE;
    y = drawNumRight(stage + 1, SCENE_STAT_X, y, SCENE_STAT_SIZE, 16 * 1 - 12, 16 * 1 - 3);
    drawLetter(38, SCENE_STAT_X, y, SCENE_STAT_SIZE, 3, 16 * 1 - 12, 16 * 1 - 3, rpbuf);
    y += SCENE_STAT_SIZE * 1.7f;
    y = drawNumRight(hsScene + 1, SCENE_STAT_X, y, SCENE_STAT_SIZE, 16 * 1 - 12, 16 * 1 - 3);
    y += SCENE_STAT_SIZE * 1.7f * 2;
    y = drawNumRight(hsScSc, SCENE_STAT_X, y, SCENE_STAT_SIZE, 16 * 1 - 12, 16 * 1 - 3);
    y += SCENE_STAT_SIZE * 1.7f;
    if (hsOfs >= 0)
    {
      drawLetter(39, SCENE_STAT_X, y, SCENE_STAT_SIZE, 3, 16 * 2 - 12, 16 * 2 - 3, rpbuf);
      y += SCENE_STAT_SIZE * 1.7f;
      drawNumRight(hsOfs, SCENE_STAT_X, y, SCENE_STAT_SIZE, 16 * 2 - 12, 16 * 2 - 3);
    }
    else
    {
      drawLetter(38, SCENE_STAT_X, y, SCENE_STAT_SIZE, 3, 16 * 4 - 12, 16 * 4 - 3, rpbuf);
      y += SCENE_STAT_SIZE * 1.7f;
      drawNumRight(-hsOfs, SCENE_STAT_X, y, SCENE_STAT_SIZE, 16 * 4 - 12, 16 * 4 - 3);
    }
  }
}

#define STG_BOX_SIZE (40 / SCREEN_DIVISOR)
#define STG_BOX_NUM 15

static int stageX[STG_BOX_NUM], stageY[STG_BOX_NUM];

void initAttractManager()
{
  int i, j, x, y, s;
  y = LAYER_HEIGHT / 3 + STG_BOX_SIZE / 2;
  s = 0;
  for (i = 0; i < 6; i++, y += STG_BOX_SIZE * 1.2f)
  {
    x = STG_BOX_SIZE / 2 + STG_BOX_SIZE / 2;
    switch (i)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      for (j = 0; j <= i; j++, s++, x += STG_BOX_SIZE * 1.2f)
      {
        stageX[s] = x;
        stageY[s] = y;
      }
      break;
    case 4:
      for (j = 0; j <= 2; j++, s++, x += STG_BOX_SIZE * 1.2f)
      {
        stageX[s] = x;
        stageY[s] = y;
      }
      x += STG_BOX_SIZE * 1.2f;
      stageX[s] = x;
      stageY[s] = y;
      s++;
      break;
    case 5:
      y += STG_BOX_SIZE / 3;
      stageX[s] = x;
      stageY[s] = y;
      break;
    }
  }
}

static int titleCnt;
static int slcStg;
static int mnp;

int initTitleAtr()
{
  SRL::Logger::LogDebug("[CDDA] initTitleAtr: stopMusic()");
  stopMusic();
  titleCnt = 0;
  slcStg = hiScore.stage;
  mnp = 0;
  return slcStg;
}

void drawTitle()
{
  const char *title = "NOIZ2SA";
  const int boxSize = 32 / SCREEN_DIVISOR;
  const int step = 36 / SCREEN_DIVISOR;
  const int totalWidth = 7 * step;
  const int baseX = (160 - totalWidth) / 2;
  const int baseY = 30 / SCREEN_DIVISOR;

  for (int i = 0; title[i] != '\0'; i++)
  {
    const int x = baseX + i * step;
    const int y = baseY;
    drawBox(x, y, boxSize, boxSize, 1, 14, buf);

    int idx = 37;
    const char c = title[i];
    if (c >= '0' && c <= '9')
    {
      idx = c - '0';
    }
    else if (c >= 'A' && c <= 'Z')
    {
      idx = c - 'A' + 10;
    }

    drawLetterBuf(idx, x, y, 12, 2, 15, 15, buf, 0);
  }
}

static int stgMv[STAGE_NUM + ENDLESS_STAGE_NUM + 1][4] = {
    {0, 0, 1, 0},
    {-1, 1, 2, 0},
    {0, 0, 2, -1},
    {-2, 1, 3, 0},
    {-2, 1, 3, -1},
    {0, 0, 3, -1},
    {-3, 1, 4, 0},
    {-3, 1, 4, -1},
    {-3, 1, 4, -1},
    {0, 0, 3, -1},
    {-4, 1, 4, 0},
    {-4, 1, 3, -1},
    {-4, 1, 2, -1},
    {0, 0, 1, -1},
    {-4, 0, 0, 0},
};

void moveTitleMenu()
{
  int pad = getPadState();
  int btn = getButtonState();
  int p = -1;
  int sm;
  if (pad & PAD_DOWN)
  {
    p = 2;
  }
  else if (pad & PAD_UP)
  {
    p = 0;
  }
  else if (pad & PAD_RIGHT)
  {
    p = 1;
  }
  else if (pad & PAD_LEFT)
  {
    p = 3;
  }
  else if (btn == 0)
  {
    mnp = 1;
  }
  if (mnp && p >= 0)
  {
    mnp = 0;
    sm = stgMv[slcStg][p];
    slcStg += sm;
    if (sm != 0)
    {
      initTitleStage(slcStg);
    }
    titleCnt = 16;
  }
  if (mnp && (btn & PAD_BUTTON1))
  {
    if (slcStg == STAGE_NUM + ENDLESS_STAGE_NUM)
    {
      quitLast();
      return;
    }
    hiScore.stage = slcStg;
    initGame(slcStg);
  }
  titleCnt++;
}

void drawTitleMenu()
{
  int i;
  const char *stgChr = "STAGE";
  const char *endlessChr = "ENDLESS";
  const char *hardChr = "HARD";
  const char *extChr = "EXTREME";
  const char *insChr = "INSANE";
  const char *quitChr = "QUIT";
  for (i = 0; i < STG_BOX_NUM; i++)
  {
    if (i == slcStg)
    {
      int sz = STG_BOX_SIZE + (6 + sctbl[(titleCnt * 16) & (DIV - 1)] / 24) / SCREEN_DIVISOR;
      drawBox(stageX[i], stageY[i], sz, sz, 16 * 2 - 14, 16 * 2 - 3, buf);
      if (i < STAGE_NUM)
      {
        drawStringBuf(stgChr, 180 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
        drawNumCenter(i + 1, 308 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 16 * 1 - 14, 16 * 1 - 2);
      }
      else
      {
        switch (i)
        {
        case 10:
          drawStringBuf(endlessChr, 188 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          break;
        case 11:
          drawStringBuf(endlessChr, 93 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          drawStringBuf(hardChr, 248 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          break;
        case 12:
          drawStringBuf(endlessChr, 36 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          drawStringBuf(extChr, 190 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          break;
        case 13:
          drawStringBuf(endlessChr, 56 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          drawStringBuf(insChr, 210 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          break;
        case 14:
          drawStringBuf(quitChr, 230 / SCREEN_DIVISOR, 80 / SCREEN_DIVISOR, 12, 2, 16 * 1 - 14, 16 * 1 - 2, buf, 0);
          break;
        }
      }
      if (i < STAGE_NUM + ENDLESS_STAGE_NUM)
      {
        drawNumCenter(hiScore.stageScore[i], 308 / SCREEN_DIVISOR, 112 / SCREEN_DIVISOR, 12, 16 * 1 - 14, 16 * 1 - 2);
      }
    }
    drawBox(stageX[i], stageY[i], STG_BOX_SIZE, STG_BOX_SIZE, 16 * 1 - 14, 16 * 1 - 3, buf);
    if (i < 9)
    {
      drawNumCenter(i + 1, stageX[i], stageY[i], 12, 16 * 1 - 16, 16 * 1 - 1);
    }
    else
    {
      switch (i)
      {
      case 9:
        drawNumCenter(10, stageX[i] + (8 / SCREEN_DIVISOR), stageY[i], 12, 16 * 1 - 16, 16 * 1 - 1);
        break;
      case 10:
        drawLetterBuf('E' - 'A' + 10, stageX[i], stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        break;
      case 11:
        drawLetterBuf('E' - 'A' + 10, stageX[i] - (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        drawLetterBuf('H' - 'A' + 10, stageX[i] + (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        break;
      case 12:
        drawLetterBuf('E' - 'A' + 10, stageX[i] - (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        drawLetterBuf('E' - 'A' + 10, stageX[i] + (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        break;
      case 13:
        drawLetterBuf('E' - 'A' + 10, stageX[i] - (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        drawLetterBuf('I' - 'A' + 10, stageX[i] + (8 / SCREEN_DIVISOR), stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        break;
      case 14:
        drawLetterBuf('Q' - 'A' + 10, stageX[i], stageY[i], 12, 2, 16 * 1 - 16, 16 * 1 - 1, buf, 0);
        break;
      }
    }
  }
}

static int goCnt;

void initGameoverAtr()
{
  goCnt = 0;
  mnp = 0;
  SRL::Logger::LogDebug("[CDDA] initGameoverAtr: fadeMusic()");
  fadeMusic();
}

void moveGameover()
{
  int btn = getButtonState();
  if (goCnt > 900 || (goCnt > 128 && mnp && (btn & PAD_BUTTON1)))
  {
    setHiScore();
    initTitle();
    return;
  }
  if (btn == 0)
  {
    mnp = 1;
  }
  goCnt++;
}

void drawGameover()
{
  const char *goChr = "GAME OVER";
  int y;
  if (goCnt < 128)
  {
    y = LAYER_HEIGHT / 3 * goCnt / 128;
  }
  else
  {
    y = LAYER_HEIGHT / 3;
  }
  drawStringBuf(goChr, 24 / SCREEN_DIVISOR, y, 20, 2, 16 * 4 - 10, 16 * 1 - 1, buf, 0);
}

static int scCnt;

void initStageClearAtr()
{
  scCnt = 0;
  mnp = 0;
  SRL::Logger::LogDebug("[CDDA] initStageClearAtr: fadeMusic()");
  fadeMusic();
}

void moveStageClear()
{
  int btn = getButtonState();
  if (scCnt > 900 || (scCnt > 128 && mnp && (btn & PAD_BUTTON1)))
  {
    setHiScore();
    initTitle();
    return;
  }
  if (btn == 0)
  {
    mnp = 1;
  }
  scCnt++;
}

void drawStageClear()
{
  const char *scChr = "STAGE CLEAR";
  int y;
  if (scCnt < 128)
  {
    y = LAYER_HEIGHT - LAYER_HEIGHT / 3 * 2 * scCnt / 128;
  }
  else
  {
    y = LAYER_HEIGHT / 3;
  }
  drawStringBuf(scChr, 24 / SCREEN_DIVISOR, y, 16, 2, 16 * 3 - 10, 16 * 1 - 1, buf, 0);
}

static int psCnt = 0;

void movePause()
{
  psCnt++;
}

void drawPause()
{
  const char *psChr = "PAUSE";
  if ((psCnt & 63) < 32)
  {
    drawStringBuf(psChr, 92 / SCREEN_DIVISOR, LAYER_HEIGHT / 3, 20, 2, 16 * 2 - 10, 16 * 1 - 1, buf, 0);
  }
}
