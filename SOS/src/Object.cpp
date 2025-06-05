#include "object.h"
#include "sprite_data.h"
#include "tile.h"

#include <iostream>
#include <mutex>

uint16_t Object::objectCount = 0; // Initialize static object count
std::mutex Object::countmutex; // Define the static mutex

Object::Object(BoxCollider collider, ObjectType type, uint16_t ID)
    : collider(collider), type(type), dir(FacingDirection::EAST), ObjID(ID)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

// Animation methods implementation
void Object::updateAnimation(float deltaTime) {
    if(type == ObjectType::TILE) {
        // Tiles do not have animations, return early
        return;
    }
    animController.update(static_cast<uint64_t>(deltaTime), dir);
}

void Object::setAnimationState(AnimationState state) {
    animController.setState(state);
}

int Object::getCurrentSpriteIndex() const {
    int index = animController.getCurrentFrame(dir);
    return index;
}

/*
* Adds an animation definition to the animation controller.
* @param state The animation state to associate with this animation.
* @param frameCount The number of frames in this animation.
* @param frameTime The time each frame is displayed in milliseconds.
* @param loop Whether the animation should loop or not.
* @note This function allows you to define multiple animations for different states of the object.
*       For example, you can have different animations for walking, jumping, etc.
*/
void Object::addAnimation(AnimationState state, int frameCount, 
                         uint32_t frameTime, bool loop) {
    AnimationDef def(frameCount, frameTime, loop);
    animController.addAnimation(state, def);
}

void Object::addSpriteSheet(AnimationState state, std::string tpsheet, uint32_t frameTime) {
    animController.addSpriteSheet(tpsheet, state, frameTime);
}

const SpriteData* Object::getCurrentSpriteData() const {
    SpriteData* spData = animController.getCurrentSpriteData();
    if (spData) {
        return spData; // Return the current sprite data
    }
    return nullptr; // No sprite data found for the current state
}

uint16_t Actor::actorCount = 0; // Initialize static actor count
Actor::Actor(Vec2 pos, std::string tpsheet, uint16_t defaultIndex, ActorType type) : position(pos), type(type), tpsheet(tpsheet), defaultIndex(defaultIndex)
{
    ObjID = actorCount++;
}

const SpriteData* Actor::getCurrentSpriteData() const
{
    return SpriteData::getSharedInstance(tpsheet);
}

BoxCollider::BoxCollider(Vec2 pos, Vec2 size) : position(pos), size(size) {}
BoxCollider::BoxCollider(float x, float y, float width, float height) 
    : position(x, y), size(width, height) {}