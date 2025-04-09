#pragma once
#include "object.h"
#include "sprite_data.h"

class Entity : public Object
{
public:
    Entity( Vec2 pos, SpriteData* spData);
    // void handlePlatformCollision(Platform* platform) override;

private:
    DEFINE_GETTER_SETTER(bool, isOnGround);
};