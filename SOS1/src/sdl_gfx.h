#pragma once
#include "gfx.h"
#include <vector>


typedef struct entityData {
    float x;
    float y;
    int ID;
};


class SDL_GFX : public GFX {
    public:
        SDL_GFX();
        void initialize();
        void sendSprite(uint16_t* spriteData, int spriteDataCount);
        std::vector<entityData> getEntityList();
    private:
        std::vector<entityData> entityList;
};