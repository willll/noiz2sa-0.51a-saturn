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

#include "memory_factory.h"
#include "noiz2sa.h"
#include "screen.h"
#include "clrtbl.h"
#include "vector.h"
#include "background.h"

static Board board[BOARD_MAX];
static void *backgroundLayerVram = nullptr;
static int backgroundLayerVramSize = 0;
static uint16_t *backgroundBuffer = nullptr;
static bool backgroundUploadLogged = false;
static constexpr int BACKGROUND_BITMAP_WIDTH = 512;
static constexpr int BACKGROUND_BITMAP_HEIGHT = 256;
static constexpr int BACKGROUND_SCREEN_X = (SCREEN_WIDTH - LAYER_WIDTH) / 2;

static int bdIdx;
static int boardMx, boardMy;
static int boardRepx, boardRepy;
static int boardRepXn, boardRepYn;
// Start a new background bitmap at most every this many move ticks.
static constexpr uint32_t kBgRenderDivisor = 6u;

static constexpr uint16_t BACKGROUND_BASE_COLOR = 0xffff;
static constexpr uint32_t BACKGROUND_HASH_OFFSET = 2166136261u;
static constexpr uint32_t BACKGROUND_HASH_PRIME = 16777619u;

/** @brief Mixes one byte into the background bitmap hash. */
static inline uint32_t hashBackgroundByte(uint32_t hash, uint8_t value)
{
  return (hash ^ value) * BACKGROUND_HASH_PRIME;
}

/** @brief Writes a solid horizontal span into the background buffer. */
static void fillBackgroundSpan(uint16_t *dst, int count, uint16_t color)
{
  for (int i = 0; i < count; i++)
  {
    dst[i] = color;
  }
}

/** @brief One board rectangle, already clipped to the screen. */
struct BackgroundRect
{
  int16_t x, y, width, height;
};
// Every board repeated boardRepXn x boardRepYn times; no stage repeats more than 4x4.
static constexpr int kMaxBackgroundRects = BOARD_MAX * 16;

/** @brief Clips one board rectangle to the screen; false if nothing is left to draw. */
static bool clipBackgroundRect(int centerX, int centerY, int width, int height, BackgroundRect *out)
{
  int x = centerX - (width >> 1);
  int y = centerY - (height >> 1);
  if (x < 0)
  {
    width += x;
    x = 0;
  }
  if (x + width > LAYER_WIDTH)
  {
    width = LAYER_WIDTH - x;
  }
  if (width <= 1)
    return false;
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
    return false;
  *out = BackgroundRect{(int16_t)x, (int16_t)y, (int16_t)width, (int16_t)height};
  return true;
}

/** @brief Positions and clips every visible board rectangle, in drawing order.
 *  Done once per render: the divisions here used to be repeated for every band. */
static int collectBackgroundRects(
    const Board *srcBoard,
    int srcBoardRepx,
    int srcBoardRepy,
    int srcBoardRepXn,
    int srcBoardRepYn,
    BackgroundRect *rects)
{
  int count = 0;
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
        if (count < kMaxBackgroundRects &&
            clipBackgroundRect(
                (bd->x + ox) / bd->z + LAYER_WIDTH / 2,
                (bd->y + oy) / bd->z + LAYER_HEIGHT / 2,
                bd->width,
                bd->height,
                &rects[count]))
        {
          count++;
        }
      }
    }
  }
  return count;
}

/** @brief Renders rows [y0, y1) of the background bitmap from collected rectangles. */
static void renderBackgroundRectRows(const BackgroundRect *rects, int count, uint16_t *dst, int y0, int y1)
{
  for (int i = y0 * LAYER_WIDTH; i < y1 * LAYER_WIDTH; i++)
  {
    dst[i] = BACKGROUND_BASE_COLOR;
  }

  const uint16_t fillColor = (uint16_t)color[1];
  const uint16_t borderColor = (uint16_t)color[3];
  for (int r = 0; r < count; r++)
  {
    const BackgroundRect &rc = rects[r];
    const int top = rc.y;
    const int bottom = rc.y + rc.height - 1;
    const int firstRow = (top > y0) ? top : y0;
    const int lastRow = (bottom < y1 - 1) ? bottom : y1 - 1;
    for (int row = firstRow; row <= lastRow; row++)
    {
      uint16_t *line = &(dst[rc.x + row * LAYER_WIDTH]);
      if (row == top || row == bottom)
      {
        fillBackgroundSpan(line, rc.width, borderColor);
      }
      else
      {
        line[0] = borderColor;
        fillBackgroundSpan(line + 1, rc.width - 2, fillColor);
        line[rc.width - 1] = borderColor;
      }
    }
  }
}

// One list for both CPUs: the slave fills it at step 0 and draws the cycle's
// bands from it; the master only renders after stopBackgroundPipeline(), when
// the slave is idle. LWRAM (allocated with the bitmap buffer): 32KB, read once per band.
static BackgroundRect *sBgRects = nullptr;
static int sBgRectCount = 0;

/** @brief Renders the full background bitmap from the current board set (master). */
static void renderBackgroundBitmap(
    const Board *srcBoard,
    int srcBoardRepx,
    int srcBoardRepy,
    int srcBoardRepXn,
    int srcBoardRepYn,
    uint16_t *dst)
{
  sBgRectCount = collectBackgroundRects(srcBoard, srcBoardRepx, srcBoardRepy, srcBoardRepXn, srcBoardRepYn, sBgRects);
  renderBackgroundRectRows(sBgRects, sBgRectCount, dst, 0, LAYER_HEIGHT);
}

/** @brief Copies a rendered background bitmap into the VDP2 bitmap container.
 *  Row by row: the source stride (LAYER_WIDTH) differs from the container
 *  stride (BACKGROUND_BITMAP_WIDTH). The SCU cannot read LWRAM, where the
 *  buffer lives (DMA_ScuMemCopy silently fell back to the master's CPU DMA with
 *  a wait per row, ~10ms of master time per upload), so the slave copies it. */
static void copyBackgroundToVram(const uint16_t *src, void *vram)
{
  const uint8_t *srcRow = (const uint8_t *)src;
  uint8_t *dstRow = (uint8_t *)vram + (BACKGROUND_SCREEN_X * (int)sizeof(uint16_t));
  for (int row = 0; row < LAYER_HEIGHT; row++)
  {
    memcpy(dstRow, srcRow, LAYER_WIDTH * sizeof(uint16_t));
    srcRow += LAYER_WIDTH * sizeof(uint16_t);
    dstRow += BACKGROUND_BITMAP_WIDTH * sizeof(uint16_t);
  }
}

// SGL's slSynch() waits for the slave, so a slave task longer than the rest of
// the frame stalls that frame: a whole render took ~60ms (mednafen, 2026-10-30)
// and made every 6th frame last 4-5 VBlanks. The slave therefore renders one
// band per tick, then copies the finished bitmap to VRAM in a last step.
static constexpr int kBgBands = 8;
static constexpr int kBgCopyStep = kBgBands;
static_assert(LAYER_HEIGHT % kBgBands == 0, "background bands must tile the layer");

class BackgroundRenderTask : public SRL::Types::ITask
{
public:
  /** @brief Captures the board state for the next render cycle. */
  void Prepare(
      const Board *srcBoard,
      int srcBoardRepx,
      int srcBoardRepy,
      int srcBoardRepXn,
      int srcBoardRepYn,
      uint16_t *dst,
      void *vram)
  {
    memcpy(snapshotBoard, srcBoard, sizeof(snapshotBoard));
    snapshotBoardRepx = srcBoardRepx;
    snapshotBoardRepy = srcBoardRepy;
    snapshotBoardRepXn = srcBoardRepXn;
    snapshotBoardRepYn = srcBoardRepYn;
    targetBuffer = dst;
    targetVram = vram;
  }

  /** @brief Selects what the next run does: a band index, kBgCopyStep, or -1 (none). */
  void SetStep(int nextStep)
  {
    step = nextStep;
  }

protected:
  /** @brief Runs one pipeline step on the slave. */
  void Do() override
  {
    // SRL purges the slave cache only in DEBUG builds. The master rewrites the
    // snapshot and step between runs, so drop any stale cached copy first.
    *reinterpret_cast<volatile uint8_t *>(0xFFFFFE92) |= 0x10;
    asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop" ::: "memory");

    if (targetBuffer != nullptr && step >= 0 && step < kBgBands)
    {
      if (step == 0)
      {
        sBgRectCount = collectBackgroundRects(snapshotBoard, snapshotBoardRepx, snapshotBoardRepy,
                                              snapshotBoardRepXn, snapshotBoardRepYn, sBgRects);
      }
      const int bandRows = LAYER_HEIGHT / kBgBands;
      renderBackgroundRectRows(sBgRects, sBgRectCount, targetBuffer, step * bandRows, (step + 1) * bandRows);
    }
    else if (targetBuffer != nullptr && step == kBgCopyStep && targetVram != nullptr)
    {
      copyBackgroundToVram(targetBuffer, targetVram);
    }

  }

private:
  Board snapshotBoard[BOARD_MAX];
  int snapshotBoardRepx = 0;
  int snapshotBoardRepy = 0;
  int snapshotBoardRepXn = 0;
  int snapshotBoardRepYn = 0;
  uint16_t *targetBuffer = nullptr;
  void *targetVram = nullptr;
  int step = 0;
};

static BackgroundRenderTask backgroundRenderTask;
static int sBgStep = -1; ///< -1 idle, 0..kBgBands-1 render band, kBgCopyStep copy
static bool sBgTaskInFlight = false;
static uint32_t sBgTicksSinceStart = 0;

/** @brief Uploads a rendered background bitmap into VDP2 VRAM from the master. */
static void uploadBackgroundBitmap(const uint16_t *src)
{
  if (backgroundLayerVram == nullptr || src == nullptr)
  {
    return;
  }

  copyBackgroundToVram(src, backgroundLayerVram);

  if (!backgroundUploadLogged)
  {
    SRL::Logger::LogInfo(
        "[BACKGROUND] First upload complete px0=0x%04x px1=0x%04x px2=0x%04x px3=0x%04x",
        (unsigned int)src[0],
        (unsigned int)src[1],
        (unsigned int)src[2],
        (unsigned int)src[3]);
    backgroundUploadLogged = true;
  }
}

/** @brief Allocates the VDP2 background layer if it is not already present. */
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

  // CPU-side render buffer: LWRAM, since HWRAM is too tight for 76KB.
  SRL::Logger::LogInfo("[BACKGROUND] HWRAM free before bg buffers: %lu LWRAM free: %lu",
                       (unsigned long)SRL::Memory::HighWorkRam::GetFreeSpace(),
                       (unsigned long)SRL::Memory::LowWorkRam::GetFreeSpace());
  backgroundBuffer = allocateLowWorkRamItems<uint16_t>(LAYER_WIDTH * LAYER_HEIGHT);
  if (backgroundBuffer == nullptr)
  {
    SRL::Logger::LogFatal("[BACKGROUND] Failed to allocate CPU background buffer (LWRAM free=%lu)",
                          (unsigned long)SRL::Memory::LowWorkRam::GetFreeSpace());
    SRL::System::Exit(1);
  }
  memset(backgroundBuffer, 0, LAYER_WIDTH * LAYER_HEIGHT * sizeof(uint16_t));
  sBgRects = allocateLowWorkRamItems<BackgroundRect>(kMaxBackgroundRects);
  if (sBgRects == nullptr)
  {
    SRL::Logger::LogFatal("[BACKGROUND] Failed to allocate background rect list (LWRAM free=%lu)",
                          (unsigned long)SRL::Memory::LowWorkRam::GetFreeSpace());
    SRL::System::Exit(1);
  }
  SRL::Logger::LogInfo("[BACKGROUND] bg buffers allocated OK: LWRAM free now %lu",
                       (unsigned long)SRL::Memory::LowWorkRam::GetFreeSpace());

  SRL::VDP2::NBG0::SetCellAddress(backgroundLayerVram, backgroundLayerVramSize);
  SRL::VDP2::VRAM::Blank(backgroundLayerVram, backgroundLayerVramSize);
  SRL::VDP2::NBG0::Init(backgroundInfo);
  SRL::VDP2::NBG0::SetPosition(backgroundPosition);
  SRL::VDP2::NBG0::TransparentDisable();
  SRL::VDP2::NBG0::SetPriority(SRL::VDP2::Priority::Layer2);
  SRL::VDP2::NBG0::ScrollEnable();
  SRL::Logger::LogInfo("[BACKGROUND] NBG0 enabled priority=2 transparent=off base=0x%04x", (unsigned int)BACKGROUND_BASE_COLOR);
}

/** @brief Dispatches one slave run per tick: at most one short background step. */
static void stepBackgroundPipeline()
{
  ensureBackgroundLayer();

  if (backgroundBuffer == nullptr)
  {
    return;
  }

  sBgTicksSinceStart++;
  int nextStep = -1;
  if (sBgTaskInFlight)
  {
    if (!backgroundRenderTask.IsDone())
    {
      return; // never wait: the slave is still on the previous run
    }
    sBgTaskInFlight = false;
    if (sBgStep >= 0 && sBgStep < kBgCopyStep)
    {
      nextStep = sBgStep + 1;
    }
  }

  if (nextStep < 0 && sBgTicksSinceStart >= kBgRenderDivisor)
  {
    backgroundRenderTask.Prepare(board, boardRepx, boardRepy, boardRepXn, boardRepYn,
                                 backgroundBuffer, backgroundLayerVram);
    sBgTicksSinceStart = 0;
    nextStep = 0;
  }

  sBgStep = nextStep;
  backgroundRenderTask.SetStep(sBgStep);
  SRL::Slave::ExecuteOnSlave(backgroundRenderTask);
  sBgTaskInFlight = true;
}

/** @brief Waits for any slave step and restarts the pipeline, so the master can
 *  use the buffer; a later step would otherwise draw the old snapshot over it. */
static void stopBackgroundPipeline()
{
  if (sBgTaskInFlight)
  {
    while (!backgroundRenderTask.IsDone())
    {
    }
    sBgTaskInFlight = false;
  }
  sBgStep = -1;
}

/** @brief Redraws the background immediately (stage start). */
static void refreshBackgroundBitmap()
{
  ensureBackgroundLayer();

  if (backgroundBuffer == nullptr)
  {
    return;
  }

  stopBackgroundPipeline();
  renderBackgroundBitmap(board, boardRepx, boardRepy, boardRepXn, boardRepYn, backgroundBuffer);
  uploadBackgroundBitmap(backgroundBuffer);
}

/** @brief Initialises the background system. */
void initBackground()
{
  int i;
  for (i = 0; i < BOARD_MAX; i++)
  {
    board[i].width = NOT_EXIST;
  }
}

/** @brief Adds one board element to the background layout. */
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

/** @brief Selects the stage background for the given stage number. */
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

/** @brief Advances the background animation and update cadence. */
void moveBackground()
{
  int i;
  Board *bd;
  // Always update scroll positions — cheap integer math, needed every tick
  // for correct game-speed scrolling regardless of render rate.
  for (i = 0; i < bdIdx; i++)
  {
    bd = &(board[i]);
    bd->x += boardMx;
    bd->y += boardMy;
    bd->x &= (boardRepx - 1);
    bd->y &= (boardRepy - 1);
  }

  stepBackgroundPipeline();
}

/** @brief Draws the current background state to the screen. */
void drawBackground()
{
  // Nothing to do on the master: the slave pipeline copies finished bitmaps to VRAM.
}

/** @brief Returns a hash of the current background bitmap for debugging. */
unsigned int getBackgroundDebugBitmapHash()
{
  ensureBackgroundLayer();

  if (backgroundBuffer == nullptr)
  {
    return 0u;
  }

  // Render current board state synchronously so the hash reflects the latest
  // board positions regardless of the slave pipeline's progress.
  stopBackgroundPipeline();
  renderBackgroundBitmap(board, boardRepx, boardRepy, boardRepXn, boardRepYn, backgroundBuffer);

  uint32_t hash = BACKGROUND_HASH_OFFSET;
  const uint16_t *pixels = backgroundBuffer;
  for (int i = 0; i < LAYER_WIDTH * LAYER_HEIGHT; i++)
  {
    const uint16_t value = pixels[i];
    hash = hashBackgroundByte(hash, (uint8_t)(value & 0xff));
    hash = hashBackgroundByte(hash, (uint8_t)((value >> 8) & 0xff));
  }

  return hash;
}

/** @brief Returns the number of active boards in the background layout. */
int getBackgroundDebugBoardCount()
{
  return bdIdx;
}
