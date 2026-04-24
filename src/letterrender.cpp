/*
 * $Id: letterrender.c,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Letter render.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "screen.h"
#include "letterrender.h"
#include "letterdata.h"

void drawLetterBuf(int idx, int lx, int ly, int ltSize, int d,
                   Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf, int panel)
{
  int i;
  Fxp x, y, length, size, t;
  int deg;

  ltSize += (SCREEN_DIVISOR * SCREEN_DIVISOR) - 1;

  for (i = 0;; i++)
  {
    deg = spData[idx][i].deg;
    if (deg > 99990)
      break;
    x = spData[idx][i].x / Fxp::Convert(SCREEN_DIVISOR);
    y = spData[idx][i].y / Fxp::Convert(SCREEN_DIVISOR);
    size = spData[idx][i].size / Fxp::Convert(SCREEN_DIVISOR);
    length = spData[idx][i].length / Fxp::Convert(SCREEN_DIVISOR);
    size *= 1.3f;
    length *= 1.1f;
    switch (d)
    {
    case 0:
      x = -x;
      y = y;
      break;
    case 1:
      t = x;
      x = -y;
      y = -t;
      deg += 90;
      break;
    case 2:
      x = x;
      y = -y;
      deg += 180;
      break;
    case 3:
      t = x;
      x = y;
      y = t;
      deg += 270;
      break;
    }
    deg %= 180;
    if (panel)
    {
      if (deg < 45 || deg > 135)
      {
        drawBoxPanel((x * Fxp::Convert(ltSize)).As<int>() + lx, (y * Fxp::Convert(ltSize)).As<int>() + ly,
                     (size * Fxp::Convert(ltSize)).As<int>(), (length * Fxp::Convert(ltSize)).As<int>(), color1, color2, buf);
      }
      else
      {
        drawBoxPanel((x * Fxp::Convert(ltSize)).As<int>() + lx, (y * Fxp::Convert(ltSize)).As<int>() + ly,
                     (length * Fxp::Convert(ltSize)).As<int>(), (size * Fxp::Convert(ltSize)).As<int>(), color1, color2, buf);
      }
    }
    else
    {
      if (deg <= 45 || deg > 135)
      {
        drawBox((x * Fxp::Convert(ltSize)).As<int>() + lx, (y * Fxp::Convert(ltSize)).As<int>() + ly,
          (size * Fxp::Convert(ltSize)).As<int>(), (length * Fxp::Convert(ltSize)).As<int>(), color1, color2, buf);
      }
      else
      {
        drawBox((x * Fxp::Convert(ltSize)).As<int>() + lx, (y * Fxp::Convert(ltSize)).As<int>() + ly,
          (length * Fxp::Convert(ltSize)).As<int>(), (size * Fxp::Convert(ltSize)).As<int>(), color1, color2, buf);
      }
    }
  }
}

void drawLetter(int idx, int lx, int ly, int ltSize, int d,
                Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf)
{
  drawLetterBuf(idx, lx, ly, ltSize, d, color1, color2, buf, 1);
}

void drawStringBuf(const char *str, int lx, int ly, int ltSize, int d,
                   Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf, int panel)
{
  int x = lx, y = ly;
  int i, c, idx;
  for (i = 0;; i++)
  {
    if (str[i] == '\0')
      break;
    c = str[i];
    if (c != ' ')
    {
      if (c >= '0' && c <= '9')
      {
        idx = c - '0';
      }
      else if (c >= 'A' && c <= 'Z')
      {
        idx = c - 'A' + 10;
      }
      else if (c >= 'a' && c <= 'z')
      {
        idx = c - 'a' + 10;
      }
      else if (c == '.')
      {
        idx = 36;
      }
      else if (c == '-')
      {
        idx = 38;
      }
      else if (c == '+')
      {
        idx = 39;
      }
      else
      {
        idx = 37;
      }
      drawLetterBuf(idx, x, y, ltSize, d, color1, color2, buf, panel);
    }
    switch (d)
    {
    case 0:
      x -= ((Fxp::Convert(ltSize) * 1.7f) / Fxp::Convert(SCREEN_DIVISOR)).As<int>();
      break;
    case 1:
      y -= ((Fxp::Convert(ltSize) * 1.7f) / Fxp::Convert(SCREEN_DIVISOR)).As<int>();
      break;
    case 2:
      x += ((Fxp::Convert(ltSize) * 1.7f) / Fxp::Convert(SCREEN_DIVISOR)).As<int>();
      break;
    case 3:
      y += ((Fxp::Convert(ltSize) * 1.7f) / Fxp::Convert(SCREEN_DIVISOR)).As<int>();
      break;
    }
  }
}

void drawString(const char *str, int lx, int ly, int ltSize, int d,
                Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf)
{
  drawStringBuf(str, lx, ly, ltSize, d, color1, color2, buf, 1);
}
