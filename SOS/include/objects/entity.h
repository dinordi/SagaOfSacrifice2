#pragma once
#include "object.h"
#include "sprite_data.h"

class Entity : public Object
{
public:
    Entity( Vec2 pos, SpriteData* spData, std::string objID);

};