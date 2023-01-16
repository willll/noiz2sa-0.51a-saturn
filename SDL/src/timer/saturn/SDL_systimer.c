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

    BERO
    bero@geocities.co.jp

    based on win32/SDL_systimer.c

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id$";
#endif

//#include <kos.h>

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_error.h"
#include "../SDL_timer_c.h"

#include <sega_tim.h>

static unsigned start;

/* Data to handle a single periodic alarm */
static int timer_alive = 0;
//static SDL_Thread *timer = NULL;
/*
	jif =  ms * HZ /1000
	ms  = jif * 1000/HZ
*/

void SDL_StartTicks()
{
	/* Set first ticks value */
	//start = jiffies; // FIXME
}

Uint32 SDL_GetTicks()
{
	//return((jiffies-start)*1000/HZ);
  return 0; // FIXME
}

void SDL_Delay(Uint32 ms)
{
	//thd_sleep(ms);
}



static int RunTimer(void *unused)
{
	/*while ( timer_alive ) {
		if ( SDL_timer_running ) {
			SDL_ThreadedTimerCheck();
		}
		SDL_Delay(10);
	}*/
	return(0);
}

/* This is only called if the event thread is not running */
int SDL_SYS_TimerInit()
{
	timer_alive = 1;

  TIM_FRT_INIT(TIM_CKS_32);
  TIM_FRT_SET_16(0);

	return 0;
}

void SDL_SYS_TimerQuit()
{
	timer_alive = 0;

}

int SDL_SYS_StartTimer()
{
	SDL_SetError("Internal logic error: DC uses threaded timer");
	return(-1);
}

void SDL_SYS_StopTimer()
{
	return;
}
