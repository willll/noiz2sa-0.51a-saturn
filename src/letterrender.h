/*
 * $Id: letterrender.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Letter render header file.
 *
 * @version $Revision: 1.1.1.1 $
 */

#include "screen.h"

void drawLetterBuf(int idx, int lx, int ly, int ltSize, int d,
		Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf, int panel);
void drawLetter(int idx, int lx, int ly, int ltSize, int d,
		Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
void drawStringBuf(const char *str, int lx, int ly, int ltSize, int d, 
		Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf, int panel);
void drawString(const char *str, int lx, int ly, int ltSize, int d, 
		Canvas::Pixel color1, Canvas::Pixel color2, Canvas::Pixel *buf);
