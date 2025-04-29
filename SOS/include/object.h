// SOS1/include/object.h

#pragma once
#include <map>

#include <Vec2.h>
#include "sprite_data.h"
#include "collision/CollisionVisitor.h"
#include "animation.h"


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
    Object(Vec2 pos, ObjectType type, SpriteData* spData, std::string ID);

    virtual void update(uint64_t deltaTime) = 0;
    virtual void accept(CollisionVisitor& visitor) = 0;
    
    // Animation methods
    void updateAnimation(uint64_t deltaTime);
    void setAnimationState(AnimationState state);
    int getCurrentSpriteIndex() const;
    void addAnimation(AnimationState state, int startFrame, int frameCount, 
                     int framesPerRow, uint32_t frameTime = 100, bool loop = true);
    bool isFacingRight() const { return facingRight; }
protected:
    AnimationController animController;
    bool facingRight;
    
private:
    DEFINE_GETTER_SETTER(Vec2, position);
    DEFINE_GETTER_SETTER(Vec2, velocity);
    DEFINE_CONST_GETTER_SETTER(std::string, ObjID); // ID of the object, for multiplayer to indicate between players and objects
};

class Actor {
public:
    const SpriteData* spriteData;
    const int spriteIndex;  //Current image in spritesheet, SpriteData handles index to srcrect
    Actor(Vec2 pos, const SpriteData* spData, uint spriteIndex = 1);
private:
    DEFINE_GETTER_SETTER(Vec2, position);
};