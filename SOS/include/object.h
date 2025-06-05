// SOS1/include/object.h

#pragma once
#include <map>
#include <unordered_map>
#include <mutex>
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

enum class ActorType {
    TEXT,
    HEALTHBAR
};

inline std::ostream& operator<<(std::ostream& os, ObjectType type) {
    switch (type) {
        case ObjectType::PLAYER: os << "PLAYER"; break;
        case ObjectType::TILE: os << "TILE"; break;
        case ObjectType::ITEM: os << "ITEM"; break;
        case ObjectType::BULLET: os << "BULLET"; break;
        case ObjectType::MINOTAUR: os << "MINOTAUR"; break;
    }
    return os;
}

class BoxCollider {
public:
BoxCollider(Vec2 pos, Vec2 size);
BoxCollider(float x, float y, float width, float height);
    Vec2 position;
    Vec2 size;
};

class Object {
public:
        static uint16_t getNextObjectID() {
            std::lock_guard<std::mutex> lock(countmutex); // Ensure thread-safe access
            return objectCount++;
        }
        static uint16_t getObjectCount() {
            std::lock_guard<std::mutex> lock(countmutex); // Ensure thread-safe access
            return objectCount;
        }
    private:
    static uint16_t objectCount; // Static counter to assign unique IDs to objects
    static std::mutex countmutex; // Mutex to protect access to objectCount
public:
    const ObjectType type;
    Object(BoxCollider collider, ObjectType type, uint16_t ID);
    virtual ~Object() = default;

    virtual void update(float deltaTime) = 0;
    virtual void accept(CollisionVisitor& visitor) = 0;
    virtual bool isCollidable() const { return true; } // Default to collidable
    
    void addSpriteSheet(AnimationState state, std::string tpsheet, uint32_t frameTime = 150);
    const SpriteData* getCurrentSpriteData() const;

    // Animation methods
    void updateAnimation(float deltaTime);  //Time in seconds
    void setAnimationState(AnimationState state);
    AnimationState getAnimationState() const { return animController.getCurrentState(); }
    virtual int getCurrentSpriteIndex() const;  //Virtual function because Tile returns a static index

    void addAnimation(AnimationState state, int frameCount, 
                     uint32_t frameTime = 100, bool loop = true);


    FacingDirection getDir() const { return dir; }
    void setDir(FacingDirection direction) { dir = direction; }

    Vec2 getposition() const { return collider.position; }
    void setposition(const Vec2& pos) { collider.position = pos; }

protected:
    AnimationController animController;
    FacingDirection dir;
    
    
private:
    DEFINE_GETTER_SETTER(BoxCollider, collider);
    DEFINE_GETTER_SETTER(Vec2, velocity);
    DEFINE_CONST_GETTER_SETTER(uint16_t, ObjID); // ID of the object, for multiplayer to indicate between players and objects
};

class Actor {
public:
    Actor(Vec2 pos, std::string tpsheet, uint16_t defaultIndex, ActorType type = ActorType::TEXT);
    const SpriteData* getCurrentSpriteData() const;
private:
    DEFINE_GETTER_SETTER(Vec2, position);
    DEFINE_GETTER_SETTER(uint16_t, defaultIndex);
    DEFINE_CONST_GETTER_SETTER(ActorType, type);
    DEFINE_GETTER_SETTER(std::string, tpsheet);
    DEFINE_GETTER_SETTER(uint16_t, ObjID);

    static uint16_t actorCount; // Static counter to assign unique IDs to actors
};