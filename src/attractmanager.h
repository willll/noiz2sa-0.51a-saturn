/*
 * $Id: attractmanager.h,v 1.3 2003/02/09 07:34:15 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

#pragma once

/**
 * Attraction manager header file.
 *
 * @version $Revision: 1.3 $
 */
#define STAGE_NUM 10
#define ENDLESS_STAGE_NUM 4
#define SCENE_NUM 10

typedef struct {
  int stageScore[STAGE_NUM+ENDLESS_STAGE_NUM];
  int sceneScore[STAGE_NUM][SCENE_NUM];
  int stage;
} HiScore;

extern int score, left, stage;

/** @brief Loads user preferences from persistent storage. */
void loadPreference();
/** @brief Saves user preferences to persistent storage. */
void savePreference();
/** @brief Initialises the game state for a stage. */
void initGameState(int stg);
/** @brief Adds score points to the current tally. */
void addScore(int s);
/** @brief Increases the player's ship count if allowed. */
int extendShip();
/** @brief Decreases the player's ship count if possible. */
int decrementShip();
/** @brief Awards the left-bonus bonus. */
void addLeftBonus();
/** @brief Updates the stage-clear score totals. */
void setClearScore();
/** @brief Updates the persistent high-score table. */
void setHiScore();
/** @brief Displays the current score state. */
void showScore();
/** @brief Draws the score overlay. */
void drawScore();
/** @brief Draws the right-side panel UI. */
void drawRPanel();
/** @brief Initialises the attract/title manager. */
void initAttractManager();
/** @brief Initialises the title attract scene. */
int initTitleAtr();
/** @brief Draws the title screen. */
void drawTitle();
/** @brief Draws the title menu. */
void drawTitleMenu();
/** @brief Initialises the game-over attract scene. */
void initGameoverAtr();
/** @brief Advances the game-over scene. */
void moveGameover();
/** @brief Draws the game-over screen. */
void drawGameover();
/** @brief Initialises the stage-clear attract scene. */
void initStageClearAtr();
/** @brief Advances the stage-clear scene. */
void moveStageClear();
/** @brief Draws the stage-clear screen. */
void drawStageClear();
/** @brief Advances the title menu animation. */
void moveTitleMenu();
/** @brief Advances the pause menu. */
void movePause();
/** @brief Draws the pause screen. */
void drawPause();
/** @brief Draws the title screen. */
void drawTitle();
