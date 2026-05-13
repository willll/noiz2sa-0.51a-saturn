/*
 * $Id: background.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Backgournd graphics data.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

typedef struct {
  int x, y, z;
  int width, height;
} Board;

#define BOARD_MAX 256

/** @brief Initialises the background system. */
void initBackground();
/** @brief Selects the stage background for the given stage. */
void setStageBackground(int stage);
/** @brief Advances the background animation and scrolling. */
void moveBackground();
/** @brief Draws the background layer. */
void drawBackground();
/** @brief Returns a debug bitmap hash for the background. */
unsigned int getBackgroundDebugBitmapHash();
/** @brief Returns the number of debug boards currently allocated. */
int getBackgroundDebugBoardCount();
