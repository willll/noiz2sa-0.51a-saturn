/*
 * $Id: screen.h,v 1.3 2002/12/31 09:34:34 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
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
	uint32_t vdp1BlitUploadUs;
	uint32_t vdp1BlitDrawUs;
	uint32_t lineCommandsSubmitted;  ///< Hardware line draws (SRL::Scene2D::DrawLine) this frame
	uint32_t polygonsSubmitted;      ///< Hardware polygon draws (SRL::Scene2D::DrawPolygon) this frame
	uint32_t vdp1SubmitFailures;     ///< DrawLine/DrawPolygon/DrawSprite calls that returned false this frame
};

/** @brief Initialises SDL and the rendering subsystem. */
void initSDL();
/** @brief Marks the entire playfield as dirty for redraw. */
void markPlayfieldDirty();
/** @brief Master, right after the frame sync: the slave fades the smoke trails into buf
 *  (or clears it), then runs the job set with setPlayfieldSlaveJob(). */
void startPlayfieldPrep();
/** @brief Master, before drawing: waits for that slave run (or does it here if none). */
void waitPlayfieldPrep();
/** @brief Master, after the trail group (foes, wakes, frags, bonuses) is drawn: keeps
 *  buf as the next frame's trail layer. Shots, ship and bullets come after, so no trails. */
void saveTrailLayer();
/** @brief Registers extra per-frame playfield work for the slave (bonus stars). */
void setPlayfieldSlaveJob(void (*prepareOnMaster)(), void (*runOnSlave)());
/** @brief Store 4 / 2 pixels in one bus write (p 4- / 2-byte aligned). The playfield
 *  is write-through RAM, so every store costs a bus write. */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline void storePixels4(uint8_t *p, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
  *reinterpret_cast<uint32_t *>(p) = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}
inline void storePixels2(uint8_t *p, uint8_t a, uint8_t b)
{
  *reinterpret_cast<uint16_t *>(p) = (uint16_t)((a << 8) | b);
}
#else // host builds (pixel test) only: same bytes at the same addresses
inline void storePixels4(uint8_t *p, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
  p[0] = a; p[1] = b; p[2] = c; p[3] = d;
}
inline void storePixels2(uint8_t *p, uint8_t a, uint8_t b)
{
  p[0] = a; p[1] = b;
}
#endif
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
