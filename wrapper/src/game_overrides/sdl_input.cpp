#include "sdl_input.h"
#include "logger.h"

SDLInput::SDLInput(SDL_Gamepad* gamepad) : dualsense(gamepad)
{
    // Initialize all button states
    set_jump(false);
    set_last_jump(false);
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
    bool jump_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool attack_pressed = false;
    
    // Read inputs from either gamepad or keyboard
    if(dualsense) {
        // Gamepad controls
        jump_pressed = SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_SOUTH);
        left_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTX) < -8000 || 
                      SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        right_pressed = SDL_GetGamepadAxis(dualsense, SDL_GAMEPAD_AXIS_LEFTX) > 8000 || 
                       SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        attack_pressed = SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_WEST);
    }
    else {
        // Keyboard controls
        const bool* state = SDL_GetKeyboardState(NULL);
        jump_pressed = state[SDL_SCANCODE_SPACE];
        left_pressed = state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT];
        right_pressed = state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT];
        attack_pressed = state[SDL_SCANCODE_K];
    }
    
    // Handle jump (triggered only on press, not hold)
    if(jump_pressed && !get_last_jump()) {
        // Logger::getInstance()->log("Jump pressed");
        set_jump(true);
    } else {
        set_jump(false);
    }
    set_last_jump(jump_pressed);
    
    // Handle directional movement (continuous while held)
    set_left(left_pressed);
    set_right(right_pressed);
    
    // Handle attack (triggered only on press, not hold)
    if(attack_pressed && !get_last_attack()) {
        Logger::getInstance()->log("Attack pressed");
        set_attack(true);
    } else {
        set_attack(false);
    }
    set_last_attack(attack_pressed);
}