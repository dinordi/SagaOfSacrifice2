// SOS1/include/object.h

// #ifndef OBJECT_H
// #define OBJECT_H
#pragma once
#include <map>

#include <Vec2.h>
#include "sprite_data.h"
#include "collision/CollisionVisitor.h"


class Platform;

constexpr float GRAVITY = 9.8f;
constexpr float MAX_VELOCITY = 15.0f;

enum class ObjectType {
    ENTITY,
    PLATFORM,
    ITEM,
    BULLET,
    ENEMY,
    PLAYER
};

class Object {
public:
    // Vec2 position;
    // Vec2 velocity;
    const ObjectType type;
    const SpriteData* spriteData;
    Object( Vec2 pos, ObjectType type, SpriteData* spData);

    virtual void update(uint64_t deltaTime) = 0;
    virtual void accept(CollisionVisitor& visitor) = 0;
private:
    DEFINE_GETTER_SETTER(Vec2, position);
    DEFINE_GETTER_SETTER(Vec2, velocity);
};

class Actor {
public:
    const SpriteData* spriteData;
    const int spriteIndex;  //Current image in spritesheet, SpriteData handles index to srcrect
    Actor(Vec2 pos, const SpriteData* spData, uint spriteIndex = 1);
private:
    DEFINE_GETTER_SETTER(Vec2, position);
};

// #endif // OBJECT_H