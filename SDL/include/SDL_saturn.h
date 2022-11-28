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

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include "sgl.h"

#undef pal

#include "sega_def.h"
#include "sega_sys.h"
#include "sega_gfs.h"
#include "sega_tim.h"
#include "sega_mem.h"

#ifndef ACTION_REPLAY
GfsDirName dir_name[MAX_DIR];
#endif

#define malloc(X) MEM_Malloc(X)
#define free(X) MEM_Free(X)

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_saturn_h */
