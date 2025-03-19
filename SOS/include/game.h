// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include "Object.h"
#include <iostream>

class Game {
public:
    Game();
    ~Game();

    void update(float deltaTime);
    void render();
    bool isRunning() const;
    std::vector<Object*>& getObjects();

private:
    bool running;
    std::vector<Object*> objects;
};

#endif // GAME_H