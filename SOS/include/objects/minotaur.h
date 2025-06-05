#pragma once

#include <string>
#include "objects/enemy.h"



class Minotaur : public Enemy
{
public:
    Minotaur(BoxCollider collider, uint16_t objID, int layer = 8);
    Minotaur(int x, int y, uint16_t objID);
    ~Minotaur() override;
    void setupAnimations(std::filesystem::path atlasPath);

    void move() override;
    void attack() override;
    void die() override;
    void update(float deltaTime) override;
};