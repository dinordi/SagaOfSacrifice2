#ifndef ENEMY_H
#define ENEMY_H

#include "object.h"

class Enemy : public Object {
public:
    int health;

    void accept(CollisionVisitor& visitor) override;
    void takeDamage(int amount);
    void reverseDirection();
    void applyPhysicsResponse(const Vec2& resolutionVector);
};

#endif // ENEMY_H
