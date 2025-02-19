#include "sdl_gfx.h"

SDL_GFX::SDL_GFX() {

}

void SDL_GFX::initialize() {
    entityList.clear();
}

void SDL_GFX::sendSprite(uint16_t* spriteData, int spriteDataCount)
{
    entityList.clear();
    for (int i = 0; i < spriteDataCount; i++) {
        entityData temp;
        temp.x = spriteData[i];
        temp.y = spriteData[i + 1];
        temp.ID = spriteData[i + 2];
        entityList.push_back(temp);
    }
}

std::vector<entityData> SDL_GFX::getEntityList() {
    return entityList;
}