#include "object.h"
#include "sprite_data.h"
#include "tile.h"

#include <iostream>

Object::Object( Vec2 pos, ObjectType type, std::string ID)
    : position(pos), type(type), dir(FacingDirection::EAST), ObjID(ID)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

// Animation methods implementation
void Object::updateAnimation(float deltaTime) {
    animController.update(deltaTime, dir);
}

void Object::setAnimationState(AnimationState state) {
    animController.setState(state);
}

int Object::getCurrentSpriteIndex() const {
    int index = animController.getCurrentFrame(dir);
    if(getCurrentSpriteData()->getid_() == "player_walking")
    return index;
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

void Object::addSpriteSheet(AnimationState state, SpriteData* spData, uint32_t frameTime, bool loop, int startFrame) {
    spriteSheets[state] = spData;
    AnimationDef def(0, spData->columns, spData->columns, frameTime, loop);
    animController.addAnimation(state, def);
}

const SpriteData* Object::getCurrentSpriteData() const {
    auto it = spriteSheets.find(animController.getCurrentState());
    if (it != spriteSheets.end()) {
        return it->second;
    }
    auto defaultIt = spriteSheets.find(AnimationState::IDLE);
    if (defaultIt != spriteSheets.end()) {
        return defaultIt->second;
    }
    std::cout << "No sprite data found for the current state: " << static_cast<int>(animController.getCurrentState()) << std::endl;
    return nullptr; // No sprite data found for the current state
}


Actor::Actor(Vec2 pos, const SpriteData* spData, uint32_t spriteIndex) : spriteIndex(spriteIndex)
{
    this->position = pos;
    this->spriteData = spData;
}
