#ifndef ENEMY_H
#define ENEMY_H

#include "objects/entity.h"
#include <memory>

class Player;

enum class EnemyState {
    IDLE,
    WANDERING,
    CHASING,
    ATTACKING,
    DYING
};

class Enemy : public Entity {
public:
    Enemy(BoxCollider collider, std::string objID, ObjectType type);

    int health;
    float attackCooldown;
    float attackRange;
    float detectionRange;
    int attackDamage;
    float moveSpeed;
    
    EnemyState currentState;

    // Enemy AI methods that control the behavior of the enemy
    void detectPlayer(std::shared_ptr<Player> player);
    bool isPlayerInRange(std::shared_ptr<Player> player, float range);
    Vec2 getDirectionToPlayer(std::shared_ptr<Player> player);
    
    // Set the target player for the enemy to track
    void setTargetPlayer(std::shared_ptr<Player> player) { targetPlayer = player; }

    // Virtual methods to be overridden by derived classes
    virtual void move() = 0; // Move the enemy
    virtual void attack() = 0; // Attack the player
    virtual void die() = 0; // Handle enemy death
    virtual void update(float deltaTime) override; // Update the enemy's state
    
    void accept(CollisionVisitor& visitor) override;
    
protected:
    std::shared_ptr<Player> targetPlayer;
    float wanderTimer;
    Vec2 wanderDirection;
    bool isDead;
};

#endif // ENEMY_H
