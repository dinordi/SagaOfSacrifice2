#include "objects/minotaur.h"
#include "objects/player.h"
#include <iostream>
#include <cmath>
#include <random>

Minotaur::Minotaur(int x, int y, std::string objID) : Enemy(BoxCollider(x, y, 96, 96), objID, ObjectType::MINOTAUR) {
    // Call the other constructor's setup code
    setvelocity(Vec2(0, 0));
    
    // Minotaur specific stats
    health = 150;
    attackDamage = 25;
    attackRange = 120.0f;
    detectionRange = 400.0f;
    moveSpeed = 100.0f;

    // Set up animations
    addSpriteSheet(AnimationState::IDLE, new SpriteData("minotaurus_idle", 192, 192, 2), 100, true);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::WALKING, new SpriteData("minotaurus_walk", 192, 192, 8), 150, true);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::ATTACKING, new SpriteData("minotaurus_slash", 384, 384, 5), 80, false);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 3);

    // Set initial state
    setAnimationState(AnimationState::IDLE);
}

Minotaur::~Minotaur() {
    // Clean up any resources
}

void Minotaur::move() {
    // Update facing direction based on velocity
    Vec2 velocity = getvelocity();
    
    // Only update direction if moving
    if (velocity.x != 0 || velocity.y != 0) {
        // Determine direction based on velocity
        if (std::abs(velocity.x) > std::abs(velocity.y)) {
            // Horizontal movement is dominant
            if (velocity.x > 0) {
                dir = FacingDirection::EAST;
            } else {
                dir = FacingDirection::WEST;
            }
        } else {
            // Vertical movement is dominant
            if (velocity.y > 0) {
                dir = FacingDirection::SOUTH;
            } else {
                dir = FacingDirection::NORTH;
            }
        }
    }
    
    // Set animation based on state
    if (currentState == EnemyState::IDLE) {
        setAnimationState(AnimationState::IDLE);
    } else if (currentState == EnemyState::WANDERING || currentState == EnemyState::CHASING) {
        setAnimationState(AnimationState::WALKING);
    }
}

void Minotaur::attack() {
    // Only attack if we have a target
    if (!targetPlayer || attackCooldown > 0) return;
    
    std::cout << "Minotaur is attacking!" << std::endl;
    
    // Play attack animation
    setAnimationState(AnimationState::ATTACKING);
    
    // Deal damage to player if in range
    if (isPlayerInRange(targetPlayer, attackRange)) {
        // Get direction to face the player
        Vec2 direction = getDirectionToPlayer(targetPlayer);
        
        // Update facing direction
        if (std::abs(direction.x) > std::abs(direction.y)) {
            // Horizontal direction is dominant
            if (direction.x > 0) {
                dir = FacingDirection::EAST;
            } else {
                dir = FacingDirection::WEST;
            }
        } else {
            // Vertical direction is dominant
            if (direction.y > 0) {
                dir = FacingDirection::SOUTH;
            } else {
                dir = FacingDirection::NORTH;
            }
        }
        
        // Deal damage to the player
        targetPlayer->takeDamage(attackDamage);
    }
    
    // Set cooldown
    attackCooldown = 1.5f;
}

void Minotaur::die() {
    if (isDead) return;
    
    std::cout << "Minotaur is dying!" << std::endl;
    // Play death animation or effects here
    
    // Mark as dead
    isDead = true;
    
    // The minotaur object will be removed by the game's cleanup mechanism
}

void Minotaur::update(float deltaTime) {
    // Use base enemy update for AI behavior
    Enemy::update(deltaTime);
    
    // Minotaur-specific updates can be added here
}