#ifndef SDL_INPUT_H
#define SDL_INPUT_H

#include "interfaces/playerInput.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>

class SDL2Input : public PlayerInput {
public:
    SDL2Input();
    ~SDL2Input() override;
    void readInput() override;
private:
    SDL_GameController* gameController;
};

#endif // SDL_INPUT_H