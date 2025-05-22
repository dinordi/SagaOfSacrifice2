// SOS1/include/object.h

#pragma once
#include <map>
#include <unordered_map>

#include <Vec2.h>
#include "sprite_data.h"
#include "collision/CollisionVisitor.h"
#include "animation.h"


class Tile;

constexpr float MAX_VELOCITY = 200.0f;

enum class ObjectType {
    PLAYER = 0x1,
    TILE,
    ITEM,
    BULLET,
    MINOTAUR
};

class BoxCollider {
public:
BoxCollider(Vec2 pos, Vec2 size);
BoxCollider(float x, float y, float width, float height);
    Vec2 position;
    Vec2 size;
};

class Object {
public:
    const ObjectType type;
    Object(BoxCollider collider, ObjectType type, std::string ID);
    virtual ~Object() = default;

    virtual void update(float deltaTime) = 0;
    virtual void accept(CollisionVisitor& visitor) = 0;
    
    void addSpriteSheet(AnimationState state, SpriteData* spData, uint32_t frameTime, bool loop, int startFrame = 0);
    const SpriteData* getCurrentSpriteData() const;

    // Animation methods
    void updateAnimation(float deltaTime);  //Time in seconds
    void setAnimationState(AnimationState state);
    int getCurrentSpriteIndex() const;
    void addAnimation(AnimationState state, int startFrame, int frameCount, 
                     int framesPerRow, uint32_t frameTime = 100, bool loop = true);
    FacingDirection getDir() const { return dir; }

    Vec2 getposition() const { return collider.position; }
    void setposition(const Vec2& pos) { collider.position = pos; }

protected:
    AnimationController animController;
    FacingDirection dir;
    
    std::unordered_map<AnimationState, SpriteData*> spriteSheets;
private:
    DEFINE_GETTER_SETTER(BoxCollider, collider);
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