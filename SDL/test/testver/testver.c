
/* Test program to compare the compile-time version of SDL with the linked
   version of SDL
*/

#include <stdio.h>

#include "SDL.h"
#include "SDL_byteorder.h"


#include <sega_dbg.h>
#include <sega_sys.h>

#include <sgl.h>

#include <string.h>
#include <stdlib.h>


static	volatile Uint16	PadData1  = 0x0000;

static const unsigned short buffer_size = 256;

int main(int argc, char *argv[])
{
	SDL_version compiled;
	unsigned char line = 1;

	char text_buffer[buffer_size];
	memset(text_buffer, 0, buffer_size);

//	*((char*)-1) = 'x'; // crash !

	/* Initialize SDL */
	if ( SDL_Init(SDL_INIT_EVERYTHING) < 0 ) {
		sprintf(text_buffer, "Couldn't initialize SDL: %s\n",SDL_GetError());
		slPrint(text_buffer, slLocate (1,line++));
		SYS_Exit(1);
	}
	SDL_SetVideoMode(320, 240, 8, SDL_HWSURFACE);
	slPrint("SDL initialized\n", slLocate (1,line++));

#if SDL_VERSION_ATLEAST(1, 2, 0)
	slPrint("Compiled with SDL 1.2 or newer\n", slLocate (1,line++));
#else
	slPrint("Compiled with SDL older than 1.2\n",  slLocate (1,line++));
#endif
	SDL_VERSION(&compiled);

	memset(text_buffer, 0, buffer_size);
	sprintf(text_buffer, "Compiled version: %d.%d.%d\n",
			compiled.major, compiled.minor, compiled.patch);
	slPrint(text_buffer, slLocate (1,line++));

	memset(text_buffer, 0, buffer_size);
	sprintf(text_buffer, "Linked version: %d.%d.%d\n",
			SDL_Linked_Version()->major,
			SDL_Linked_Version()->minor,
			SDL_Linked_Version()->patch);
	slPrint(text_buffer, slLocate (1,line++));

	memset(text_buffer, 0, buffer_size);
	sprintf(text_buffer, "This is a %s endian machine.\n",
		(SDL_BYTEORDER == SDL_LIL_ENDIAN) ? "little" : "big");
	slPrint(text_buffer, slLocate (1,line++));

	slSynch();

	for(;;);

	SDL_Quit();

	return(0);
}
