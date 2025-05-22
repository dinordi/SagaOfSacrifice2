#include "objects/enemy.h"
#include <iostream>
#include <cmath>
#include <random>
#include "objects/player.h"

Enemy::Enemy(BoxCollider collider, std::string objID, ObjectType type) : Entity(collider, objID, type) {
    // Initialize Enemy-specific attributes here
    health = 100;
    attackCooldown = 0.0f;
    attackRange = 100.0f;
    detectionRange = 300.0f;
    attackDamage = 10;
    moveSpeed = 120.0f;
    currentState = EnemyState::IDLE;
    wanderTimer = 0.0f;
    wanderDirection = Vec2(0, 0);
    isDead = false;
    
    // std::cout << "Enemy created with ID: " << objID << " at position (" 
    //           << getcollider().position.x << ", " << getcollider().position.y << ")" << std::endl;
}

void Enemy::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Enemy::update(float deltaTime) {
    if (isDead) return;
    
    // Decrease attack cooldown
    if (attackCooldown > 0) {
        attackCooldown -= deltaTime;
    }

    // Update based on current state
    switch (currentState) {
        case EnemyState::IDLE:
            // Transition to wandering after some time
            wanderTimer += deltaTime;
            if (wanderTimer > 2.0f) { // Stay idle for 2 seconds
                wanderTimer = 0.0f;
                currentState = EnemyState::WANDERING;
                
                // Choose random direction
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> dis(-1.0, 1.0);
                wanderDirection = Vec2(dis(gen), dis(gen));
                wanderDirection.normalize();
            }
            break;
            
        case EnemyState::WANDERING:
            // Move in wander direction
            {
            Vec2 vel = wanderDirection * moveSpeed * 0.5f;
            float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            if(speed > MAX_VELOCITY)
            {
                vel = (vel / speed) * MAX_VELOCITY;
            }
            setvelocity(vel);
            wanderTimer += deltaTime;
            
            // Change direction or go idle periodically
            if (wanderTimer > 3.0f) { // Wander for 3 seconds
                wanderTimer = 0.0f;
                
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 10);
                
                if (dis(gen) > 7) { // 30% chance to go idle
                    currentState = EnemyState::IDLE;
                } else {
                    // Choose a new wandering direction
                    std::uniform_real_distribution<> disr(-1.0, 1.0);
                    wanderDirection = Vec2(disr(gen), disr(gen));
                    wanderDirection.normalize();
                }
            }
            }
            break;
            
        case EnemyState::CHASING:
            if (targetPlayer) {
                // Update direction to player
                Vec2 direction = getDirectionToPlayer(targetPlayer);
                Vec2 vel = direction * moveSpeed;
                float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
                if(speed > MAX_VELOCITY)
                {
                    vel = (vel / speed) * MAX_VELOCITY;
                }
                setvelocity(vel);
                
                // Check if in attack range
                if (isPlayerInRange(targetPlayer, attackRange)) {
                    currentState = EnemyState::ATTACKING;
                }
                
                // If player gets too far, go back to wandering
                if (!isPlayerInRange(targetPlayer, detectionRange * 1.5f)) {
                    currentState = EnemyState::WANDERING;
                    wanderTimer = 0.0f;
                }
            } else {
                currentState = EnemyState::WANDERING;
                wanderTimer = 0.0f;
            }
            break;
            
        case EnemyState::ATTACKING:
            // Stop moving when attacking
            setvelocity(Vec2(0, 0));
            
            if (attackCooldown <= 0) {
                attack();
                attackCooldown = 1.0f; // 1 second between attacks
            }
            
            // Return to chasing if player is out of attack range
            if (targetPlayer && !isPlayerInRange(targetPlayer, attackRange)) {
                currentState = EnemyState::CHASING;
            }
            break;
            
        case EnemyState::DYING:
            setvelocity(Vec2(0, 0));
            die();
            break;
    }
    
    // Check for player detection
    if (targetPlayer && (currentState == EnemyState::IDLE || currentState == EnemyState::WANDERING)) {
        detectPlayer(targetPlayer);
    }

    //Print old position
    std::cout << "Old Position: (" << getcollider().position.x << ", " << getcollider().position.y << ")" << std::endl;
    
    // Update position based on velocity
    BoxCollider* pColl = &getcollider();
    Vec2* pos = &pColl->position;
    Vec2 vel = getvelocity();
    pos->x += vel.x * deltaTime;
    pos->y += vel.y * deltaTime;
    setcollider(*pColl); // Update collider position
    
    // Print new position
    std::cout << "New Position: (" << getcollider().position.x << ", " << getcollider().position.y << ")" << std::endl;

    // Update animation
    // updateAnimation(deltaTime*1000);
    Entity::update(deltaTime); // Call the base class update
    
    // Call the derived class's move function
    move();
}

void Enemy::detectPlayer(std::shared_ptr<Player> player) {
    if (!player) return;
    
    if (isPlayerInRange(player, detectionRange)) {
        currentState = EnemyState::CHASING;
        targetPlayer = player;
    }
}

bool Enemy::isPlayerInRange(std::shared_ptr<Player> player, float range) {
    if (!player) return false;
    
    Vec2 enemyPos = getcollider().position;
    Vec2 playerPos = player->getposition();
    
    float distance = std::sqrt(
        std::pow(playerPos.x - enemyPos.x, 2) + 
        std::pow(playerPos.y - enemyPos.y, 2)
    );
    
    return distance <= range;
}

Vec2 Enemy::getDirectionToPlayer(std::shared_ptr<Player> player) {
    if (!player) return Vec2(0, 0);
    
    Vec2 enemyPos = getcollider().position;
    Vec2 playerPos = player->getposition();
    
    Vec2 direction = Vec2(
        playerPos.x - enemyPos.x,
        playerPos.y - enemyPos.y
    );
    
    // Normalize if not zero
    if (direction.x != 0 || direction.y != 0) {
        direction.normalize();
    }
    
    return direction;
}
