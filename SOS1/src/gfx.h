#pragma once
#include <cstdint>
#include <vector>

struct SpriteData;

class GFX 
{
public:
    virtual ~GFX() = default;

    virtual void initialize() = 0;
    virtual void sendSprite(uint16_t* spriteData, int spriteDataCount) = 0;
    virtual void sendSprite2(std::vector<SpriteData*>& spriteList) = 0;
    virtual std::vector<SpriteData*> getEntityList() = 0;
};