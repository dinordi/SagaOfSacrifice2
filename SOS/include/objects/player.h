#pragma once

#include "Object.h"
#include "interfaces/sprite_data.h"
#include "objects/platform.h"

class Player : public Object {
public:
    Player(int ID, int x, int y);
    bool collisionWith(Object* other) override;
    void update(uint64_t deltaTime) override;
};