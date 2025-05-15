#include "objects/platform.h"

Platform::Platform(int x, int y, SpriteData* spData, std::string objID) : Object(Vec2(x, y), ObjectType::PLATFORM, spData, objID) {
    // Initialize platform-specific attributes here
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