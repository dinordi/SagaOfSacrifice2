#pragma once

#include <string>
#include "objects/enemy.h"
#include <filesystem>



class Minotaur : public Enemy
{
public:
    Minotaur(BoxCollider collider, std::string objID);
    Minotaur(int x, int y, std::string objID);
    ~Minotaur() override;
    void setupAnimations(std::filesystem::path atlasPath);

    void move() override;
    void attack() override;
    void die() override;
    void update(float deltaTime) override;
};