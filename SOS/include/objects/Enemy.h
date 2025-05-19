#ifndef ENEMY_H
#define ENEMY_H

#include "objects/entity.h"

class Enemy : public Entity {
public:
    Enemy(Vec2 pos, std::string objID);

    int health;

    void accept(CollisionVisitor& visitor) override;
    // void takeDamage(int amount);
    // void reverseDirection();
    // void applyPhysicsResponse(const Vec2& resolutionVector);
};

#endif // ENEMY_H
