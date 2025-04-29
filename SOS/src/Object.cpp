#include "object.h"
#include "sprite_data.h"
#include "platform.h"

Object::Object( Vec2 pos, ObjectType type, SpriteData* spData, std::string ID)
    : position(pos), type(type), spriteData(spData), facingRight(true), ObjID(ID)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

// Animation methods implementation
void Object::updateAnimation(uint64_t deltaTime) {
    animController.update(deltaTime);
}

void Object::setAnimationState(AnimationState state) {
    animController.setState(state);
}

int Object::getCurrentSpriteIndex() const {
    return animController.getCurrentFrame();
}

void Object::addAnimation(AnimationState state, int startFrame, int frameCount, 
                         int framesPerRow, uint32_t frameTime, bool loop) {
    AnimationDef def(startFrame, frameCount, framesPerRow, frameTime, loop);
    animController.addAnimation(state, def);
}

// void Object::handlePlatformCollision(Platform* platform) {
    // virtual Method 
// }


Actor::Actor(Vec2 pos, const SpriteData* spData, uint spriteIndex) : spriteIndex(spriteIndex)
{
    this->position = pos;
    this->spriteData = spData;
}
