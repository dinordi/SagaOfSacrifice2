#include "objects/platform.h"

Platform::Platform(int ID, int x, int y) : Object(ID, x, y) {
    // Initialize platform-specific attributes here
    this->position.x = x;
    this->position.y = y;

    SpriteData* sprite = new SpriteData();
    sprite->ID = ID;
    sprite->width = 800; // Example width
    sprite->height = 127; // Example height
    this->setSpriteData(sprite);
}

bool Platform::collisionWith(Object* other) {
    // Implement collision detection logic here
    return false; // Placeholder
}

void Platform::update(uint64_t deltaTime) {

}