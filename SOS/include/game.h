// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include "object.h"
#include <iostream>

#include "interfaces/playerInput.h"
#include "objects/player.h"
#include "objects/platform.h"
#include "collision/CollisionManager.h"

class Game {
public:
    Game(PlayerInput* input);
    ~Game();
    bool isRunning() const;

    void update(uint64_t deltaTime);
    std::vector<Object*>& getObjects();
    std::vector<Actor*>& getActors();


private:
    void drawWord(const std::string& word, int x, int y);
    void mapCharacters();

    bool running;
    std::vector<Object*> objects;
    std::vector<Actor*> actors; //Non-interactive objects i.e. text, background, etc.
    PlayerInput* input;
    CollisionManager* collisionManager;
    Player* player;
    
    SpriteData* characters;
    std::map<char, int> characterMap;
};

#endif // GAME_H