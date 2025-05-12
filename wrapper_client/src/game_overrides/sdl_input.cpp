#include "sdl_input.h"
#include "logger.h"

SDLInput::SDLInput(SDL_Gamepad* gamepad) : dualsense(gamepad)
{
    // Initialize all button states
    set_up(false);
    set_last_up(false);
    set_down(false);
    set_last_down(false);
    set_left(false);
    set_last_left(false);
    set_right(false);
    set_last_right(false);
    set_attack(false);
    set_last_attack(false);
}

void SDLInput::readInput() {
    SDL_PumpEvents();
    
    // Current button states
    bool up_pressed = false;
    bool down_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool attack_pressed = false;
    
    // Read inputs from either gamepad or keyboard
    if(dualsense) {
        // Gamepad controls
        up_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTY) < -8000 || 
                    SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_UP);
        down_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTY) > 8000 || 
                      SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        left_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTX) < -8000 || 
                      SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        right_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTX) > 8000 || 
                       SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        attack_pressed = SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_WEST);
    }
    else {
        // Keyboard controls
        const bool* state = SDL_GetKeyboardState(NULL);
        up_pressed = state[SDL_SCANCODE_W];
        down_pressed = state[SDL_SCANCODE_S];
        left_pressed = state[SDL_SCANCODE_A];
        right_pressed = state[SDL_SCANCODE_D];
        attack_pressed = state[SDL_SCANCODE_K];
    }
    
    
    // Handle directional movement (continuous while held)
    set_left(left_pressed);
    set_right(right_pressed);
    
    // Handle attack (triggered only on press, not hold)
    if(attack_pressed && !get_last_attack()) {
        // Logger::getInstance()->log("Attack pressed");
        set_attack(true);
    } else {
        set_attack(false);
    }
    set_last_attack(attack_pressed);
}