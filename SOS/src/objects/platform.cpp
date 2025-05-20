#include "objects/platform.h"

Platform::Platform(int x, int y, std::string objID) : Object(Vec2(x, y), ObjectType::PLATFORM, objID) {
    // Initialize platform-specific attributes here
    // platformType = PlatformType::GROUND; // Default type
    addSpriteSheet(AnimationState::IDLE, new SpriteData("Tilemap_Flat", 64, 64, 12));
    addAnimation(AnimationState::IDLE, 5, 1, 12, 250, true); // Idle animation (1 frames)
}

// bool Platform::collisionWith(Object* other) {
//     // Implement collision detection logic here
//     return false; // Placeholder
// }

void Platform::update(float deltaTime) {

}

void Platform::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Platform::setPlatformType(PlatformType type) {
    platformType = type;
}

PlatformType Platform::getPlatformType() const {
    return platformType;
}