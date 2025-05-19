#include "objects/Enemy.h"
#include <iostream>

Enemy::Enemy( Vec2 pos, std::string objID) : Entity(pos, objID) {
    // Initialize Enemy-specific attributes here
    std::cout << "Enemy created with ID: " << getCurrentSpriteData()->getid_() << " at position (" << pos.x << ", " << pos.y << ")" << std::endl;
}

void Enemy::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}
