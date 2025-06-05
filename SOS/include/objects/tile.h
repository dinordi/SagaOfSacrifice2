#pragma once

#include "object.h"
#include "sprite_data.h"


class Tile : public Object {
public:
    Tile(int x, int y, uint16_t objID, std::string tileMap, int tileIndex,
         int tileWidth, int tileHeight, int columns);
    // Tile(int ID, int x, int y);
    void update(float deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    bool isBreakable() const;
    bool isCollidable() const override;
    
    void setupAnimations(std::filesystem::path atlasPath);
    
    int getCurrentSpriteIndex() const override {
        return tileIndex;
    }
    
    bool hasFlag(uint32_t flag) const { return (collisionFlags & flag) != 0; }
    void setFlag(uint32_t flag) { collisionFlags |= flag; }
    void clearFlag(uint32_t flag) { collisionFlags &= ~flag; }
    uint32_t getFlags() const { return collisionFlags; }

    // Collision flags
    static const uint32_t BLOCKS_HORIZONTAL_LEFT = 0x00000001;
    static const uint32_t BLOCKS_VERTICAL_TOP = 0x00000002;
    static const uint32_t REDUCES_SPEED = 0x00000004;  // Mud or similar
    static const uint32_t ALLOWS_CLIMBING = 0x00000008; // Stairs
    static const uint32_t BLOCKS_PROJECTILES = 0x00000010;
    static const uint32_t BLOCKS_HORIZONTAL_RIGHT = 0x00000020;
    static const uint32_t BLOCKS_VERTICAL_BOTTOM = 0x00000020;


private:
    uint32_t collisionFlags = 0;
    DEFINE_CONST_GETTER_SETTER(uint8_t, tileIndex);
    DEFINE_CONST_GETTER_SETTER(std::string, tileMapName);
};