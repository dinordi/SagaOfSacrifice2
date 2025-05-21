#include "objects/enemy.h"
#include <iostream>

Enemy::Enemy( BoxCollider collider, std::string objID) : Entity(collider, objID) {
    // Initialize Enemy-specific attributes here
    std::cout << "Enemy created with ID: " << getCurrentSpriteData()->getid_() << " at position (" << getcollider().position.x << ", " << getcollider().position.y << ")" << std::endl;
}

void Enemy::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}
