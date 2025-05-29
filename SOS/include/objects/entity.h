#pragma once
#include "object.h"
#include "sprite_data.h"

class Entity : public Object
{
public:
    Entity( BoxCollider collider, std::string objID, ObjectType type);
    
    bool isDead() const { return isDead_; }
    
    void update(float deltaTime) override; // Updating animation
    
protected:
    bool isDead_;
};