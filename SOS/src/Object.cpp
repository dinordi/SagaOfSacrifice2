#include "Object.h"
#include "interfaces/sprite_data.h"

Object::Object(int ID, int x, int y)
{
    // this->spriteData = new SpriteData();
    // this->spriteData->ID = ID;

    // this->spriteData->width = 32; // Example width
    // this->spriteData->height = 32; // Example height

}

void Object::setSpriteData(SpriteData* sprite)
{
    this->spriteData = sprite;
}