#pragma once
#include "gfx.h"
#include <vector>
#include "logger.h"

#include "sprite_data.h"


class SDL_GFX : public GFX {
    public:
        SDL_GFX();
        void initialize();
        void sendSprite(uint16_t* spriteData, int spriteDataCount);
        void sendSprite2(std::vector<SpriteData*>& spriteList);
        std::vector<SpriteData*> getEntityList();
    private:
        std::vector<SpriteData*> entityList;
};