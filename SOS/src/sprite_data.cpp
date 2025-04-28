#include "sprite_data.h"


SpriteData::SpriteData(int ID, int width, int height, int columns)
: ID(ID), width(width), height(height), columns(columns) 
{

}

const SpriteRect& SpriteData::getSpriteRect(int index) const {
    int x = (index % columns) * width;
    int y = (index / columns) * height;
    return  SpriteRect(x, y, width, height, ID);
}