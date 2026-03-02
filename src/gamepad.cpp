/*
 * Saturn-compatible gamepad implementation
 * Simplified version using SDL1 joystick API directly
 * Replaces Linux evdev-based implementation
 */

#include <SDL.h>
#include <srl_memory.hpp>  // for memory allocation
#include "gamepad.h"

static int g_joystick_initialized = 0;

// Game controller structure (simplified for SDL1)
struct _SDL_GameController {
    SDL_Joystick *joystick;
    int ref_count;
    int index;
};

static SDL_GameController *g_controller = null_ptr;

// Initialize game controller subsystem
int SDL_GameControllerInit(void) {
    if (g_joystick_initialized) {
        return 0;
    }
    
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        return -1;
    }
    
    g_joystick_initialized = 1;
    return 0;
}

// Check if joystick is a game controller
SDL_bool SDL_IsGameController(int joystick_index) {
    // On Saturn, assume all joysticks are game controllers
    if (joystick_index < 0 || joystick_index >= SDL_NumJoysticks()) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

// Load controller mappings from file (no-op on Saturn)
Uint8 SDL_GameControllerAddMappingsFromFile(char *db_path, Uint8 freerw) {
    (void)db_path;
    (void)freerw;
    // Saturn doesn't need external mapping files
    return 0;
}

// Open a game controller
SDL_GameController *SDL_GameControllerOpen(int joystick_index) {
    if (joystick_index < 0 || joystick_index >= SDL_NumJoysticks()) {
        return null_ptr;
    }
    
    // Reuse existing controller if already open
    if (g_controller && g_controller->index == joystick_index) {
        g_controller->ref_count++;
        return g_controller;
    }
    
    SDL_Joystick *joystick = SDL_JoystickOpen(joystick_index);
    if (!joystick) {
        return null_ptr;
    }
    
    // Allocate new controller structure
    if (!g_controller) {
        g_controller = (SDL_GameController*)malloc(sizeof(SDL_GameController));
        if (!g_controller) {
            SDL_JoystickClose(joystick);
            return null_ptr;
        }
    }
    
    g_controller->joystick = joystick;
    g_controller->ref_count = 1;
    g_controller->index = joystick_index;
    
    return g_controller;
}

// Close a game controller
void SDL_GameControllerClose(SDL_GameController *gamecontroller) {
    if (!gamecontroller) {
        return;
    }
    
    gamecontroller->ref_count--;
    if (gamecontroller->ref_count <= 0) {
        if (gamecontroller->joystick) {
            SDL_JoystickClose(gamecontroller->joystick);
            gamecontroller->joystick = null_ptr;
        }
    }
}

// Get button state (simplified mapping)
Uint8 SDL_GameControllerGetButton(SDL_GameController *gamecontroller, SDL_GameControllerButton button) {
    if (!gamecontroller || !gamecontroller->joystick) {
        return 0;
    }
    
    SDL_Joystick *joystick = gamecontroller->joystick;
    
    // Map controller buttons to joystick buttons
    // Saturn controller has 12 buttons typically
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A:
            return SDL_JoystickGetButton(joystick, 0);
        case SDL_CONTROLLER_BUTTON_B:
            return SDL_JoystickGetButton(joystick, 1);
        case SDL_CONTROLLER_BUTTON_X:
            return SDL_JoystickGetButton(joystick, 2);
        case SDL_CONTROLLER_BUTTON_Y:
            return SDL_JoystickGetButton(joystick, 3);
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return SDL_JoystickGetButton(joystick, 4);
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return SDL_JoystickGetButton(joystick, 5);
        case SDL_CONTROLLER_BUTTON_START:
            return SDL_JoystickGetButton(joystick, 8);
        case SDL_CONTROLLER_BUTTON_BACK:
            return SDL_JoystickGetButton(joystick, 9);
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            // Check hat or axis
            {
                int hat = SDL_JoystickGetHat(joystick, 0);
                return (hat & SDL_HAT_UP) ? 1 : 0;
            }
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            {
                int hat = SDL_JoystickGetHat(joystick, 0);
                return (hat & SDL_HAT_DOWN) ? 1 : 0;
            }
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            {
                int hat = SDL_JoystickGetHat(joystick, 0);
                return (hat & SDL_HAT_LEFT) ? 1 : 0;
            }
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            {
                int hat = SDL_JoystickGetHat(joystick, 0);
                return (hat & SDL_HAT_RIGHT) ? 1 : 0;
            }
        default:
            return 0;
    }
}

// Get axis state
Sint16 SDL_GameControllerGetAxis(SDL_GameController *gamecontroller, SDL_GameControllerAxis axis) {
    if (!gamecontroller || !gamecontroller->joystick) {
        return 0;
    }
    
    SDL_Joystick *joystick = gamecontroller->joystick;
    
    // Map controller axes to joystick axes
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
            return SDL_JoystickGetAxis(joystick, 0);
        case SDL_CONTROLLER_AXIS_LEFTY:
            return SDL_JoystickGetAxis(joystick, 1);
        case SDL_CONTROLLER_AXIS_RIGHTX:
            return SDL_JoystickGetAxis(joystick, 2);
        case SDL_CONTROLLER_AXIS_RIGHTY:
            return SDL_JoystickGetAxis(joystick, 3);
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            return SDL_JoystickGetAxis(joystick, 4);
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            return SDL_JoystickGetAxis(joystick, 5);
        default:
            return 0;
    }
}

// Get controller name
const char *SDL_GameControllerName(SDL_GameController *gamecontroller) {
    if (!gamecontroller || !gamecontroller->joystick) {
        return null_ptr;
    }
    return SDL_JoystickName(gamecontroller->index);
}

// Get name for joystick index
const char *SDL_GameControllerNameForIndex(int joystick_index) {
    if (joystick_index < 0 || joystick_index >= SDL_NumJoysticks()) {
        return null_ptr;
    }
    return "Saturn Controller";
}

// Update controller state (called each frame)
void SDL_GameControllerUpdate(void) {
    SDL_JoystickUpdate();
}

// Get underlying joystick
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller) {
    if (!gamecontroller) {
        return null_ptr;
    }
    return gamecontroller->joystick;
}

// Check if controller is attached
SDL_bool SDL_GameControllerGetAttached(SDL_GameController *gamecontroller) {
    if (!gamecontroller || !gamecontroller->joystick) {
        return SDL_FALSE;
    }
    return SDL_JoystickOpened(gamecontroller->index) ? SDL_TRUE : SDL_FALSE;
}

// Event filter (no-op)
int SDL_GameControllerEventState(int state) {
    (void)state;
    return 0;
}

// Cleanup
void SDL_GameControllerQuit(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        free(g_controller);
        g_controller = null_ptr;
    }
    
    if (g_joystick_initialized) {
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        g_joystick_initialized = 0;
    }
}
