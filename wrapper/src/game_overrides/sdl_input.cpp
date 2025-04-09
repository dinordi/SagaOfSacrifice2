#include "sdl_input.h"
#include "logger.h"

SDLInput::SDLInput(SDL_Gamepad* gamepad) : dualsense(gamepad)
{
    set_jump(false);
    set_last_jump(false);
}

void SDLInput::readInput() {

    SDL_PumpEvents();
    bool jump_pressed = false;

    if(dualsense)
    {
        jump_pressed = SDL_GetGamepadButton(dualsense, SDL_GAMEPAD_BUTTON_SOUTH);
    }
    else
    {
        const bool* state = SDL_GetKeyboardState(NULL);
        jump_pressed = state[SDL_SCANCODE_SPACE];
    }
    
    if(jump_pressed && !get_last_jump())
    {
        Logger::getInstance()->log("Jump pressed");
        set_jump(true);
    }
    else
    {
        set_jump(false);
    }
    set_last_jump(jump_pressed);

    // up = state[SDL_SCANCODE_W];
    // down = state[SDL_SCANCODE_S];
    // left = state[SDL_SCANCODE_A];
    // right = state[SDL_SCANCODE_D];
    // action = state[SDL_SCANCODE_K];
    // jump = state[SDL_SCANCODE_SPACE];
    // dash = state[SDL_SCANCODE_J];
    // reset = state[SDL_SCANCODE_ESCAPE];
    // start = state[SDL_SCANCODE_RETURN];

}
