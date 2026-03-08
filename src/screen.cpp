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
#include "palette.h"

using namespace SRL::Math::Types;

int brightness = DEFAULT_BRIGHTNESS;

static SRL_Surface *video, *layer, *lpanel, *rpanel;
static LayerBit **smokeBuf;
static LayerBit *pbuf;
LayerBit *l1buf, *l2buf;
LayerBit *buf;
LayerBit *lpbuf, *rpbuf;
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
    SRL::Logger::LogDebug("[SPRITE] Loading sprite %d: %s", i, spriteFile[i]);

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
    SRL::Logger::LogTrace("[SPRITE] TGA info: %dx%d pixels", info.Width, info.Height);

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
      textureIndex = SRL::VDP1::TryLoadTexture(tga, MyPalette::LoadPalette);
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
      SRL::Logger::LogTrace("[SPRITE] TGA loaded into VDP1 with texture index: %d", textureIndex);
    }

    SRL::Logger::LogTrace("[SPRITE] TGA object created for: %s", spriteFile[i]);

    // Copy TGA image data to SDL surface
    // if ( img->pixels != nullptr && tga->GetData() != nullptr ) {
    //   memcpy(img->pixels, tga->GetData(), info.Width * info.Height);
    //   SRL::Logger::LogTrace("[SPRITE] Image data copied to SDL surface (%d bytes)", info.Width * info.Height);
    // }

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

  // color[0].Red = color[0].Green = color[0].Blue = 31;
  // SDL_SetColors(video, color, 0, 1);

  SRL::Logger::LogInfo("[SPRITE] Sprite loading complete: %d sprites loaded", SPRITE_NUM);
}

void drawSprite(const uint8_t n, const int16_t x, const int16_t y)
{
  SRL::Math::Types::Vector3D pos(Fxp::Convert(x), Fxp::Convert(y), 500.0);
  SRL::Scene2D::DrawSprite(sprite[n].textureIndex, pos);
}

// Initialize palletes.
// static void initPalette()
// {
//   int i;
//   for (i = 0; i < 256; i++)
//   {
//     color[i].Red = color[i].Red * brightness / 256;
//     color[i].Green = color[i].Green * brightness / 256;
//     color[i].Blue = color[i].Blue * brightness / 256;
//   }
//   SDL_SetColors(video, color, 0, 256);
//   SDL_SetColors(layer, color, 0, 256);
//   SDL_SetColors(lpanel, color, 0, 256);
//   SDL_SetColors(rpanel, color, 0, 256);
// }

static int lyrSize;

static void makeSmokeBuf()
{
  int x, y, mx, my;
  lyrSize = sizeof(LayerBit) * pitch * LAYER_HEIGHT;
  if (nullptr == (smokeBuf = (LayerBit **)malloc(sizeof(LayerBit *) * pitch * LAYER_HEIGHT)))
  {
    SRL::Logger::LogFatal("Couldn't malloc smokeBuf.");
    SRL::System::Exit(1);
  }
  if (nullptr == (pbuf = (LayerBit *)malloc(lyrSize + sizeof(LayerBit))) ||
      nullptr == (l1buf = (LayerBit *)malloc(lyrSize + sizeof(LayerBit))) ||
      nullptr == (l2buf = (LayerBit *)malloc(lyrSize + sizeof(LayerBit))))
  {
    SRL::Logger::LogFatal("Couldn't malloc buffer.");
    SRL::System::Exit(1);
  }
  pbuf[pitch * LAYER_HEIGHT] = 0;
  for (y = 0; y < LAYER_HEIGHT; y++)
  {
    for (x = 0; x < LAYER_WIDTH; x++)
    {
      mx = x + sctbl[(x * 8) & (DIV - 1)] / 128;
      my = y + sctbl[(y * 8) & (DIV - 1)] / 128;
      if (mx < 0 || mx >= LAYER_WIDTH || my < 0 || my >= LAYER_HEIGHT)
      {
        smokeBuf[x + y * pitch] = &(pbuf[pitch * LAYER_HEIGHT]);
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
  // if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
  //   SRL::Logger::LogFatal("Unable to initialize SDL: %s", SDL_GetError());
  //   SRL::System::Exit(1);
  // }
  // else {
  //   SRL::Logger::LogInfo("SDL initialized successfully");
  // }

  // if ( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) {
  //   SRL::Logger::LogWarning("Unable to initialize SDL_JOYSTICK");
  //   joystickMode = 0;
  // }

  // if ( (video = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP,
  //                                SDL_DOUBLEBUF | SDL_HWSURFACE | SDL_HWPALETTE | SDL_FULLSCREEN)) == nullptr ) {
  //   SRL::Logger::LogFatal("Unable to create SDL screen: %s", SDL_GetError());
  //   SRL::System::Exit(1);
  // }
  // pfrm = video->format;
  // if ( nullptr == ( layer = SDL_CreateRGBSurface
  // 	(SDL_SWSURFACE, LAYER_WIDTH, LAYER_HEIGHT, videoBpp,
  // 	 pfrm->Rmask, pfrm->Gmask, pfrm->Bmask, pfrm->Amask)) ||
  //      nullptr == ( lpanel = SDL_CreateRGBSurface
  // 	(SDL_SWSURFACE, PANEL_WIDTH, PANEL_HEIGHT, videoBpp,
  // 	 pfrm->Rmask, pfrm->Gmask, pfrm->Bmask, pfrm->Amask)) ||
  //      nullptr == ( rpanel = SDL_CreateRGBSurface
  // 	(SDL_SWSURFACE, PANEL_WIDTH, PANEL_HEIGHT, videoBpp,
  // 	 pfrm->Rmask, pfrm->Gmask, pfrm->Bmask, pfrm->Amask)) ) {
  //       SRL::Logger::LogFatal("Couldn't create surface: %s", SDL_GetError());
  //     SRL::System::Exit(1);
  // }
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

  // pitch = layer->pitch/(videoBpp/8);
  // buf = (LayerBit*)layer->pixels;
  // ppitch = lpanel->pitch/(videoBpp/8);
  // lpbuf = (LayerBit*)lpanel->pixels;
  // rpbuf = (LayerBit*)rpanel->pixels;

  MyPalette::initPalette();
  // makeSmokeBuf();
  // clearLPanel();
  // clearRPanel();

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
  SDL_FillRect(layer, &layerClearRect, 0);
}

void clearLPanel()
{
  SDL_FillRect(lpanel, &panelClearRect, 0);
}

void clearRPanel()
{
  SDL_FillRect(rpanel, &panelClearRect, 0);
}

void smokeScreen()
{
  int i;
  memcpy(pbuf, l2buf, lyrSize);
  for (i = lyrSize - 1; i >= 0; i--)
  {
    l1buf[i] = colorDfs[l1buf[i]];
    l2buf[i] = colorDfs[*(smokeBuf[i])];
  }
}

void drawLine(int x1, int y1, int x2, int y2, LayerBit color, int width, LayerBit *buf)
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
                   LayerBit color1, LayerBit color2, int width)
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
             LayerBit color1, LayerBit color2, LayerBit *buf)
{
  int i, j;
  LayerBit cl;
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
                  LayerBit color1, LayerBit color2, LayerBit *buf)
{
  int i, j;
  LayerBit cl;
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
      pad |= PAD_RIGHT;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Left))
    {
      pad |= PAD_LEFT;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Down))
    {
      pad |= PAD_DOWN;
    }
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Up))
    {
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
    fireBtn2 = gamepad->IsHeld(Digital::Button::B);

    slowBtn1 = gamepad->IsHeld(Digital::Button::R);
    slowBtn2 = gamepad->IsHeld(Digital::Button::L);
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
