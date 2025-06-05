#include "objects/minotaur.h"
#include "objects/player.h"
#include <iostream>
#include <cmath>
#include <random>
#include <filesystem>

Minotaur::Minotaur(int x, int y, uint16_t objID) : Enemy(BoxCollider(x, y, 64, 64), objID, ObjectType::MINOTAUR) {
    // Call the other constructor's setup code
    setvelocity(Vec2(0, 0));
    
    // Minotaur specific stats
    health = 150;
    attackDamage = 25;
    attackRange = 120.0f;
    detectionRange = 400.0f;
    moveSpeed = 100.0f;
}

Minotaur::~Minotaur() {
    // Clean up any resources
}

void Minotaur::setupAnimations(std::filesystem::path atlasPath)
{
    // Set up animations
    addSpriteSheet(AnimationState::IDLE, atlasPath / "minotaurus_idle.tpsheet");
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 0, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 2,3);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 4,5);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 6,7);

    addSpriteSheet(AnimationState::WALKING, atlasPath / "minotaurus_walk.tpsheet");
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0,7);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 8,15);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 16, 23);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 24, 31);

    addSpriteSheet(AnimationState::ATTACKING, atlasPath / "minotaurus_slash.tpsheet");
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0,4);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 5,9);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 10,14);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 15,19);

    std::cout << "Setting up healthbar" << std::endl;
    //Setup healthbar
    healthbar_ = std::make_unique<Healthbar>(getposition().x, getposition().y - 20, atlasPath / "healthbar.tpsheet", health);
    // Set initial state
    setAnimationState(AnimationState::IDLE);
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
    if (isDead_) return;
    
    // Play death animation or effects here
    
    // Mark as dead
    isDead_ = true;
    
    // The minotaur object will be removed by the game's cleanup mechanism
}

void Minotaur::update(float deltaTime) {
    // Interpolation for remote minotaurs is handled in Entity::update
    Enemy::update(deltaTime);
    // Minotaur-specific updates can be added here
}