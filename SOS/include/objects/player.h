#pragma once

#include "objects/entity.h"
#include "interfaces/sprite_data.h"
#include "objects/platform.h"

class Player : public Entity {

public:
    Player( Vec2 pos, SpriteData* spData );
    void update(uint64_t deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    void takeDamage(int amount);
    void collectItem();
    void applyPhysicsResponse(const Vec2& resolutionVector);
};