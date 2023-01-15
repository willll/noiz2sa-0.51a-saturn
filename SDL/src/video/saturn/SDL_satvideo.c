/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id$";
#endif

/* SEGA Saturn SDL video driver implementation; ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_saturn.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_satvideo.h"
#include "SDL_satevents_c.h"
#include "SDL_satmouse_c.h"

#include <sgl.h>

#define SATURNVID_DRIVER_NAME "saturn"

#define TVSTAT      (*(volatile Uint16 *)0x25F80004)

/* Initialization/Query functions */
static int SAT_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **SAT_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *SAT_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int SAT_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void SAT_VideoQuit(_THIS);

/* Hardware surface functions */
static int SAT_AllocHWSurface(_THIS, SDL_Surface *surface);
static int SAT_LockHWSurface(_THIS, SDL_Surface *surface);
static void SAT_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void SAT_FreeHWSurface(_THIS, SDL_Surface *surface);

/* etc. */
static void SAT_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* SEGA Saturn driver bootstrap functions */

static int SAT_Available(void)
{
	const char *envr = getenv("SDL_VIDEODRIVER");
	if ((envr) && (strcmp(envr, SATURNVID_DRIVER_NAME) == 0)) {
		return(1);
	}

	return(0);
}

static void SAT_DeleteDevice(SDL_VideoDevice *device)
{
	free(device->hidden);
	free(device);
}

static unsigned int get_hz()
{
	if((TVSTAT & 1) == 0)
		return 60;
	else
		return 50;
}

static SDL_VideoDevice *SAT_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			free(device);
		}
		return(0);
	}
	memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = SAT_VideoInit;
	device->ListModes = SAT_ListModes;
	device->SetVideoMode = SAT_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = SAT_SetColors;
	device->UpdateRects = SAT_UpdateRects;
	device->VideoQuit = SAT_VideoQuit;
	device->AllocHWSurface = SAT_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = SAT_LockHWSurface;
	device->UnlockHWSurface = SAT_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = SAT_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = SAT_InitOSKeymap;
	device->PumpEvents = SAT_PumpEvents;

	device->free = SAT_DeleteDevice;

	return device;
}

VideoBootStrap SAT_bootstrap = {
	SATURNVID_DRIVER_NAME, "SDL saturn video driver",
	SAT_Available, SAT_CreateDevice
};


int SAT_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	fprintf(stderr, "WARNING: You are using the SDL saturn video driver!\n");

	/* Determine the screen depth (use default 8-bit depth) */
	/* we change this during the SDL_SetVideoMode implementation... */
	vformat->BitsPerPixel = 8;
	vformat->BytesPerPixel = 1;

	/* We're done! */
	return(0);
}

SDL_Rect **SAT_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
   	 return (SDL_Rect **) -1;
}

SDL_Surface *SAT_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	unsigned char tv_mode=TV_320x224;

  switch(width) {
    case 320:
      switch(height) {
        case 224:
          tv_mode = TV_320x224;
          break;
        case 240:
          tv_mode = TV_320x240;
          break;
        case 256:
          tv_mode = TV_320x256;
          break;
        case 448:
          tv_mode = TV_320x448;
          break;
        case 480:
          tv_mode = TV_320x480;
          break;
        case 512 :
          tv_mode = TV_320x512;
          break;
        default:
          SDL_SetError("TODO !");
      }
      break;
    case 352:
      switch(height) {
        case 224:
          tv_mode = TV_352x224;
          break;
        case 240:
          tv_mode = TV_352x240;
          break;
        case 256:
          tv_mode = TV_352x256;
          break;
        case 448:
          tv_mode = TV_352x448;
          break;
        case 480:
          tv_mode = TV_352x480;
          break;
        case 512 :
          tv_mode = TV_352x512;
          break;
        default:
          SDL_SetError("TODO !");
      }
      break;
    case 640:
      switch(height) {
        case 224:
          tv_mode = TV_640x224;
          break;
        case 240:
          tv_mode = TV_640x240;
          break;
        case 256:
          tv_mode = TV_640x256;
          break;
        case 448:
          tv_mode = TV_640x448;
          break;
        case 480:
          tv_mode = TV_640x480;
          break;
        case 512 :
          tv_mode = TV_640x512;
          break;
        default:
          SDL_SetError("TODO !");
      }
      break;
    case 704:
      switch(height) {
        case 224:
          tv_mode = TV_704x224;
          break;
        case 240:
          tv_mode = TV_704x240;
          break;
        case 256:
          tv_mode = TV_704x256;
          break;
        case 448:
          tv_mode = TV_704x448;
          break;
        case 480:
          tv_mode = TV_704x480;
          break;
        case 512 :
          tv_mode = TV_704x512;
          break;
        default:
          SDL_SetError("TODO !");
      }
      break;
    default:
      SDL_SetError("TODO !");
  }

  switch(bpp) {
    case 16:
      slColRAMMode(CRM16_2048);
      slCharNbg1(COL_TYPE_16 , CHAR_SIZE_1x1);
      break;
    case 256:
      slColRAMMode(CRM16_2048);
      slCharNbg1(COL_TYPE_256 , CHAR_SIZE_1x1);
      break;
    case 2048:
      slColRAMMode(CRM16_2048);
      slCharNbg1(COL_TYPE_2048 , CHAR_SIZE_1x1);
      break;
    case 32768:
      slColRAMMode(CRM16_2048);
      slCharNbg1(COL_TYPE_32768 , CHAR_SIZE_1x1);
      break;
    case 16777216:
      slColRAMMode(CRM16_2048);
      slCharNbg1(COL_TYPE_1M , CHAR_SIZE_1x1);
      break;
    default:
      SDL_SetError("TODO !");
}
/* 	printf("Setting mode %dx%d\n", width, height); */

  slInitSystem(tv_mode, NULL, 1);

  if ( this->hidden->buffer ) {
    free( this->hidden->buffer );
  }

  if ( current ) {
    free( current );
  }

  this->hidden->hz = get_hz();

  current = (SDL_Surface*)malloc(sizeof(SDL_Surface));

  //this->hidden->buffer = malloc(width * height * (bpp / 8));
  this->hidden->buffer = malloc(width * height ));
  if ( ! this->hidden->buffer ) {
    SDL_SetError("Couldn't allocate buffer for requested mode");
    return(NULL);
  }

	memset(this->hidden->buffer, 0, width * height * (bpp / 8));

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, 0, 0, 0, 0) ) {
		free(this->hidden->buffer);
		this->hidden->buffer = NULL;
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

	/* Set up the new mode framebuffer */
	current->flags = flags & SDL_FULLSCREEN;
	this->hidden->w = current->w = width;
	this->hidden->h = current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->pixels = this->hidden->buffer;

	/* We're done */
	return current;
}

/* We don't actually allow hardware surfaces other than the main one */
static int SAT_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void SAT_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* We need to wait for vertical retrace on page flipped displays */
static int SAT_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void SAT_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static void SAT_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	/* do nothing. */
}

int SAT_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* do nothing of note. */
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void SAT_VideoQuit(_THIS)
{
	if (this->screen->pixels != NULL)
	{
		free(this->screen->pixels);
		this->screen->pixels = NULL;
	}
}
