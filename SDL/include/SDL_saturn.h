/*
...
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id$";
#endif

/* Include file for SDL application focus event handling */

#ifndef _SDL_saturn_h
#define _SDL_saturn_h

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include "sgl.h"

#include "sega_sys.h"
#include "sega_gfs.h"
#include "sega_tim.h"

#ifndef ACTION_REPLAY
GfsDirName dir_name[MAX_DIR];
#endif

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_saturn_h */
