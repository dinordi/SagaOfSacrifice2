#include "object.h"
#include "sprite_data.h"
#include "platform.h"

Object::Object( Vec2 pos, ObjectType type, SpriteData* spData, std::string ID)
    : position(pos), type(type), spriteData(spData), dir(FacingDirection::RIGHT), ObjID(ID)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

// Animation methods implementation
void Object::updateAnimation(float deltaTime) {
    animController.update(deltaTime);
}

void Object::setAnimationState(AnimationState state) {
    animController.setState(state);
}

int Object::getCurrentSpriteIndex() const {
    return animController.getCurrentFrame();
}

/*
* Adds an animation definition to the animation controller.
* @param state The animation state to associate with this animation.
* @param startFrame The starting frame index in the sprite sheet.
* @param frameCount The number of frames in this animation.
* @param framesPerRow The number of frames per row in the sprite sheet.
* @param frameTime The time each frame is displayed in milliseconds.
* @param loop Whether the animation should loop or not.
* @note This function allows you to define multiple animations for different states of the object.
*       For example, you can have different animations for walking, jumping, etc.
*/
void Object::addAnimation(AnimationState state, int startFrame, int frameCount, 
                         int framesPerRow, uint32_t frameTime, bool loop) {
    AnimationDef def(startFrame, frameCount, framesPerRow, frameTime, loop);
    animController.addAnimation(state, def);
}

// void Object::handlePlatformCollision(Platform* platform) {
    // virtual Method 
// }


Actor::Actor(Vec2 pos, const SpriteData* spData, uint32_t spriteIndex) : spriteIndex(spriteIndex)
{
    this->position = pos;
    this->spriteData = spData;
}
