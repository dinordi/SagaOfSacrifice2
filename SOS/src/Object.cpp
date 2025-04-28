#include "object.h"
#include "sprite_data.h"
#include "platform.h"

Object::Object( Vec2 pos, ObjectType type, SpriteData* spData)
    : position(pos), type(type), spriteData(spData)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

// void Object::handlePlatformCollision(Platform* platform) {
    // virtual Method 
// }


Actor::Actor(Vec2 pos, const SpriteData* spData, int spriteIndex) : spriteIndex(spriteIndex)
{
    this->position = pos;
    this->spriteData = spData;
}
