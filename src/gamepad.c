/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/*
  Copyright 2016 Michal Nowikowski. All rights reserved.
  Adjusted SDL_gamecontroller.c based on SDL2 for SDL1.
*/

// based on https://gist.github.com/meghprkh/9cdce0cd4e0f41ce93413b250a207a55
// and on http://www.linuxjournal.com/article/6429

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/ioctl.h>
//#include <linux/input.h>
#include <unistd.h>
//#include <endian.h>
#include <stdint.h>
//#include <dirent.h>
#include <string.h>

#include <SDL.h>

#include "gamepad.h"


#define test_bit(nr, addr) \
    (((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)

static int
IsJoystick(int fd)
{


    return 1;
}



#define SDL_InvalidParamError(param)    SDL_SetError("Parameter '%s' is invalid", (param))

#define SDL_CONTROLLER_PLATFORM_FIELD "platform:"

#define ABS(_x) ((_x) < 0 ? -(_x) : (_x))

#define SDL_zero(x) SDL_memset(&(x), 0, sizeof((x)))


/* A structure that encodes the stable unique id for a joystick device */
typedef struct {
    Uint8 data[16];
} SDL_JoystickGUID;

/* keep track of the hat and mask value that transforms this hat movement into a button/axis press */
struct _SDL_HatMapping
{
    int hat;
    Uint8 mask;
};

#define k_nMaxReverseEntries 20

/**
 * We are encoding the "HAT" as 0xhm. where h == hat ID and m == mask
 * MAX 4 hats supported
 */
#define k_nMaxHatEntries 0x3f + 1

/* our in memory mapping db between joystick objects and controller mappings */
struct _SDL_ControllerMapping
{


};


/*
 * convert a string to its enum equivalent
 */
SDL_GameControllerButton SDL_GameControllerGetButtonFromString(const char *pchString)
{

    return SDL_CONTROLLER_BUTTON_INVALID;
}

/*
 * convert a string to its enum equivalent
 */
SDL_GameControllerAxis SDL_GameControllerGetAxisFromString(const char *pchString)
{

    return SDL_CONTROLLER_AXIS_INVALID;
}



/* convert the string version of a joystick guid to the struct */
SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *pchGUID)
{
    SDL_JoystickGUID guid;
    // int maxoutputbytes= sizeof(guid);
    // size_t len = SDL_strlen(pchGUID);
    // Uint8 *p;
    // size_t i;
    //
    // /* Make sure it's even */
    // len = (len) & ~0x1;
    //
    // SDL_memset(&guid, 0x00, sizeof(guid));
    //
    // p = (Uint8 *)&guid;
    // for (i = 0; (i < len) && ((p - (Uint8 *)&guid) < maxoutputbytes); i+=2, p++) {
    //     *p = (nibble(pchGUID[i]) << 4) | nibble(pchGUID[i+1]);
    // }

    return guid;
}

/*
 * Add or update an entry into the Mappings Database
 */
int
SDL_GameControllerAddMapping(const char *mappingString)
{
  return 0;
    // char *pchGUID;
    // SDL_JoystickGUID jGUID;
    // SDL_bool is_xinput_mapping = SDL_FALSE;
    // SDL_bool is_emscripten_mapping = SDL_FALSE;
    // SDL_bool existing = SDL_FALSE;
    // ControllerMapping_t *pControllerMapping;
    //
    // if (!mappingString) {
    //     SDL_InvalidParamError("mappingString");
    //     return -1;
    // }
    //
    // pchGUID = SDL_PrivateGetControllerGUIDFromMappingString(mappingString);
    // if (!pchGUID) {
    //     SDL_SetError("Couldn't parse GUID from %s", mappingString);
    //     return -1;
    // }
    // if (!SDL_strcasecmp(pchGUID, "xinput")) {
    //     is_xinput_mapping = SDL_TRUE;
    // }
    // if (!SDL_strcasecmp(pchGUID, "emscripten")) {
    //     is_emscripten_mapping = SDL_TRUE;
    // }
    // jGUID = SDL_JoystickGetGUIDFromString(pchGUID);
    // SDL_free(pchGUID);
    //
    // pControllerMapping = SDL_PrivateAddMappingForGUID(jGUID, mappingString, &existing);
    // if (!pControllerMapping) {
    //     return -1;
    // }
    //
    // if (existing) {
    //     return 0;
    // } else {
    //     if (is_xinput_mapping) {
    //         s_pXInputMapping = pControllerMapping;
    //     }
    //     if (is_emscripten_mapping) {
    //         s_pEmscriptenMapping = pControllerMapping;
    //     }
    //     return 1;
    // }
}


SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int device_index)
{
    // char guidstr[33];
    SDL_JoystickGUID emptyGUID;
    // char *dev_path = get_gamepad_dev_path(0);
    // if (!dev_path) {
    //   SDL_zero(emptyGUID);
    //   return emptyGUID;
    // }
    //
    // get_guid(dev_path, guidstr);
    // free(dev_path);

    return emptyGUID;
}

/*
 * Open a controller for use - the index passed as an argument refers to
 * the N'th controller on the system.  This index is the value which will
 * identify this controller in future controller events.
 *
 * This function returns a controller identifier, or NULL if an error occurred.
 */
SDL_GameController *
SDL_GameControllerOpen(int device_index)
{
    SDL_GameController *gamecontroller;
    // SDL_GameController *gamecontrollerlist;
    // ControllerMapping_t *pSupportedController = NULL;
    //
    // if ((device_index < 0) || (device_index >= SDL_NumJoysticks())) {
    //     SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
    //     return (NULL);
    // }
    //
    // gamecontrollerlist = SDL_gamecontrollers;
    // /* If the controller is already open, return it */
    // /* TODO
    // while (gamecontrollerlist) {
    //     if (SDL_SYS_GetInstanceIdOfDeviceIndex(device_index) == gamecontrollerlist->joystick->instance_id) {
    //             gamecontroller = gamecontrollerlist;
    //             ++gamecontroller->ref_count;
    //             return (gamecontroller);
    //     }
    //     gamecontrollerlist = gamecontrollerlist->next;
    // }
    // */
    //
    // /* Find a controller mapping */
    // pSupportedController =  SDL_PrivateGetControllerMapping(device_index);
    // if (!pSupportedController) {
    //     SDL_SetError("Couldn't find mapping for device (%d)", device_index);
    //     return (NULL);
    // }
    //
    // /* Create and initialize the joystick */
    // gamecontroller = (SDL_GameController *) SDL_malloc((sizeof *gamecontroller));
    // if (gamecontroller == NULL) {
    //     SDL_OutOfMemory();
    //     return NULL;
    // }
    //
    // SDL_memset(gamecontroller, 0, (sizeof *gamecontroller));
    // gamecontroller->joystick = SDL_JoystickOpen(device_index);
    // if (!gamecontroller->joystick) {
    //     SDL_free(gamecontroller);
    //     return NULL;
    // }
    //
    // SDL_PrivateLoadButtonMapping(&gamecontroller->mapping, pSupportedController->guid, pSupportedController->name, pSupportedController->mapping);
    //
    // printf("Gamecontroller: %s\n", pSupportedController->name);
    //
    // /* Add joystick to list */
    // ++gamecontroller->ref_count;
    // /* Link the joystick in the list */
    // gamecontroller->next = SDL_gamecontrollers;
    // SDL_gamecontrollers = gamecontroller;
    //
    // //SDL_SYS_JoystickUpdate(gamecontroller->joystick);
    // SDL_JoystickUpdate(); // TODO

    return (gamecontroller);
}

/*
 * Get the current state of an axis control on a controller
 */
Sint16
SDL_GameControllerGetAxis(SDL_GameController * gamecontroller, SDL_GameControllerAxis axis)
{
    // if (!gamecontroller)
    //     return 0;
    //
    // if (gamecontroller->mapping.axes[axis] >= 0) {
    //     Sint16 value = (SDL_JoystickGetAxis(gamecontroller->joystick, gamecontroller->mapping.axes[axis]));
    //     switch (axis) {
    //         case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
    //         case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
    //             /* Shift it to be 0 - 32767. */
    //             value = value / 2 + 16384;
    //         default:
    //             break;
    //     }
    //     return value;
    // } else if (gamecontroller->mapping.buttonasaxis[axis] >= 0) {
    //     Uint8 value;
    //     value = SDL_JoystickGetButton(gamecontroller->joystick, gamecontroller->mapping.buttonasaxis[axis]);
    //     if (value > 0)
    //         return 32767;
    //     return 0;
    // }
    return 0;
}


/*
 * Get the current state of a button on a controller
 */
Uint8
SDL_GameControllerGetButton(SDL_GameController * gamecontroller, SDL_GameControllerButton button)
{
    // if (!gamecontroller)
    //     return 0;
    //
    // if (gamecontroller->mapping.buttons[button] >= 0) {
    //     return (SDL_JoystickGetButton(gamecontroller->joystick, gamecontroller->mapping.buttons[button]));
    // } else if (gamecontroller->mapping.axesasbutton[button] >= 0) {
    //     Sint16 value;
    //     value = SDL_JoystickGetAxis(gamecontroller->joystick, gamecontroller->mapping.axesasbutton[button]);
    //     if (ABS(value) > 32768/2)
    //         return 1;
    //     return 0;
    // } else if (gamecontroller->mapping.hatasbutton[button].hat >= 0) {
    //     Uint8 value;
    //     value = SDL_JoystickGetHat(gamecontroller->joystick, gamecontroller->mapping.hatasbutton[button].hat);
    //
    //     if (value & gamecontroller->mapping.hatasbutton[button].mask)
    //         return 1;
    //     return 0;
    // }

    return 0;
}
