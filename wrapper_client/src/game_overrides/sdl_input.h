#ifndef SDL_INPUT_H
#define SDL_INPUT_H

#include "interfaces/playerInput.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>

class SDLInput : public PlayerInput {
public:
    SDLInput(SDL_Gamepad* gamepad);
    void readInput() override;
private:
    SDL_Gamepad* dualsense;
};

#endif // SDL_INPUT_H