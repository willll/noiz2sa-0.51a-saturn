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

#pragma once

#include <srl.hpp>  
#include "SDL.h"
#include "vector.h"
#include "gamepad.h"
#include "canvas.h"

using namespace SRL::Input;

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

struct ScreenVdpPerfStats
{
	uint32_t panelUploadUs;
	uint32_t panelUploadBytes;
	uint32_t playfieldUploadUs;
	uint32_t playfieldUploadBytes;
	uint32_t flipPresentUs;
	uint32_t hwLineUs;
	uint32_t blendCopyUs;
	uint32_t blendAlphaUs;
	uint32_t vdp1BlitUploadUs;
	uint32_t vdp1BlitDrawUs;
};

/** @brief Initialises SDL and the rendering subsystem. */
void initSDL();
/** @brief Shuts down SDL and releases rendering resources. */
void closeSDL();
/** @brief Blends the playfield and panel surfaces. */
void blendScreen();
/** @brief Marks the entire playfield as dirty for redraw. */
void markPlayfieldDirty();
/** @brief Marks a rectangular playfield region as dirty. */
void markPlayfieldDirtyRect(int x, int y, int width, int height);
/** @brief Presents the current frame to the display. */
void flipScreen();
/** @brief Clears the main screen buffer. */
void clearScreen();
/** @brief Clears the left panel buffer. */
void clearLPanel();
/** @brief Clears the right panel buffer. */
void clearRPanel();
/** @brief Draws the screen smoke transition effect. */
void smokeScreen();
/** @brief Copies the current VDP performance stats into the output structure. */
void consumeScreenVdpPerfStats(ScreenVdpPerfStats *outStats);
/** @brief Draws a thick line with two colours. */
void drawThickLine(int x1, int y1, int x2, int y2, Canvas::Pixel color1, Canvas::Pixel color2, int width);
/** @brief Draws a line into the given pixel buffer. */
void drawLine(int x1, int y1, int x2, int y2, Canvas::Pixel color, int width, Canvas::Pixel *buf);
/** @brief Draws a filled box into the given pixel buffer. */
void drawBox(int x, int y, int width, int height,
	     Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
/** @brief Draws a filled panel box into the given pixel buffer. */
void drawBoxPanel(int x, int y, int width, int height,
		  Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
/** @brief Draws an integer as a sprite number sequence. */
int drawNum(int n, int x ,int y, int s, int c1, int c2);
/** @brief Draws an integer aligned to the right. */
int drawNumRight(int n, int x ,int y, int s, int c1, int c2);
/** @brief Draws an integer centered at the target position. */
int drawNumCenter(int n, int x ,int y, int s, int c1, int c2);
/** @brief Draws a sprite from the sprite table. */
void drawSprite(const uint8_t n, const int16_t x, const int16_t y);

/** @brief Returns the current pad state bitmask. */
int getPadState();
/** @brief Returns the current button state bitmask. */
int getButtonState();
