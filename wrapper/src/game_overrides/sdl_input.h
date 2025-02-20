#ifndef SDL_INPUT_H
#define SDL_INPUT_H

#include "input.h"
#include <SDL3/SDL.h>

class SDLInput : public Input {
public:
    SDLInput();
    void read() override;
};

#endif // SDL_INPUT_H