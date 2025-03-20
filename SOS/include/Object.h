// SOS1/include/Object.h

#ifndef OBJECT_H
#define OBJECT_H

#include <Vec2.h>
#include "sprite_data.h"

constexpr float GRAVITY = 1.0f;

class Object {
public:
    Vec2 position;
    Vec2 velocity;
    Object(int ID, int x, int y);
    
    SpriteData* getSpriteData() { return spriteData; }
    void setSpriteData(SpriteData* sprite);

    virtual void update(uint64_t deltaTime) = 0;
    virtual bool collisionWith(Object* other) = 0;
private:
    SpriteData* spriteData;
};

#endif // OBJECT_H