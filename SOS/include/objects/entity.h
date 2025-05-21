#pragma once
#include "object.h"
#include "sprite_data.h"

class Entity : public Object
{
public:
    Entity( BoxCollider collider, std::string objID);

};