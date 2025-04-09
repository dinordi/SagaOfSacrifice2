// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include "object.h"
#include <iostream>

#include "interfaces/playerInput.h"
#include "objects/player.h"
#include "objects/platform.h"

class Game {
public:
    Game(PlayerInput* input);
    ~Game();

    void update(uint64_t deltaTime);
    void render();
    bool isRunning() const;
    std::vector<Object*>& getObjects();

private:
    bool running;
    std::vector<Object*> objects;
    PlayerInput* input;
};

#endif // GAME_H