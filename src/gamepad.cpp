/*
 * Saturn-compatible gamepad implementation
 * Simplified version using SDL1 joystick API directly
 * Replaces Linux evdev-based implementation
 */

#include <SDL.h>
#include <srl.hpp>
#include <srl_memory.hpp>  // for memory allocation
#include <srl_input.hpp>   // for Digital input
#include <srl_log.hpp>     // for logging
#include "gamepad.h"

using namespace SRL::Input;

static int g_joystick_initialized = 0;

bool initGamepad() {
    gamepad = new Digital(0);
    if(gamepad == nullptr) {
        SRL::Logger::LogFatal("[GAMEPAD] Failed to initialize gamepad");
        return false;
    } else {
        SRL::Logger::LogInfo("[GAMEPAD] Gamepad initialized successfully");
    }

    if (gamepad->IsConnected()) {   
        SRL::Logger::LogInfo("[GAMEPAD] Gamepad is connected");
        return true;
    } else {
        SRL::Logger::LogWarning("[GAMEPAD] No gamepad detected");
        return false;
    }
}

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


// Get button state using SRL Digital input
Uint8 SDL_GameControllerGetButton(SRL::Input::Digital *gamecontroller, SDL_GameControllerButton button) {
    if (!gamecontroller || !gamecontroller->IsConnected()) {
        return 0;
    }
    
    // Map SDL button enum to SRL Digital button
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::A) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_B:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::B) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_X:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::X) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_Y:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Y) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::L) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::R) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_START:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::START) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_BACK:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Z) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Up) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Down) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Left) ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return gamecontroller->IsHeld(SRL::Input::Digital::Button::Right) ? 1 : 0;
        default:
            return 0;
    }
}


