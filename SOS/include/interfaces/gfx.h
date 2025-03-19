#pragma once
#include <cstdint>
#include <vector>

struct SpriteData;

class GFX 
{
public:
    virtual ~GFX() = default;

    virtual void initialize() = 0;
    virtual void sendSprites(std::vector<SpriteData*>& spriteList) = 0;
    virtual std::vector<SpriteData*> getEntityList() = 0;
};