#ifndef ENEMY_H
#define ENEMY_H

#include "objects/entity.h"

class Enemy : public Entity {
public:
    Enemy(BoxCollider collider, std::string objID);

    int health;

    // Enemy AI methods that control the behavior of the enemy


    // Virtual methods to be overridden by derived classes
    virtual void move() = 0; // Move the enemy
    virtual void attack() = 0; // Attack the player
    virtual void die() = 0; // Handle enemy death
    virtual void update(float deltaTime) override; // Update the enemy's state
};

#endif // ENEMY_H
