// SOS1/include/object.h

#pragma once
#include <map>

#include <Vec2.h>
#include "sprite_data.h"
#include "collision/CollisionVisitor.h"
#include "animation.h"


class Platform;

constexpr float MAX_VELOCITY = 15.0f;

enum class ObjectType {
    PLAYER = 0x1,
    ENTITY,
    PLATFORM,
    ITEM,
    BULLET,
    ENEMY
};

enum class FacingDirection {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Object {
public:
    // Vec2 position;
    // Vec2 velocity;
    const ObjectType type;
    const SpriteData* spriteData;
    Object(Vec2 pos, ObjectType type, SpriteData* spData, std::string ID);
    virtual ~Object() = default;

    virtual void update(float deltaTime) = 0;
    virtual void accept(CollisionVisitor& visitor) = 0;
    
    // Animation methods
    void updateAnimation(float deltaTime);  //Time in seconds
    void setAnimationState(AnimationState state);
    int getCurrentSpriteIndex() const;
    void addAnimation(AnimationState state, int startFrame, int frameCount, 
                     int framesPerRow, uint32_t frameTime = 100, bool loop = true);
    FacingDirection getDir() const { return dir; }
protected:
    AnimationController animController;
    FacingDirection dir;
    
private:
    DEFINE_GETTER_SETTER(Vec2, position);
    DEFINE_GETTER_SETTER(Vec2, velocity);
    DEFINE_CONST_GETTER_SETTER(std::string, ObjID); // ID of the object, for multiplayer to indicate between players and objects
};

class Actor {
public:
    const SpriteData* spriteData;
    const int spriteIndex;  //Current image in spritesheet, SpriteData handles index to srcrect
    Actor(Vec2 pos, const SpriteData* spData, uint32_t spriteIndex = 1);
private:
    DEFINE_GETTER_SETTER(Vec2, position);
};