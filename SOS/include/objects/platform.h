#pragma once

#include "object.h"
#include "sprite_data.h"

enum class PlatformType {
    GROUND,
    WALL,
    STAIRS,
    OBSTACLE
};

class Platform : public Object {
public:
    Platform(int x, int y, std::string objID);
    // Platform(int ID, int x, int y);
    void update(float deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    bool isBreakable() const;
    
    PlatformType getPlatformType() const;
    void setPlatformType(PlatformType type);
    
    bool hasFlag(uint32_t flag) const { return (collisionFlags & flag) != 0; }
    void setFlag(uint32_t flag) { collisionFlags |= flag; }
    void clearFlag(uint32_t flag) { collisionFlags &= ~flag; }

    // Collision flags
    static const uint32_t BLOCKS_HORIZONTAL = 0x00000001;
    static const uint32_t BLOCKS_VERTICAL = 0x00000002;
    static const uint32_t REDUCES_SPEED = 0x00000004;  // Mud or similar
    static const uint32_t ALLOWS_CLIMBING = 0x00000008; // Stairs
    static const uint32_t BLOCKS_PROJECTILES = 0x00000010;


private:
    PlatformType platformType;
    uint32_t collisionFlags = 0;
};