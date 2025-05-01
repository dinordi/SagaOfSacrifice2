#include "objects/Enemy.h"
#include <iostream>

Enemy::Enemy( Vec2 pos, SpriteData* spData, std::string objID) : Entity(pos, spData, objID) {
    // Initialize Enemy-specific attributes here
    std::cout << "Enemy created with ID: " << spriteData->getid_() << " at position (" << pos.x << ", " << pos.y << ")" << std::endl;
}

void Enemy::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}
