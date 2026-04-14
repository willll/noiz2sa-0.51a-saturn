/*
 * $Id: background.c,v 1.2 2002/12/31 09:34:34 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Handle background graphics.
 *
 * @version $Revision: 1.2 $
 */
#include "SDL.h"
#include <srl.hpp>
#include <srl_memory.hpp> // for memory allocation
#include <srl_log.hpp>    // for logging
#include <srl_system.hpp> // for exit

#include "noiz2sa.h"
#include "screen.h"
#include "clrtbl.h"
#include "vector.h"
#include "background.h"

static Board board[BOARD_MAX];
static void *backgroundLayerVram = nullptr;
static int backgroundLayerVramSize = 0;
static uint16_t *backgroundBuffers[2] = { nullptr, nullptr };
static uint32_t backgroundUploadedGeneration = 0;
static uint32_t backgroundRequestedGeneration = 0;
static bool backgroundUploadLogged = false;
static constexpr int BACKGROUND_BITMAP_WIDTH = 512;
static constexpr int BACKGROUND_BITMAP_HEIGHT = 256;
static constexpr int BACKGROUND_SCREEN_X = (SCREEN_WIDTH - LAYER_WIDTH) / 2;

static int bdIdx;
static int boardMx, boardMy;
static int boardRepx, boardRepy;
static int boardRepXn, boardRepYn;
static constexpr uint16_t BACKGROUND_BASE_COLOR = 0xffff;
static constexpr uint32_t BACKGROUND_HASH_OFFSET = 2166136261u;
static constexpr uint32_t BACKGROUND_HASH_PRIME = 16777619u;

static inline uint32_t hashBackgroundByte(uint32_t hash, uint8_t value)
{
  return (hash ^ value) * BACKGROUND_HASH_PRIME;
}

static void fillBackgroundSpan(uint16_t *dst, int count, uint16_t color)
{
  for (int i = 0; i < count; i++)
  {
    dst[i] = color;
  }
}

static void rasterBackgroundRect(
    uint16_t *dst,
    int centerX,
    int centerY,
    int width,
    int height,
    const uint16_t fillColor,
    const uint16_t borderColor)
{
  int x = centerX - (width >> 1);
  int y = centerY - (height >> 1);
  if (x < 0) { width += x; x = 0; }
  if (x + width > LAYER_WIDTH) { width = LAYER_WIDTH - x; }
  if (width <= 1) return;
  if (y < 0) { height += y; y = 0; }
  if (y + height > LAYER_HEIGHT) { height = LAYER_HEIGHT - y; }
  if (height <= 1) return;

  int ptr = x + y * LAYER_WIDTH;
  fillBackgroundSpan(&(dst[ptr]), width, borderColor);
  y++;
  for (int row = 0; row < height - 2; row++, y++)
  {
    ptr = x + y * LAYER_WIDTH;
    dst[ptr] = borderColor;
    ptr++;
    fillBackgroundSpan(&(dst[ptr]), width - 2, fillColor);
    ptr += width - 2;
    dst[ptr] = borderColor;
  }
  ptr = x + y * LAYER_WIDTH;
  fillBackgroundSpan(&(dst[ptr]), width, borderColor);
}

static void renderBackgroundBitmap(
    const Board *srcBoard,
    int srcBoardRepx,
    int srcBoardRepy,
    int srcBoardRepXn,
    int srcBoardRepYn,
    uint16_t *dst)
{
  for (int i = 0; i < LAYER_WIDTH * LAYER_HEIGHT; i++)
  {
    dst[i] = BACKGROUND_BASE_COLOR;
  }

  const int osx = -srcBoardRepx * (srcBoardRepXn / 2);
  const int osy = -srcBoardRepy * (srcBoardRepYn / 2);

  for (int i = 0; i < BOARD_MAX; i++)
  {
    if (srcBoard[i].width == NOT_EXIST)
    {
      continue;
    }

    const Board *bd = &(srcBoard[i]);
    int ox = osx;
    for (int rx = 0; rx < srcBoardRepXn; rx++, ox += srcBoardRepx)
    {
      int oy = osy;
      for (int ry = 0; ry < srcBoardRepYn; ry++, oy += srcBoardRepy)
      {
        rasterBackgroundRect(
            dst,
            (bd->x + ox) / bd->z + LAYER_WIDTH / 2,
            (bd->y + oy) / bd->z + LAYER_HEIGHT / 2,
            bd->width,
            bd->height,
            (uint16_t)color[1],
            (uint16_t)color[3]);
      }
    }
  }
}

class BackgroundRenderTask : public SRL::Types::ITask
{
public:
  void Prepare(
      const Board *srcBoard,
      int srcBoardRepx,
      int srcBoardRepy,
      int srcBoardRepXn,
      int srcBoardRepYn,
      uint16_t *dst,
      uint32_t generation,
      int bufferIndex)
  {
    memcpy(snapshotBoard, srcBoard, sizeof(snapshotBoard));
    snapshotBoardRepx = srcBoardRepx;
    snapshotBoardRepy = srcBoardRepy;
    snapshotBoardRepXn = srcBoardRepXn;
    snapshotBoardRepYn = srcBoardRepYn;
    targetBuffer = dst;
    targetGeneration = generation;
    targetBufferIndex = bufferIndex;
  }

  uint32_t GetGeneration() const
  {
    return targetGeneration;
  }

  int GetBufferIndex() const
  {
    return targetBufferIndex;
  }

protected:
  void Do() override
  {
    if (targetBuffer == nullptr)
    {
      return;
    }

    renderBackgroundBitmap(
        snapshotBoard,
        snapshotBoardRepx,
        snapshotBoardRepy,
        snapshotBoardRepXn,
        snapshotBoardRepYn,
        targetBuffer);
  }

private:
  Board snapshotBoard[BOARD_MAX];
  int snapshotBoardRepx = 0;
  int snapshotBoardRepy = 0;
  int snapshotBoardRepXn = 0;
  int snapshotBoardRepYn = 0;
  uint16_t *targetBuffer = nullptr;
  uint32_t targetGeneration = 0;
  int targetBufferIndex = 0;
};

static BackgroundRenderTask backgroundRenderTask;

static void uploadBackgroundBitmap(const uint16_t *src)
{
  if (backgroundLayerVram == nullptr || src == nullptr)
  {
    return;
  }

  const uint8_t *srcRow = (const uint8_t *)src;
  uint8_t *dstRow = (uint8_t *)backgroundLayerVram + (BACKGROUND_SCREEN_X * (int)sizeof(uint16_t));
  for (int row = 0; row < LAYER_HEIGHT; row++)
  {
    slDMACopy((void *)srcRow, dstRow, LAYER_WIDTH * sizeof(uint16_t));
    srcRow += LAYER_WIDTH * sizeof(uint16_t);
    dstRow += BACKGROUND_BITMAP_WIDTH * sizeof(uint16_t);
  }
  slDMAWait();

  if (!backgroundUploadLogged)
  {
    SRL::Logger::LogInfo(
        "[BACKGROUND] First upload complete gen=%lu px0=0x%04x px1=0x%04x px2=0x%04x px3=0x%04x",
        (unsigned long)backgroundRequestedGeneration,
        (unsigned int)src[0],
        (unsigned int)src[1],
        (unsigned int)src[2],
        (unsigned int)src[3]);
    backgroundUploadLogged = true;
  }
}

static void ensureBackgroundLayer()
{
  if (backgroundLayerVram != nullptr)
  {
    return;
  }

  SRL::Bitmap::BitmapInfo backgroundInfo(BACKGROUND_BITMAP_WIDTH, BACKGROUND_BITMAP_HEIGHT);
  backgroundInfo.ColorMode = SRL::CRAM::TextureColorMode::RGB555;
  SRL::Math::Types::Vector2D backgroundPosition(0.0, 0.0);

  backgroundLayerVram = SRL::VDP2::VRAM::Allocate(131072, 32, SRL::VDP2::VramBank::A1, 4);
  if (backgroundLayerVram != nullptr)
  {
    void *secondBank = SRL::VDP2::VRAM::Allocate(131072, 32, SRL::VDP2::VramBank::B0, 4);
    if (secondBank == nullptr)
    {
      backgroundLayerVram = nullptr;
    }
  }
  backgroundLayerVramSize = 262144;
  if (backgroundLayerVram == nullptr)
  {
    SRL::Logger::LogFatal("[BACKGROUND] Failed to allocate VDP2 background layer");
    SRL::System::Exit(1);
  }

  SRL::Logger::LogInfo(
      "[BACKGROUND] NBG0 VRAM addr=%p size=%d mode=RGB555 container=%dx%d",
      backgroundLayerVram,
      backgroundLayerVramSize,
      BACKGROUND_BITMAP_WIDTH,
      BACKGROUND_BITMAP_HEIGHT);

  for (int i = 0; i < 2; i++)
  {
    backgroundBuffers[i] = (uint16_t *)SRL::Memory::HighWorkRam::Malloc(LAYER_WIDTH * LAYER_HEIGHT * sizeof(uint16_t));
    if (backgroundBuffers[i] == nullptr)
    {
      SRL::Logger::LogFatal("[BACKGROUND] Failed to allocate CPU background buffer %d", i);
      SRL::System::Exit(1);
    }
    memset(backgroundBuffers[i], 0, LAYER_WIDTH * LAYER_HEIGHT * sizeof(uint16_t));
  }

  SRL::VDP2::NBG0::SetCellAddress(backgroundLayerVram, backgroundLayerVramSize);
  SRL::VDP2::VRAM::Blank(backgroundLayerVram, backgroundLayerVramSize);
  SRL::VDP2::NBG0::Init(backgroundInfo);
  SRL::VDP2::NBG0::SetPosition(backgroundPosition);
  SRL::VDP2::NBG0::TransparentDisable();
  SRL::VDP2::NBG0::SetPriority(SRL::VDP2::Priority::Layer2);
  SRL::VDP2::NBG0::ScrollEnable();
  SRL::Logger::LogInfo("[BACKGROUND] NBG0 enabled priority=2 transparent=off base=0x%04x", (unsigned int)BACKGROUND_BASE_COLOR);
}

static void submitBackgroundRender()
{
  ensureBackgroundLayer();

  if (backgroundBuffers[0] == nullptr || backgroundBuffers[1] == nullptr)
  {
    return;
  }

  if (backgroundRenderTask.IsRunning())
  {
    return;
  }

  if (backgroundRenderTask.IsDone() && backgroundRenderTask.GetGeneration() > backgroundUploadedGeneration)
  {
    return;
  }

  const int bufferIndex = (int)(backgroundRequestedGeneration & 1u);
  backgroundRequestedGeneration++;
  backgroundRenderTask.Prepare(
      board,
      boardRepx,
      boardRepy,
      boardRepXn,
      boardRepYn,
      backgroundBuffers[bufferIndex],
      backgroundRequestedGeneration,
      bufferIndex);
  SRL::Slave::ExecuteOnSlave(backgroundRenderTask);
}

static void presentCompletedBackground()
{
  if (!backgroundRenderTask.IsDone())
  {
    return;
  }

  if (backgroundRenderTask.GetGeneration() <= backgroundUploadedGeneration)
  {
    return;
  }

  uploadBackgroundBitmap(backgroundBuffers[backgroundRenderTask.GetBufferIndex()]);
  backgroundUploadedGeneration = backgroundRenderTask.GetGeneration();
}

static void refreshBackgroundBitmap()
{
  ensureBackgroundLayer();

  if (backgroundBuffers[0] == nullptr)
  {
    return;
  }

  renderBackgroundBitmap(board, boardRepx, boardRepy, boardRepXn, boardRepYn, backgroundBuffers[0]);
  uploadBackgroundBitmap(backgroundBuffers[0]);
  backgroundRequestedGeneration++;
  backgroundUploadedGeneration = backgroundRequestedGeneration;
}

void initBackground()
{
  int i;
  for (i = 0; i < BOARD_MAX; i++)
  {
    board[i].width = NOT_EXIST;
  }
}

static void addBoard(int x, int y, int z, int width, int height)
{
  if (bdIdx >= BOARD_MAX)
    return;
  board[bdIdx].x = x;
  board[bdIdx].y = y;
  z *= SCREEN_DIVISOR;
  board[bdIdx].z = z;
  board[bdIdx].width = width / z;
  board[bdIdx].height = height / z;
  bdIdx++;
}

void setStageBackground(int bn)
{
  int i, j, k;
  ensureBackgroundLayer();
  bdIdx = 0;

  switch (bn)
  {
  case 0:
  case 6:
    addBoard(9000, 9000, 500, 25000, 25000);
    for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
      {
        if (i > 1 || j > 1)
        {
          addBoard(i * 16384, j * 16384, 500, 10000 + (i * 12345) % 3000, 10000 + (j * 54321) % 3000);
        }
      }
    }
    for (j = 0; j < 8; j++)
    {
      for (i = 0; i < 4; i++)
      {
        addBoard(0, i * 16384, 500 - j * 50, 20000 - j * 1000, 12000 - j * 500);
      }
    }
    for (i = 0; i < 8; i++)
    {
      addBoard(0, i * 8192, 100, 20000, 6400);
    }
    if (bn == 0)
    {
      boardMx = 40;
      boardMy = 300;
    }
    else
    {
      boardMx = -40;
      boardMy = 480;
    }
    boardRepx = boardRepy = 65536;
    boardRepXn = boardRepYn = 4;
    break;
  case 1:
    addBoard(12000, 12000, 400, 48000, 48000);
    addBoard(12000, 44000, 400, 48000, 8000);
    addBoard(44000, 12000, 400, 8000, 48000);
    for (i = 0; i < 16; i++)
    {
      addBoard(0, 0, 400 - i * 10, 16000, 16000);
      if (i < 6)
      {
        addBoard(9600, 16000, 400 - i * 10, 40000, 16000);
      }
    }
    boardMx = 128;
    boardMy = 512;
    boardRepx = boardRepy = 65536;
    boardRepXn = boardRepYn = 4;
    break;
  case 2:
    for (i = 0; i < 16; i++)
    {
      addBoard(7000 + i * 3000, 0, 1600 - i * 100, 24000, 5000);
      addBoard(7000 + i * 3000, 50000, 1600 - i * 100, 4000, 10000);
      addBoard(-7000 - i * 3000, 0, 1600 - i * 100, 24000, 5000);
      addBoard(-7000 - i * 3000, 50000, 1600 - i * 100, 4000, 10000);
    }
    boardMx = 0;
    boardMy = 1200;
    boardRepx = 0;
    boardRepy = 65536;
    boardRepXn = 1;
    boardRepYn = 10;
    break;
  case 3:
    addBoard(9000, 9000, 500, 30000, 30000);
    for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
      {
        if (i > 1 || j > 1)
        {
          addBoard(i * 16384, j * 16384, 500, 12000 + (i * 12345) % 3000, 12000 + (j * 54321) % 3000);
        }
      }
    }
    for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
      {
        if ((i > 1 || j > 1) && (i + j) % 3 == 0)
        {
          addBoard(i * 16384, j * 16384, 480, 9000 + (i * 12345) % 3000, 9000 + (j * 54321) % 3000);
        }
      }
    }
    addBoard(9000, 9000, 480, 20000, 20000);
    addBoard(9000, 9000, 450, 20000, 20000);
    addBoard(32768, 40000, 420, 65536, 5000);
    addBoard(30000, 32768, 370, 4800, 65536);
    addBoard(32768, 0, 8, 65536, 10000);
    boardMx = 10;
    boardMy = 100;
    boardRepx = boardRepy = 65536;
    boardRepXn = boardRepYn = 4;
    break;
  case 4:
    addBoard(32000, 12000, 160, 48000, 48000);
    addBoard(32000, 44000, 160, 48000, 8000);
    addBoard(64000, 12000, 160, 8000, 48000);
    for (i = 0; i < 16; i++)
    {
      addBoard(20000, 0, 160 - i * 10, 16000, 16000);
      if (i < 6)
      {
        addBoard(29600, 16000, 160 - i * 10, 40000, 16000);
      }
    }
    boardMx = 0;
    boardMy = 128;
    boardRepx = boardRepy = 65536;
    boardRepXn = 2;
    boardRepYn = 2;
    break;
  case 5:
    for (k = 0; k < 5; k++)
    {
      j = 0;
      for (i = 0; i < 16; i++)
      {
        addBoard(j, i * 4096, 200 - k * 10, 16000, 4096);
        addBoard(j + 16000 - j * 2, i * 4096, 200 - k * 10, 16000, 4096);
        if (i < 4)
          j += 2000;
        else if (i < 6)
          j -= 3500;
        else if (i < 12)
          j += 1500;
        else
          j -= 2000;
      }
    }
    boardMx = -10;
    boardMy = 25;
    boardRepx = boardRepy = 65536;
    boardRepXn = boardRepYn = 2;
    break;
  }

  refreshBackgroundBitmap();
}

void moveBackground()
{
  int i;
  Board *bd;
  for (i = 0; i < bdIdx; i++)
  {
    bd = &(board[i]);
    bd->x += boardMx;
    bd->y += boardMy;
    bd->x &= (boardRepx - 1);
    bd->y &= (boardRepy - 1);
  }

  refreshBackgroundBitmap();
}

void drawBackground()
{
}

unsigned int getBackgroundDebugBitmapHash()
{
  ensureBackgroundLayer();

  if (backgroundBuffers[0] == nullptr)
  {
    return 0u;
  }

  uint32_t hash = BACKGROUND_HASH_OFFSET;
  const uint16_t *pixels = backgroundBuffers[0];
  for (int i = 0; i < LAYER_WIDTH * LAYER_HEIGHT; i++)
  {
    const uint16_t value = pixels[i];
    hash = hashBackgroundByte(hash, (uint8_t)(value & 0xff));
    hash = hashBackgroundByte(hash, (uint8_t)((value >> 8) & 0xff));
  }

  return hash;
}

int getBackgroundDebugBoardCount()
{
  return bdIdx;
}
