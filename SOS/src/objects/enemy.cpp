#include "objects/Enemy.h"
#include <iostream>

Enemy::Enemy( Vec2 pos, SpriteData* spData) : Entity(pos, spData) {
    // Initialize Enemy-specific attributes here
    std::cout << "Enemy created with ID: " << spriteData->ID << " at position (" << position.x << ", " << position.y << ")" << std::endl;
}

void Enemy::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}