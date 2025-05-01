#include "sprite_data.h"


SpriteData::SpriteData(std::string id, int width, int height, int columns)
: id_(id), width(width), height(height), columns(columns) 
{

}

SpriteRect SpriteData::getSpriteRect(int index) const {
    int x = (index % columns) * width;
    int y = (index / columns) * height;
    return  SpriteRect(x, y, width, height, id_);
}