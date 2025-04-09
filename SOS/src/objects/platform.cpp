#include "objects/platform.h"

Platform::Platform(int ID, int x, int y, SpriteData* spData) : Object(Vec2(x, y), ObjectType::PLATFORM, spData) {
    // Initialize platform-specific attributes here
}

// bool Platform::collisionWith(Object* other) {
//     // Implement collision detection logic here
//     return false; // Placeholder
// }

void Platform::update(uint64_t deltaTime) {

}

void Platform::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}