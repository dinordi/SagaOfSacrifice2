#pragma once

#include "objects/entity.h"
#include "interfaces/sprite_data.h"
#include "objects/platform.h"

class Player : public Entity {

public:
    Player( Vec2 pos, SpriteData* spData );
    bool collisionWith(Object* other) override;
    void update(uint64_t deltaTime) override;
};