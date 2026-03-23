/*
 * $Id: screen.c,v 1.3 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 * Copyright 2016 Michal Nowikowski. All rights reserved.
 */

/**
 * SDL screen handler.
 *
 * @version $Revision: 1.3 $
 */
#include <math.h>
#include <srl.hpp>
#include <srl_log.hpp> // for logging
// #include <srl_memory.hpp>  // for malloc/free
#include <srl_string.hpp> // for string and memory functions
#include <srl_system.hpp> // for exit
#include <srl_bitmap.hpp> // for TGA loading
#include "SDL.h"

#include "noiz2sa.h"
#include "screen.h"
#include "clrtbl.h"
#include "vector.h"
#include "degutil.h"
#include "letterrender.h"
#include "attractmanager.h"
#include "gamepad.h"
#include "canvas_palette.h"

using namespace SRL::Math::Types;

int brightness = DEFAULT_BRIGHTNESS;

static SRL_Surface *video, *layer, *lpanel, *rpanel;
static SRL_Surface videoSurface;
static SRL_Surface layerSurface;
static SRL_Surface lpanelSurface;
static SRL_Surface rpanelSurface;

static Canvas::Pixel **smokeBuf;
static Canvas::Pixel *pbuf;
Canvas::Pixel *l1buf, *l2buf;
Canvas::Pixel *buf;
Canvas::Pixel *lpbuf, *rpbuf;
static Canvas *layerCanvas = nullptr;
static Canvas *leftPanelCanvas = nullptr;
static Canvas *rightPanelCanvas = nullptr;
static Canvas *smokeCanvas = nullptr;
static Canvas *layer1Canvas = nullptr;
static Canvas *layer2Canvas = nullptr;
static Palette *canvasPalette = nullptr;
static Canvas::Pixel smokeFallback = 0;
static SDL_Rect layerRect, layerClearRect;
static SDL_Rect lpanelRect, rpanelRect, panelClearRect;
static int pitch;

// Handle TGA images in ISO 8:3 format
#define SPRITE_NUM 7

static SRL_Surface sprite[SPRITE_NUM];
static const char *spriteFile[SPRITE_NUM] = {
    "TITLEN.TGA",
    "TITLEO.TGA",
    "TITLEI.TGA",
    "TITLEZ.TGA",
    "TITLE2.TGA",
    "TITLES.TGA",
    "TITLEA.TGA",
};

// SDL_Joystick *stick = nullptr;
// SDL_GameController *gamepad = nullptr;
Digital *gamepad = nullptr;

static void loadSprites()
{
  SRL::Logger::LogInfo("[SPRITE] Beginning sprite loading sequence");

  int i;
  char name[256];
  SRL::Bitmap::TGA *tga = nullptr;
  // SRL_Surface *img = nullptr;

  // FIX ME !
  // color[0].Red = 100 >> 3; color[0].Green = 0; color[0].Blue = 0;
  // SDL_SetColors(video, color, 0, 1);

  // Navigate to IMAGES directory on CD first
  SRL::Logger::LogDebug("[SPRITE] Changing to IMAGES directory on CD");
  SRL::Cd::ChangeDir((char *)nullptr);
  SRL::Cd::ChangeDir("IMAGES");
  SRL::Logger::LogDebug("[SPRITE] Successfully navigated to IMAGES directory");

  for (i = 0; i < SPRITE_NUM; i++)
  {
    SRL::Logger::LogInfo("[SPRITE] Loading sprite %d: %s", i, spriteFile[i]);

    // Load TGA file directly from IMAGES directory (already changed via ChangeDir above)
    strcpy(name, spriteFile[i]);

    // Load TGA file using SRL bitmap loader
    tga = new SRL::Bitmap::TGA(name);

    if (tga == nullptr || tga->GetData() == nullptr)
    {
      SRL::Logger::LogFatal("Failed to load TGA sprite: %s", name);
      SRL::System::Exit(1);
    }

    // Get bitmap info to check if loaded successfully
    SRL::Bitmap::BitmapInfo info = tga->GetInfo();
    //SRL::Logger::LogTrace("[SPRITE] TGA info: %dx%d pixels", info.Width, info.Height);

    if (info.Width == 0 || tga->GetData() == nullptr)
    {
      SRL::Logger::LogFatal("Unable to load TGA sprite: %s", spriteFile[i]);
      SRL::System::Exit(1);
    }

    int32_t textureIndex = -1;

    if (info.ColorMode == SRL::CRAM::TextureColorMode::RGB555) // RGBA texture
    {
      textureIndex = SRL::VDP1::TryLoadTexture(tga); // Loads TGA into VDP1
    }
    else
    {
      // assume is pallet texture
      textureIndex = SRL::VDP1::TryLoadTexture(tga, Palette::LoadPalette);
    }

    if (textureIndex < 0)
    {
      SRL::Logger::LogFatal("textureIndex(%d) not loaded", textureIndex);
      SRL::Logger::LogFatal("Texture count =%d", SRL::VDP1::GetTextureCount());
      SRL::Logger::LogFatal("Available memory =%u", SRL::VDP1::GetAvailableMemory());
      SRL::System::Exit(1);
    }
    else
    {
      //SRL::Logger::LogTrace("[SPRITE] TGA loaded into VDP1 with texture index: %d", textureIndex);
    }

    //SRL::Logger::LogTrace("[SPRITE] TGA object created for: %s", spriteFile[i]);

    delete tga;

    sprite[i] = SRL_Surface(textureIndex, info.Width, info.Height);
    SRL::Logger::LogTrace("[SPRITE] SDL surface created for sprite %d", i);

    // SDL_ConvertSurface(img,
    // 		   video->format,
    // 		   SDL_HWSURFACE | SDL_SRCCOLORKEY);

    // if ( sprite[i] == nullptr ) {
    //   SRL::Logger::LogFatal("Failed to convert sprite to video format: %s", spriteFile[i]);
    //   SRL::System::Exit(1);
    // }

    SRL::Logger::LogTrace("[SPRITE] Sprite %d converted to video format (colorkey=0)", i);
    // SDL_SetColorKey(sprite[i], SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
    SRL::Logger::LogDebug("[SPRITE] Sprite %d loaded successfully", i);
  }

  color[0].Red = color[0].Green = color[0].Blue = 31;
  SDL_SetColors(video, color, 0, 1);

  SRL::Logger::LogInfo("[SPRITE] Sprite loading complete: %d sprites loaded", SPRITE_NUM);
}

void drawSprite(const uint8_t n, const int16_t x, const int16_t y)
{
  SRL::Math::Types::Vector3D pos(Fxp::Convert(x), Fxp::Convert(y), 500.0);
  SRL::Scene2D::DrawSprite(sprite[n].textureIndex, pos);
}

static int lyrSize;

static void makeSmokeBuf()
{
  int x, y, mx, my;

  if (canvasPalette == nullptr)
  {
    canvasPalette = new Palette(256);
    if (canvasPalette == nullptr)
    {
      SRL::Logger::LogFatal("Couldn't allocate canvas palette.");
      SRL::System::Exit(1);
    }
    canvasPalette->Init();
  }

  if (smokeCanvas == nullptr)
  {
    smokeCanvas = new Canvas(pitch, LAYER_HEIGHT, *canvasPalette);
  }
  if (layer1Canvas == nullptr)
  {
    layer1Canvas = new Canvas(pitch, LAYER_HEIGHT, *canvasPalette);
  }
  if (layer2Canvas == nullptr)
  {
    layer2Canvas = new Canvas(pitch, LAYER_HEIGHT, *canvasPalette);
  }

  if (smokeCanvas == nullptr || layer1Canvas == nullptr || layer2Canvas == nullptr)
  {
    SRL::Logger::LogFatal("Couldn't allocate Canvas buffers.");
    SRL::System::Exit(1);
  }

  pbuf = smokeCanvas->GetData();
  l1buf = layer1Canvas->GetData();
  l2buf = layer2Canvas->GetData();

  if (pbuf == nullptr || l1buf == nullptr || l2buf == nullptr)
  {
    SRL::Logger::LogFatal("Couldn't access Canvas pixel buffers.");
    SRL::System::Exit(1);
  }

  lyrSize = sizeof(Canvas::Pixel) * pitch * LAYER_HEIGHT;

  if (smokeBuf != nullptr)
  {
    free(smokeBuf);
    smokeBuf = nullptr;
  }

  smokeBuf = (Canvas::Pixel **)malloc(sizeof(Canvas::Pixel *) * pitch * LAYER_HEIGHT);
  if (smokeBuf == nullptr)
  {
    SRL::Logger::LogWarning("Couldn't allocate smokeBuf lookup table. Using fallback smoke mode.");
    return;
  }

  smokeFallback = 0;
  for (y = 0; y < LAYER_HEIGHT; y++)
  {
    for (x = 0; x < LAYER_WIDTH; x++)
    {
      mx = x + sctbl[(x * 8) & (DIV - 1)] / 128;
      my = y + sctbl[(y * 8) & (DIV - 1)] / 128;
      if (mx < 0 || mx >= LAYER_WIDTH || my < 0 || my >= LAYER_HEIGHT)
      {
        smokeBuf[x + y * pitch] = &smokeFallback;
      }
      else
      {
        smokeBuf[x + y * pitch] = &(pbuf[mx + my * pitch]);
      }
    }
  }
}

void initSDL()
{
  videoSurface = SRL_Surface(-1, SCREEN_WIDTH, SCREEN_HEIGHT);
  layerSurface = SRL_Surface(-1, LAYER_WIDTH, LAYER_HEIGHT);
  lpanelSurface = SRL_Surface(-1, PANEL_WIDTH, PANEL_HEIGHT);
  rpanelSurface = SRL_Surface(-1, PANEL_WIDTH, PANEL_HEIGHT);

  video = &videoSurface;
  layer = &layerSurface;
  lpanel = &lpanelSurface;
  rpanel = &rpanelSurface;

  if (canvasPalette == nullptr)
  {
    canvasPalette = new Palette(256);
    if (canvasPalette == nullptr)
    {
      SRL::Logger::LogFatal("Couldn't allocate canvas palette.");
      SRL::System::Exit(1);
    }
    canvasPalette->Init();
  }

  if (layerCanvas == nullptr)
  {
    layerCanvas = new Canvas(LAYER_WIDTH, LAYER_HEIGHT, *canvasPalette);
  }
  if (leftPanelCanvas == nullptr)
  {
    leftPanelCanvas = new Canvas(PANEL_WIDTH, PANEL_HEIGHT, *canvasPalette);
  }
  if (rightPanelCanvas == nullptr)
  {
    rightPanelCanvas = new Canvas(PANEL_WIDTH, PANEL_HEIGHT, *canvasPalette);
  }

  if (layerCanvas == nullptr || leftPanelCanvas == nullptr || rightPanelCanvas == nullptr)
  {
    SRL::Logger::LogFatal("Couldn't allocate render Canvas objects.");
    SRL::System::Exit(1);
  }

  layerRect.x = (SCREEN_WIDTH - LAYER_WIDTH) / 2;
  layerRect.y = (SCREEN_HEIGHT - LAYER_HEIGHT) / 2;
  layerRect.w = LAYER_WIDTH;
  layerRect.h = LAYER_HEIGHT;
  layerClearRect.x = layerClearRect.y = 0;
  layerClearRect.w = LAYER_WIDTH;
  layerClearRect.h = LAYER_HEIGHT;
  lpanelRect.x = 0;
  lpanelRect.y = (SCREEN_HEIGHT - PANEL_HEIGHT) / 2;
  rpanelRect.x = SCREEN_WIDTH - PANEL_WIDTH;
  rpanelRect.y = (SCREEN_HEIGHT - PANEL_HEIGHT) / 2;
  lpanelRect.w = rpanelRect.w = PANEL_WIDTH;
  lpanelRect.h = rpanelRect.h = PANEL_HEIGHT;
  panelClearRect.x = panelClearRect.y = 0;
  panelClearRect.w = PANEL_WIDTH;
  panelClearRect.h = PANEL_HEIGHT;

  pitch = LAYER_WIDTH;
  buf = layerCanvas->GetData();
  lpbuf = leftPanelCanvas->GetData();
  rpbuf = rightPanelCanvas->GetData();

  Palette::initPalette();
  makeSmokeBuf();
  clearLPanel();
  clearRPanel();

  loadSprites();
  // if (joystickMode == 1) {
  // stick = SDL_JoystickOpen(0);
  // SDL_GameControllerAddMappingsFromFile(SHARE_LOC "gamecontrollerdb.txt", 1);

  //}

  // SDL_WM_GrabInput(SDL_GRAB_ON);
}

void blendScreen()
{
  int i;
  for (i = lyrSize - 1; i >= 0; i--)
  {
    buf[i] = colorAlp[l1buf[i]][l2buf[i]];
  }
}

void flipScreen()
{
  SDL_BlitSurface(layer, nullptr, video, &layerRect);
  SDL_BlitSurface(lpanel, nullptr, video, &lpanelRect);
  SDL_BlitSurface(rpanel, nullptr, video, &rpanelRect);
  if (status == TITLE)
  {
    drawTitle();
  }
  SDL_Flip(video);
}

void clearScreen()
{
  memset(buf, 0, LAYER_WIDTH * LAYER_HEIGHT);
}

void clearLPanel()
{
  memset(lpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);
}

void clearRPanel()
{
  memset(rpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);
}

void smokeScreen()
{
  int i;
  smokeFallback = 0;
  memcpy(pbuf, l2buf, lyrSize);

  if (smokeBuf == nullptr)
  {
    for (i = lyrSize - 1; i >= 0; i--)
    {
      l1buf[i] = colorDfs[l1buf[i]];
      l2buf[i] = colorDfs[pbuf[i]];
    }
    return;
  }

  for (i = lyrSize - 1; i >= 0; i--)
  {
    l1buf[i] = colorDfs[l1buf[i]];
    l2buf[i] = colorDfs[*(smokeBuf[i])];
  }
}

void drawLine(int x1, int y1, int x2, int y2, Canvas::Pixel color, int width, Canvas::Pixel *buf)
{
  int lx, ly, ax, ay, x, y, ptr, i, j;
  int xMax, yMax;

  lx = absN(x2 - x1);
  ly = absN(y2 - y1);
  if (lx < ly)
  {
    x1 -= width >> 1;
    x2 -= width >> 1;
  }
  else
  {
    y1 -= width >> 1;
    y2 -= width >> 1;
  }
  xMax = LAYER_WIDTH - width - 1;
  yMax = LAYER_HEIGHT - width - 1;

  if (x1 < 0)
  {
    if (x2 < 0)
      return;
    y1 = (y1 - y2) * x2 / (x2 - x1) + y2;
    x1 = 0;
  }
  else if (x2 < 0)
  {
    y2 = (y2 - y1) * x1 / (x1 - x2) + y1;
    x2 = 0;
  }
  if (x1 > xMax)
  {
    if (x2 > xMax)
      return;
    y1 = (y1 - y2) * (x2 - xMax) / (x2 - x1) + y2;
    x1 = xMax;
  }
  else if (x2 > xMax)
  {
    y2 = (y2 - y1) * (x1 - xMax) / (x1 - x2) + y1;
    x2 = xMax;
  }
  if (y1 < 0)
  {
    if (y2 < 0)
      return;
    x1 = (x1 - x2) * y2 / (y2 - y1) + x2;
    y1 = 0;
  }
  else if (y2 < 0)
  {
    x2 = (x2 - x1) * y1 / (y1 - y2) + x1;
    y2 = 0;
  }
  if (y1 > yMax)
  {
    if (y2 > yMax)
      return;
    x1 = (x1 - x2) * (y2 - yMax) / (y2 - y1) + x2;
    y1 = yMax;
  }
  else if (y2 > yMax)
  {
    x2 = (x2 - x1) * (y1 - yMax) / (y1 - y2) + x1;
    y2 = yMax;
  }

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);

  if (lx < ly)
  {
    if (ly == 0)
      ly++;
    ax = ((x2 - x1) << 8) / ly;
    ay = ((y2 - y1) >> 8) | 1;
    x = x1 << 8;
    y = y1;
    for (i = ly; i > 0; i--, x += ax, y += ay)
    {
      ptr = y * pitch + (x >> 8);
      for (j = width; j > 0; j--, ptr++)
      {
        buf[ptr] = color;
      }
    }
  }
  else
  {
    if (lx == 0)
      lx++;
    ay = ((y2 - y1) << 8) / lx;
    ax = ((x2 - x1) >> 8) | 1;
    x = x1;
    y = y1 << 8;
    for (i = lx; i > 0; i--, x += ax, y += ay)
    {
      ptr = (y >> 8) * pitch + x;
      for (j = width; j > 0; j--, ptr += pitch)
      {
        buf[ptr] = color;
      }
    }
  }
}

void drawThickLine(int x1, int y1, int x2, int y2,
                   Canvas::Pixel color1, Canvas::Pixel color2, int width)
{
  int lx, ly, ax, ay, x, y, ptr, i, j;
  int xMax, yMax;
  int width1, width2;

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);
  if (lx < ly)
  {
    x1 -= width >> 1;
    x2 -= width >> 1;
  }
  else
  {
    y1 -= width >> 1;
    y2 -= width >> 1;
  }
  xMax = LAYER_WIDTH - width;
  yMax = LAYER_HEIGHT - width;

  if (x1 < 0)
  {
    if (x2 < 0)
      return;
    y1 = (y1 - y2) * x2 / (x2 - x1) + y2;
    x1 = 0;
  }
  else if (x2 < 0)
  {
    y2 = (y2 - y1) * x1 / (x1 - x2) + y1;
    x2 = 0;
  }
  if (x1 > xMax)
  {
    if (x2 > xMax)
      return;
    y1 = (y1 - y2) * (x2 - xMax) / (x2 - x1) + y2;
    x1 = xMax;
  }
  else if (x2 > xMax)
  {
    y2 = (y2 - y1) * (x1 - xMax) / (x1 - x2) + y1;
    x2 = xMax;
  }
  if (y1 < 0)
  {
    if (y2 < 0)
      return;
    x1 = (x1 - x2) * y2 / (y2 - y1) + x2;
    y1 = 0;
  }
  else if (y2 < 0)
  {
    x2 = (x2 - x1) * y1 / (y1 - y2) + x1;
    y2 = 0;
  }
  if (y1 > yMax)
  {
    if (y2 > yMax)
      return;
    x1 = (x1 - x2) * (y2 - yMax) / (y2 - y1) + x2;
    y1 = yMax;
  }
  else if (y2 > yMax)
  {
    x2 = (x2 - x1) * (y1 - yMax) / (y1 - y2) + x1;
    y2 = yMax;
  }

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);
  width1 = width - 2;

  if (lx < ly)
  {
    if (ly == 0)
      ly++;
    ax = ((x2 - x1) << 8) / ly;
    ay = ((y2 - y1) >> 8) | 1;
    x = x1 << 8;
    y = y1;
    ptr = y * pitch + (x >> 8) + 1;
    memset(&(buf[ptr]), color2, width1);
    x += ax;
    y += ay;
    for (i = ly - 1; i > 1; i--, x += ax, y += ay)
    {
      ptr = y * pitch + (x >> 8);
      buf[ptr] = color2;
      ptr++;
      memset(&(buf[ptr]), color1, width1);
      ptr += width1;
      buf[ptr] = color2;
    }
    ptr = y * pitch + (x >> 8) + 1;
    memset(&(buf[ptr]), color2, width1);
  }
  else
  {
    if (lx == 0)
      lx++;
    ay = ((y2 - y1) << 8) / lx;
    ax = ((x2 - x1) >> 8) | 1;
    x = x1;
    y = y1 << 8;
    ptr = ((y >> 8) + 1) * pitch + x;
    for (j = width1; j > 0; j--, ptr += pitch)
    {
      buf[ptr] = color2;
    }
    x += ax;
    y += ay;
    for (i = lx - 1; i > 1; i--, x += ax, y += ay)
    {
      ptr = (y >> 8) * pitch + x;
      buf[ptr] = color2;
      ptr += pitch;
      for (j = width1; j > 0; j--, ptr += pitch)
      {
        buf[ptr] = color1;
      }
      buf[ptr] = color2;
    }
    ptr = ((y >> 8) + 1) * pitch + x;
    for (j = width1; j > 0; j--, ptr += pitch)
    {
      buf[ptr] = color2;
    }
  }
}

void drawBox(int x, int y, int width, int height,
             Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf)
{
  int i, j;
  Canvas::Pixel cl;
  int ptr;

  x -= width >> 1;
  y -= height >> 1;
  if (x < 0)
  {
    width += x;
    x = 0;
  }
  if (x + width >= LAYER_WIDTH)
  {
    width = LAYER_WIDTH - x;
  }
  if (width <= 1)
    return;
  if (y < 0)
  {
    height += y;
    y = 0;
  }
  if (y + height > LAYER_HEIGHT)
  {
    height = LAYER_HEIGHT - y;
  }
  if (height <= 1)
    return;

  ptr = x + y * LAYER_WIDTH;
  memset(&(buf[ptr]), color2, width);
  y++;
  for (i = 0; i < height - 2; i++, y++)
  {
    ptr = x + y * LAYER_WIDTH;
    buf[ptr] = color2;
    ptr++;
    memset(&(buf[ptr]), color1, width - 2);
    ptr += width - 2;
    buf[ptr] = color2;
  }
  ptr = x + y * LAYER_WIDTH;
  memset(&(buf[ptr]), color2, width);
}

void drawBoxPanel(int x, int y, int width, int height,
                  Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf)
{
  int i, j;
  Canvas::Pixel cl;
  int ptr;

  x -= width >> 1;
  y -= height >> 1;
  if (x < 0)
  {
    width += x;
    x = 0;
  }
  if (x + width >= PANEL_WIDTH)
  {
    width = PANEL_WIDTH - x;
  }
  if (width <= 1)
    return;
  if (y < 0)
  {
    height += y;
    y = 0;
  }
  if (y + height > PANEL_HEIGHT)
  {
    height = PANEL_HEIGHT - y;
  }
  if (height <= 1)
    return;

  ptr = x + y * PANEL_WIDTH;
  memset(&(buf[ptr]), color2, width);
  y++;
  for (i = 0; i < height - 2; i++, y++)
  {
    ptr = x + y * PANEL_WIDTH;
    buf[ptr] = color2;
    ptr++;
    memset(&(buf[ptr]), color1, width - 2);
    ptr += width - 2;
    buf[ptr] = color2;
  }
  ptr = x + y * PANEL_WIDTH;
  memset(&(buf[ptr]), color2, width);
}

// Draw the numbers.
int drawNum(int n, int x, int y, int s, int c1, int c2)
{
  for (;;)
  {
    drawLetter(n % 10, x, y, s, 1, c1, c2, lpbuf);
    // y += (s*1.8f) / (float)SCREEN_DIVISOR;
    y += s; // (s*480) >> 9;
    n /= 10;
    if (n <= 0)
      break;
  }
  return y;
}

int drawNumRight(int n, int x, int y, int s, int c1, int c2)
{
  int d, nd, drawn = 0;
  for (d = 100000000; d > 0; d /= 10)
  {
    nd = (int)(n / d);
    if (nd > 0 || drawn)
    {
      n -= d * nd;
      drawLetter(nd % 10, x, y, s, 3, c1, c2, rpbuf);
      y += (s * 1.7f) / (float)SCREEN_DIVISOR;
      drawn = 1;
    }
  }
  if (!drawn)
  {
    drawLetter(0, x, y, s, 3, c1, c2, rpbuf);
    y += (s * 1.7f) / (float)SCREEN_DIVISOR;
  }
  return y;
}

int drawNumCenter(int n, int x, int y, int s, int c1, int c2)
{
  for (;;)
  {
    drawLetterBuf(n % 10, x, y, s, 2, c1, c2, buf, 0);
    x -= (s * 1.7f) / (float)SCREEN_DIVISOR;
    n /= 10;
    if (n <= 0)
      break;
  }
  return y;
}

#define JOYSTICK_AXIS 16384

int getPadState()
{
  int pad = 0;

  if (gamepad && gamepad->IsConnected())
  {
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Right))
    {
      if (gamepad->WasPressed(SRL::Input::Digital::Button::Right))
        SRL::Logger::LogTrace("[INPUT] Key pressed: RIGHT");
      pad |= PAD_RIGHT;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Left))
    {
      if (gamepad->WasPressed(SRL::Input::Digital::Button::Left))
        SRL::Logger::LogTrace("[INPUT] Key pressed: LEFT");
      pad |= PAD_LEFT;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Down))
    {
      if (gamepad->WasPressed(SRL::Input::Digital::Button::Down))
        SRL::Logger::LogTrace("[INPUT] Key pressed: DOWN");
      pad |= PAD_DOWN;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Up))
    {
      if (gamepad->WasPressed(SRL::Input::Digital::Button::Up))
        SRL::Logger::LogTrace("[INPUT] Key pressed: UP");
      pad |= PAD_UP;
    }
  }

  return pad;
}

int buttonReversed = 0;

int getButtonState()
{
  int btn = 0;
  int fireBtn1 = 0, fireBtn2 = 0, slowBtn1 = 0, slowBtn2 = 0;
  if (gamepad->IsConnected())
  {
    fireBtn1 = gamepad->IsHeld(Digital::Button::A);
    if (gamepad->WasPressed(Digital::Button::A))
      SRL::Logger::LogTrace("[INPUT] Button pressed: A (fire)");
    fireBtn2 = gamepad->IsHeld(Digital::Button::B);
    if (gamepad->WasPressed(Digital::Button::B))
      SRL::Logger::LogTrace("[INPUT] Button pressed: B (fire)");

    slowBtn1 = gamepad->IsHeld(Digital::Button::R);
    if (gamepad->WasPressed(Digital::Button::R))
      SRL::Logger::LogTrace("[INPUT] Button pressed: R (slow)");
    slowBtn2 = gamepad->IsHeld(Digital::Button::L);
    if (gamepad->WasPressed(Digital::Button::L))
      SRL::Logger::LogTrace("[INPUT] Button pressed: L (slow)");
  }
  if (fireBtn1 || fireBtn2)
  {
    if (!buttonReversed)
    {
      btn |= PAD_BUTTON1;
    }
    else
    {
      btn |= PAD_BUTTON2;
    }
  }
  if (slowBtn1 || slowBtn2)
  {
    if (!buttonReversed)
    {
      btn |= PAD_BUTTON2;
    }
    else
    {
      btn |= PAD_BUTTON1;
    }
  }
  return btn;
}
