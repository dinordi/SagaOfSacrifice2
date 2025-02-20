#include "sdl_gfx.h"

SDL_GFX::SDL_GFX() {

}

void SDL_GFX::initialize() {
    entityList.clear();
}

void SDL_GFX::sendSprite(uint16_t* spriteData, int spriteDataCount)
{
    entityList.clear();
    for (int i = 0; i < spriteDataCount; i += 3) {
        SpriteData* temp = new SpriteData();
        temp->ID = spriteData[i];
        temp->x = spriteData[i + 1];
        temp->y = spriteData[i + 2];
        entityList.push_back(temp);
    }
}

void SDL_GFX::sendSprite2(std::vector<SpriteData*>& spriteList)
{
    entityList.clear();
    for (auto& sprite : spriteList) {
        // Logger::getInstance()->log("sdl_gfx-ID: " + std::to_string(sprite->ID));
        entityList.push_back(sprite);
    }
}

std::vector<SpriteData*> SDL_GFX::getEntityList() {
    return entityList;
}