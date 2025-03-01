#include "sdl_input.h"
#include "logger.h"

SDLInput::SDLInput()
{

}

void SDLInput::read() {
    SDL_PumpEvents();

    const bool* state = SDL_GetKeyboardState(NULL);


    up = state[SDL_SCANCODE_W];
    down = state[SDL_SCANCODE_S];
    left = state[SDL_SCANCODE_A];
    right = state[SDL_SCANCODE_D];
    action = state[SDL_SCANCODE_K];
    jump = state[SDL_SCANCODE_SPACE];
    dash = state[SDL_SCANCODE_J];
    reset = state[SDL_SCANCODE_ESCAPE];
    start = state[SDL_SCANCODE_RETURN];

}
