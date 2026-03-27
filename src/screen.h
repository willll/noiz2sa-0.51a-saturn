/*
 * $Id: screen.h,v 1.3 2002/12/31 09:34:34 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * SDL screen functions header file.
 *
 * @version $Revision: 1.3 $
 */

#include <srl.hpp>  
#include "SDL.h"
#include "vector.h"
#include "gamepad.h"
#include "canvas.h"

using namespace SRL::Input;

#ifndef NOIZ2SA_SCREEN_DIVISOR
#define NOIZ2SA_SCREEN_DIVISOR 2
#endif

#define SCREEN_DIVISOR NOIZ2SA_SCREEN_DIVISOR

#define SCREEN_WIDTH (640 / SCREEN_DIVISOR)
#define SCREEN_HEIGHT (480 / SCREEN_DIVISOR)
#define LAYER_WIDTH (320 / SCREEN_DIVISOR)
#define LAYER_HEIGHT (480 / SCREEN_DIVISOR)
#define PANEL_WIDTH (160 / SCREEN_DIVISOR)
#define PANEL_HEIGHT (480 / SCREEN_DIVISOR)

#define SCAN_WIDTH (320 / SCREEN_DIVISOR)
#define SCAN_HEIGHT (480 / SCREEN_DIVISOR)

#define SCAN_WIDTH_8 (SCAN_WIDTH<<8)
#define SCAN_HEIGHT_8 (SCAN_HEIGHT<<8)

#define BPP 8

#define PAD_UP 1
#define PAD_DOWN 2
#define PAD_LEFT 4
#define PAD_RIGHT 8
#define PAD_BUTTON1 16
#define PAD_BUTTON2 32

#define DEFAULT_BRIGHTNESS 224

#define pitch LAYER_WIDTH
#define lyrSize (LAYER_WIDTH * LAYER_HEIGHT)

extern Canvas::Pixel *l1buf, *l2buf;
extern Canvas::Pixel *buf;
extern Canvas::Pixel *lpbuf, *rpbuf;
extern int buttonReversed;
extern int brightness;
extern Digital *gamepad;

void initSDL();
void closeSDL();
void blendScreen();
void flipScreen();
void clearScreen();
void clearLPanel();
void clearRPanel();
void smokeScreen();
void drawThickLine(int x1, int y1, int x2, int y2, Canvas::Pixel color1, Canvas::Pixel color2, int width);
void drawLine(int x1, int y1, int x2, int y2, Canvas::Pixel color, int width, Canvas::Pixel *buf);
void drawBox(int x, int y, int width, int height,
	     Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
void drawBoxHardware(int x, int y, int width, int height,
		     Canvas::Pixel color1, Canvas::Pixel color2);
void drawBoxPanel(int x, int y, int width, int height,
		  Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
int drawNum(int n, int x ,int y, int s, int c1, int c2);
int drawNumRight(int n, int x ,int y, int s, int c1, int c2);
int drawNumCenter(int n, int x ,int y, int s, int c1, int c2);
void drawSprite(const uint8_t n, const int16_t x, const int16_t y);

int getPadState();
int getButtonState();
