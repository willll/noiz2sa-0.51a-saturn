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
#include <srl_memory.hpp> // for LowWorkRam frame buffer allocation
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

static Canvas::Pixel **smokeBuf = nullptr;
// Frame buffers allocated from LWRAM (0x00200000, 1MB) to preserve main RAM heap for BLB parsers
static Canvas::Pixel *pbuf  = nullptr;
Canvas::Pixel *l1buf = nullptr;
Canvas::Pixel *l2buf = nullptr;
Canvas::Pixel *buf   = nullptr;
Canvas::Pixel *lpbuf = nullptr;
Canvas::Pixel *rpbuf = nullptr;
static Canvas::Pixel smokeFallback = 0;
static SDL_Rect layerRect, layerClearRect;
static SDL_Rect lpanelRect, rpanelRect, panelClearRect;
static void *panelLayerVram = nullptr;
static int panelLayerVramSize = 0;
static constexpr int PANEL_LAYER_WIDTH = 512;
static constexpr int PANEL_LAYER_HEIGHT = 256;
static constexpr int PANEL_LAYER_RIGHT_X = SCREEN_WIDTH - PANEL_WIDTH;
static constexpr int PANEL_LAYER_PLAYFIELD_X = (SCREEN_WIDTH - LAYER_WIDTH) / 2;

struct PendingLineCommand
{
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  Canvas::Pixel color;
};

static constexpr int MAX_PENDING_LINE_COMMANDS = 512;
static PendingLineCommand pendingLineCommands[MAX_PENDING_LINE_COMMANDS];
static int pendingLineCommandCount = 0;

// Handle TGA images in ISO 8:3 format
#define SPRITE_NUM 7

static SRL_Surface sprite[SPRITE_NUM];
static Canvas::Pixel titleSprite[SPRITE_NUM][20 * 20];
static const char *spriteFile[SPRITE_NUM] = {
    "TITLEN.TGA",
    "TITLEO.TGA",
    "TITLEI.TGA",
    "TITLEZ.TGA",
    "TITLE2.TGA",
    "TITLES.TGA",
    "TITLEA.TGA",
};

static int32_t loadSpriteTextureRGB555(SRL::Bitmap::BitmapInfo &info, const uint8_t *srcPixels)
{
  if (srcPixels == nullptr || info.Width == 0 || info.Height == 0)
  {
    return -1;
  }

  int32_t textureIndex = SRL::VDP1::TryAllocateTexture(
      info.Width,
      info.Height,
      SRL::CRAM::TextureColorMode::RGB555,
      0);
  if (textureIndex < 0)
  {
    return -1;
  }

  uint16_t *dstPixels = (uint16_t *)SRL::VDP1::Textures[textureIndex].GetData();
  if (dstPixels == nullptr)
  {
    return -1;
  }

  const uint32_t pixelCount = (uint32_t)info.Width * (uint32_t)info.Height;

  if (info.Palette != nullptr && info.Palette->Colors != nullptr)
  {
    for (uint32_t p = 0; p < pixelCount; p++)
    {
      uint8_t idx = 0;
      if (info.ColorMode == SRL::CRAM::TextureColorMode::Paletted16)
      {
        const uint8_t packed = srcPixels[p >> 1];
        idx = ((p & 1u) == 0u) ? (uint8_t)((packed >> 4) & 0x0f) : (uint8_t)(packed & 0x0f);
      }
      else
      {
        idx = srcPixels[p];
      }

      if (idx >= info.Palette->Count)
      {
        dstPixels[p] = 0;
      }
      else
      {
        dstPixels[p] = (uint16_t)info.Palette->Colors[idx];
      }
    }
  }
  else
  {
    const uint16_t *srcRgb = (const uint16_t *)srcPixels;
    for (uint32_t p = 0; p < pixelCount; p++)
    {
      dstPixels[p] = srcRgb[p];
    }
  }

  return textureIndex;
}

static void queueHardwareLineCommand(int x1, int y1, int x2, int y2, Canvas::Pixel lineColor)
{
  if (pendingLineCommandCount >= MAX_PENDING_LINE_COMMANDS)
  {
    return;
  }

  pendingLineCommands[pendingLineCommandCount].x1 = (int16_t)x1;
  pendingLineCommands[pendingLineCommandCount].y1 = (int16_t)y1;
  pendingLineCommands[pendingLineCommandCount].x2 = (int16_t)x2;
  pendingLineCommands[pendingLineCommandCount].y2 = (int16_t)y2;
  pendingLineCommands[pendingLineCommandCount].color = lineColor;
  pendingLineCommandCount++;
}

static void flushHardwareLineCommands()
{
  const int16_t lineSort = 500;

  for (int i = 0; i < pendingLineCommandCount; i++)
  {
    const PendingLineCommand &line = pendingLineCommands[i];
    const int16_t sceneX1 = (int16_t)(layerRect.x + line.x1 - (SCREEN_WIDTH / 2));
    const int16_t sceneY1 = (int16_t)(layerRect.y + line.y1 - (SCREEN_HEIGHT / 2));
    const int16_t sceneX2 = (int16_t)(layerRect.x + line.x2 - (SCREEN_WIDTH / 2));
    const int16_t sceneY2 = (int16_t)(layerRect.y + line.y2 - (SCREEN_HEIGHT / 2));

    SRL::Scene2D::DrawLine(
        Vector2D(Fxp::Convert(sceneX1), Fxp::Convert(sceneY1)),
        Vector2D(Fxp::Convert(sceneX2), Fxp::Convert(sceneY2)),
        color[line.color],
        Fxp::Convert(lineSort));
  }

  pendingLineCommandCount = 0;
}

static void uploadPanelBitmapRegion(const Canvas::Pixel *src, const SDL_Rect &dirtyRect, int dstX)
{
  if (panelLayerVram == nullptr || src == nullptr || dirtyRect.w == 0 || dirtyRect.h == 0)
  {
    return;
  }

  const uint8_t *srcRow = src + dirtyRect.x + (dirtyRect.y * PANEL_WIDTH);
  uint8_t *dstRow = (uint8_t *)panelLayerVram + dstX + dirtyRect.x + (dirtyRect.y * PANEL_LAYER_WIDTH);

  for (int row = 0; row < dirtyRect.h; row++)
  {
    slDMACopy((void *)srcRow, dstRow, dirtyRect.w);
    srcRow += PANEL_WIDTH;
    dstRow += PANEL_LAYER_WIDTH;
  }
  slDMAWait();
}

static void uploadPlayfieldBitmapRegion(const Canvas::Pixel *src, const SDL_Rect &dirtyRect)
{
  if (panelLayerVram == nullptr || src == nullptr || dirtyRect.w == 0 || dirtyRect.h == 0)
  {
    return;
  }

  const uint8_t *srcRow = src + dirtyRect.x + (dirtyRect.y * LAYER_WIDTH);
  uint8_t *dstRow = (uint8_t *)panelLayerVram + PANEL_LAYER_PLAYFIELD_X + dirtyRect.x + (dirtyRect.y * PANEL_LAYER_WIDTH);

  for (int row = 0; row < dirtyRect.h; row++)
  {
    slDMACopy((void *)srcRow, dstRow, dirtyRect.w);
    srcRow += LAYER_WIDTH;
    dstRow += PANEL_LAYER_WIDTH;
  }
  slDMAWait();
}

static void refreshPanelLayer()
{
  if (panelLayerVram == nullptr)
  {
    return;
  }

  if (layer->dirty)
  {
    SDL_Rect dirtyRect;
    dirtyRect.x = layer->dirtyX1;
    dirtyRect.y = layer->dirtyY1;
    dirtyRect.w = (uint16_t)(layer->dirtyX2 - layer->dirtyX1);
    dirtyRect.h = (uint16_t)(layer->dirtyY2 - layer->dirtyY1);
    uploadPlayfieldBitmapRegion(buf, dirtyRect);
  }

  if (lpanel->dirty)
  {
    SDL_Rect dirtyRect;
    dirtyRect.x = lpanel->dirtyX1;
    dirtyRect.y = lpanel->dirtyY1;
    dirtyRect.w = (uint16_t)(lpanel->dirtyX2 - lpanel->dirtyX1);
    dirtyRect.h = (uint16_t)(lpanel->dirtyY2 - lpanel->dirtyY1);
    uploadPanelBitmapRegion(lpbuf, dirtyRect, 0);
    SDL_ClearDirtyRect(lpanel);
  }

  if (rpanel->dirty)
  {
    SDL_Rect dirtyRect;
    dirtyRect.x = rpanel->dirtyX1;
    dirtyRect.y = rpanel->dirtyY1;
    dirtyRect.w = (uint16_t)(rpanel->dirtyX2 - rpanel->dirtyX1);
    dirtyRect.h = (uint16_t)(rpanel->dirtyY2 - rpanel->dirtyY1);
    uploadPanelBitmapRegion(rpbuf, dirtyRect, PANEL_LAYER_RIGHT_X);
    SDL_ClearDirtyRect(rpanel);
  }
}

static void initPanelLayer(const int16_t paletteBank)
{
  SRL::Bitmap::BitmapInfo panelInfo(PANEL_LAYER_WIDTH, PANEL_LAYER_HEIGHT);
  panelInfo.ColorMode = SRL::CRAM::TextureColorMode::Paletted256;
  Vector2D panelPosition(0.0, 0.0);

  panelLayerVram = SRL::VDP2::VRAM::Allocate(
      PANEL_LAYER_WIDTH * PANEL_LAYER_HEIGHT,
      32,
      SRL::VDP2::VramBank::A0,
      2);
  panelLayerVramSize = PANEL_LAYER_WIDTH * PANEL_LAYER_HEIGHT;
  if (panelLayerVram == nullptr)
  {
    SRL::Logger::LogFatal("[SCREEN] Failed to allocate VDP2 panel layer");
    SRL::System::Exit(1);
  }

  SRL::VDP2::NBG1::SetCellAddress(panelLayerVram, panelLayerVramSize);
  SRL::VDP2::NBG1::TilePalette = SRL::CRAM::Palette(SRL::CRAM::TextureColorMode::Paletted256, (uint16_t)paletteBank);
  SRL::VDP2::VRAM::Blank(panelLayerVram, panelLayerVramSize);
  SRL::VDP2::NBG1::Init(panelInfo);
  SRL::VDP2::NBG1::SetPosition(panelPosition);
  SRL::VDP2::NBG1::TransparentEnable();
  SRL::VDP2::NBG1::SetPriority(SRL::VDP2::Priority::Layer6);
  SRL::VDP2::NBG1::ScrollEnable();
}

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
    char step[64];
    snprintf(step, sizeof(step), "Loading sprite %d/%d", i + 1, SPRITE_NUM);
    updateLoadingProgress(step, 10 + ((i + 1) * 14) / SPRITE_NUM);

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

    // Always convert title TGAs to RGB555 so each image keeps its own palette colors.
    textureIndex = loadSpriteTextureRGB555(info, (const uint8_t *)tga->GetData());

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

    // Cache a GP32-compatible 20x20 software sprite before releasing TGA data.
    const uint8_t *srcPixels = reinterpret_cast<const uint8_t *>(tga->GetData());
    if (srcPixels != nullptr && info.Width >= 40 && info.Height >= 40)
    {
      for (int sy = 0; sy < 20; sy++)
      {
        for (int sx = 0; sx < 20; sx++)
        {
          const int srcX = sx * 2;
          const int srcY = sy * 2;
          titleSprite[i][sy * 20 + sx] = srcPixels[srcY * info.Width + srcX];
        }
      }
    }
    else
    {
      memset(titleSprite[i], 0, sizeof(titleSprite[i]));
    }

    //SRL::Logger::LogTrace("[SPRITE] TGA object created for: %s", spriteFile[i]);

    sprite[i] = SRL_Surface(textureIndex, info.Width, info.Height);
    SRL::Logger::LogTrace("[SPRITE] SDL surface created for sprite %d", i);

    delete tga;

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
  if (n >= SPRITE_NUM)
  {
    return;
  }

  // Software fallback so title sprites remain visible even if emulator VDP1 composition is incomplete.
  if (status == TITLE)
  {
    for (int sy = 0; sy < 20; sy++)
    {
      const int dstY = y + sy;
      if (dstY < 0 || dstY >= LAYER_HEIGHT)
      {
        continue;
      }

      for (int sx = 0; sx < 20; sx++)
      {
        const int dstX = x + sx;
        if (dstX < 0 || dstX >= LAYER_WIDTH)
        {
          continue;
        }

        const Canvas::Pixel pixel = titleSprite[n][sy * 20 + sx];
        if (pixel != 0)
        {
          buf[dstY * pitch + dstX] = pixel;
        }
      }
    }

    SDL_SetSurfaceDirty(layer);
  }

  // Draw directly from the loaded 40x40 title texture and scale to GP32-like 20x20.
  const int16_t screenX = (int16_t)(layerRect.x + x - (SCREEN_WIDTH / 2));
  const int16_t screenY = (int16_t)(layerRect.y + y - (SCREEN_HEIGHT / 2));

  const Vector3D pos(
      Fxp::Convert(screenX),
      Fxp::Convert(screenY),
      100.0);

    const int32_t previousEndCode = SRL::Scene2D::GetEffect(SRL::Scene2D::SpriteEffect::EnableECD);
    SRL::Scene2D::SetEffect(SRL::Scene2D::SpriteEffect::EnableECD, 0);

  SRL::Scene2D::DrawSprite(
      (uint16_t)sprite[n].textureIndex,
      pos,
      Angle::Zero(),
      Vector2D(0.5, 0.5),
      SRL::Scene2D::ZoomPoint::UpperLeft);

    SRL::Scene2D::SetEffect(SRL::Scene2D::SpriteEffect::EnableECD, previousEndCode == 1 ? 1 : 0);
}

static void makeSmokeBuf()
{
  int x, y, mx, my;

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

  // Allocate frame buffers from LWRAM (1MB at 0x00200000), preserving main RAM heap for BLB parsers
  const size_t lwrFreeBefore = SRL::Memory::LowWorkRam::GetFreeSpace();
  SRL::Logger::LogInfo("[SCREEN] LWRAM free before frame buffers: %lu", (unsigned long)lwrFreeBefore);

  pbuf  = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(lyrSize);
  l1buf = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(lyrSize);
  l2buf = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(lyrSize);
  buf   = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(lyrSize);
  lpbuf = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(PANEL_WIDTH * PANEL_HEIGHT);
  rpbuf = (Canvas::Pixel *)SRL::Memory::LowWorkRam::Malloc(PANEL_WIDTH * PANEL_HEIGHT);
  if (!pbuf || !l1buf || !l2buf || !buf || !lpbuf || !rpbuf)
  {
    SRL::Logger::LogWarning(
        "[SCREEN] LWRAM alloc fallback: p=%p l1=%p l2=%p b=%p lp=%p rp=%p freeAfter=%lu",
        pbuf,
        l1buf,
        l2buf,
        buf,
        lpbuf,
        rpbuf,
        (unsigned long)SRL::Memory::LowWorkRam::GetFreeSpace());

    if (!pbuf)  pbuf  = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(lyrSize);
    if (!l1buf) l1buf = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(lyrSize);
    if (!l2buf) l2buf = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(lyrSize);
    if (!buf)   buf   = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(lyrSize);
    if (!lpbuf) lpbuf = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(PANEL_WIDTH * PANEL_HEIGHT);
    if (!rpbuf) rpbuf = (Canvas::Pixel *)SRL::Memory::HighWorkRam::Malloc(PANEL_WIDTH * PANEL_HEIGHT);
  }

  if (!pbuf || !l1buf || !l2buf || !buf || !lpbuf || !rpbuf)
  {
    SRL::Logger::LogFatal("[SCREEN] Failed to allocate frame buffers (LWRAM+HWRAM)");
    SRL::System::Exit(1);
  }

  memset(pbuf,  0, lyrSize);
  memset(l1buf, 0, lyrSize);
  memset(l2buf, 0, lyrSize);
  memset(buf,   0, lyrSize);
  memset(lpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);
  memset(rpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);

  // Bind SDL surface wrappers to software pixel buffers for SDL_BlitSurface uploads.
  layer->pixels = buf;
  lpanel->pixels = lpbuf;
  rpanel->pixels = rpbuf;
  video->pixels = nullptr;
  // Keep software surfaces paletted so VRAM uploads can use DMA directly.
  layer->preferRGB555 = false;
  lpanel->preferRGB555 = false;
  rpanel->preferRGB555 = false;
  SDL_SetSurfaceDirty(layer);
  SDL_SetSurfaceDirty(lpanel);
  SDL_SetSurfaceDirty(rpanel);

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

  const int32_t mainPaletteBank = Palette::initPalette();
  SDL_SetBlitPaletteBank((int16_t)mainPaletteBank);

  // The software gameplay layer and title sprites are drawn through VDP1.
  // Raise the default RGB sprite bank above NBG0 so center-layer UI is not hidden by the background.
  SRL::VDP2::SpriteLayer::SetPriority(SRL::VDP2::Priority::Layer7);

  initPanelLayer((int16_t)mainPaletteBank);
  makeSmokeBuf();
  clearLPanel();
  clearRPanel();
  refreshPanelLayer();

  loadSprites();
}

void blendScreen()
{
  SDL_CopyBytes(buf, l2buf, lyrSize);

  for (int i = lyrSize - 1; i >= 0; i--)
  {
    if (l1buf[i] == 0)
    {
      continue;
    }

    buf[i] = (buf[i] == 0) ? l1buf[i] : colorAlp[l1buf[i]][buf[i]];
  }

  SDL_SetSurfaceDirty(layer);
}

void markPlayfieldDirty()
{
  SDL_SetSurfaceDirty(layer);
}

void flipScreen()
{
  static uint32_t flipCounter = 0;

  refreshPanelLayer();

  uint32_t blitStartUs = SDL_GetProfileMicros();
  SDL_BlitSurface(layer, nullptr, video, &layerRect);
  uint32_t timeLayerUs = SDL_GetProfileMicros() - blitStartUs;

  const uint32_t hwLineStartUs = SDL_GetProfileMicros();
  flushHardwareLineCommands();
  const uint32_t timeHwLineUs = SDL_GetProfileMicros() - hwLineStartUs;
  
  SDL_Flip(video);

  flipCounter++;
  if ((flipCounter % 60u) == 0u)
  {
    uint32_t calls = 0;
    uint32_t uploads = 0;
    uint32_t uploadPixels = 0;
    uint32_t uploadUs = 0;
    uint32_t drawUs = 0;
    SDL_GetBlitStats(&calls, &uploads, &uploadPixels, &uploadUs, &drawUs);
    SRL::Logger::LogWarning(
      "[BLIT_US] frame=%lu layer=%lu lines=%lu panel_dma=%lu | calls=%lu uploads=%lu up=%lu draw=%lu",
        (unsigned long)flipCounter,
      (unsigned long)timeLayerUs,
      (unsigned long)timeHwLineUs,
      (unsigned long)0,
        (unsigned long)calls,
        (unsigned long)uploads,
      (unsigned long)uploadUs,
      (unsigned long)drawUs);
    SDL_ResetBlitStats();
  }

}

void clearScreen()
{
  memset(buf, 0, LAYER_WIDTH * LAYER_HEIGHT);
  SDL_SetSurfaceDirty(layer);
}

void clearLPanel()
{
  memset(lpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);
  SDL_SetSurfaceDirty(lpanel);
}

void clearRPanel()
{
  memset(rpbuf, 0, PANEL_WIDTH * PANEL_HEIGHT);
  SDL_SetSurfaceDirty(rpanel);
}

void smokeScreen()
{
  // The previous CPU decay pass was immediately cleared before drawing each frame.
  // Keep smoke disabled here and rely on hardware compositing plus palette fades instead.
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

  // Fast Saturn path: bullet wake lines are safe to emit in active gameplay only.
  if (width == 1 && buf == l1buf && status == IN_GAME)
  {
    queueHardwareLineCommand(x1, y1, x2, y2, color);
    return;
  }

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
  if (x < 0) { width += x; x = 0; }
  if (x + width >= LAYER_WIDTH) { width = LAYER_WIDTH - x; }
  if (width <= 1) return;
  if (y < 0) { height += y; y = 0; }
  if (y + height > LAYER_HEIGHT) { height = LAYER_HEIGHT - y; }
  if (height <= 1) return;

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
  if (x < 0) { width += x; x = 0; }
  if (x + width >= PANEL_WIDTH) { width = PANEL_WIDTH - x; }
  if (width <= 1) return;
  if (y < 0) { height += y; y = 0; }
  if (y + height > PANEL_HEIGHT) { height = PANEL_HEIGHT - y; }
  if (height <= 1) return;

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

  if (buf == lpbuf)
  {
    SDL_MarkDirtyRect(lpanel, x, y - height + 1, width, height);
  }
  else if (buf == rpbuf)
  {
    SDL_MarkDirtyRect(rpanel, x, y - height + 1, width, height);
  }
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

int getPadState()
{
  int pad = 0;

  if (gamepad && gamepad->IsConnected())
  {
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Right))
      pad |= PAD_RIGHT;
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Left))
      pad |= PAD_LEFT;
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Down))
      pad |= PAD_DOWN;
    if (gamepad->IsHeld(SRL::Input::Digital::Button::Up))
      pad |= PAD_UP;
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
