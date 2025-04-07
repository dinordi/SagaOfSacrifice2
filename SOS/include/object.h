// SOS1/include/object.h

// #ifndef OBJECT_H
// #define OBJECT_H
#pragma once

#include <Vec2.h>
#include "sprite_data.h"

#define DEFINE_GETTER_SETTER(type, member) \
private:                                   \
    type member;                           \
public:                                    \
    type& get##member() { return member; } \
    void set##member(type& value) { member = value; }


class Platform;

constexpr float GRAVITY = 1.0f;

enum class ObjectType {
    ENTITY,
    PLATFORM,
    ITEM,
    BULLET
};

class Object {
public:
    Vec2 position;
    Vec2 velocity;
    const ObjectType type;
    const SpriteData* spriteData;
    Object( Vec2 pos, ObjectType type, SpriteData* spData);

    virtual void update(uint64_t deltaTime) = 0;
    virtual bool collisionWith(Object* other) = 0;
    virtual void handlePlatformCollision(Platform* platform);
private:
    // DEFINE_GETTER_SETTER(Vec2, position);
};

// #endif // OBJECT_H