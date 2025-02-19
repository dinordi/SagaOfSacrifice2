#pragma once
#include <cstdint>
#include <vector>

struct entityData;

class GFX 
{
public:
    virtual ~GFX() = default;

    virtual void initialize() = 0;
    virtual void sendSprite(uint16_t* spriteData, int spriteDataCount) = 0;
    virtual std::vector<entityData> getEntityList() = 0;
};