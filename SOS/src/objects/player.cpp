#include "objects/player.h"
#include <iostream>
#include "interfaces/AudioManager.h"

Player::Player( Vec2 pos, SpriteData* spData, std::string objID) : Entity(pos, spData, objID), 
    health(100), isAttacking(false), isJumping(false), attackTimer(0) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << objID << " at position (" << pos.x << ", " << pos.y << ")" << std::endl;
    setvelocity(Vec2(0, 0)); // Initialize velocity to zero
    // Setup player animations
    setupAnimations();
}

void Player::setupAnimations() {
    // Define player animations based on sprite sheet layout
    // Parameters: (AnimationState, startFrame, frameCount, framesPerRow, frameTime, loop)
    
    // Example animation setup - adjust these based on your actual sprite sheet
    addAnimation(AnimationState::IDLE, 0, 2, spriteData->columns, 150, true);        // Idle animation (2 frames)
    addAnimation(AnimationState::WALKING, 2, 3, spriteData->columns, 100, true);      // Walking animation (3 frames)
    
    // Set initial state
    setAnimationState(AnimationState::IDLE);
}

void Player::updateAnimationState() {
    
    if (isMoving()) {
        setAnimationState(AnimationState::WALKING);
    } else {
        setAnimationState(AnimationState::IDLE);
    }
    
}

bool Player::isMoving() const {
    // Check if the player is moving based on velocity
    return getvelocity().x != 0 || getvelocity().y != 0;
}

void Player::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Player::update(uint64_t deltaTime) {
    // Update player-specific logic here
    // handleInput(input, deltaTime); // Handle input

    Vec2 pos = getposition();
    Vec2 vel = getvelocity();

    float deltaTimeF = static_cast<float>(deltaTime);
    static uint64_t timems = 0.0f;
    timems += deltaTime;

    pos.x += vel.x * deltaTimeF;
    pos.y += vel.y * deltaTimeF;

    // Set direction based on horizontal velocity
    if (vel.x > 0) {
        dir = FacingDirection::RIGHT;
    } 
    else if (vel.x < 0) {
        dir = FacingDirection::LEFT;
    }
    if (vel.y < 0) {
        dir = FacingDirection::UP;
    } 
    else if (vel.y > 0) {
        dir = FacingDirection::DOWN;
    }

    // Handle attack timer
    if (isAttacking) {
        attackTimer += deltaTime;
        if (attackTimer >= 400) {
            isAttacking = false;
            attackTimer = 0;
        }
    }    // Modular audio: play walking sound only when starting to move
    bool currentlyMoving = isMoving();
    if (currentlyMoving && !wasMoving) {
        std::cout << "[DEBUG] Attempting to play walking sound..." << std::endl;
        
        // Add safety checks to avoid crashes
        try {
            // Check if AudioManager instance exists
            if (&AudioManager::Instance() == nullptr) {
                std::cerr << "[ERROR] AudioManager instance is null!" << std::endl;
            } else {
                std::cout << "[DEBUG] AudioManager instance exists, calling playSound..." << std::endl;
                AudioManager::Instance().playSound("walking");
                std::cout << "[DEBUG] After playSound call" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in audio playback: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception in audio playback" << std::endl;
        }
    }
    else if (!currentlyMoving && wasMoving) {
        std::cout << "[DEBUG] Attempting to stop walking sound..." << std::endl;
        AudioManager::Instance().stopSound("walking");
    }
    wasMoving = currentlyMoving;

    // Update animation state based on player state
    updateAnimationState();
    // Update the animation controller
    updateAnimation(deltaTime);

    vel.x = 0; // Reset horizontal velocity
    vel.y = 0; // Reset vertical velocity
    setvelocity(vel); // Update velocity
    setposition(pos); // Update position
}

void Player::handleInput(PlayerInput* input, uint64_t deltaTime) {
    
    // Handle player input here
    Vec2 vel = getvelocity();
    
    if (input->get_left()) {
        vel.x = -0.3f; // Move left
    }
    if (input->get_right()) {
        // std::cout << "get_right" << std::endl;
        vel.x = 0.3f; // Move right
    }
    if (input->get_down()) {
        vel.y = 0.3f; // Move down
    }
    if (input->get_up()) {
        vel.y = -0.3f; // Move up
    }
    
    // Handle attack input
    if (input->get_attack() && !isAttacking) {
        isAttacking = true;
        attackTimer = 0;
    }
    
    setvelocity(vel);
}

void Player::takeDamage(int amount) {
    health -= amount;
    if (health <= 0) {
        // Handle player death
        setAnimationState(AnimationState::DYING);
    } else {
        // Briefly show hurt animation
        setAnimationState(AnimationState::HURT);
    }
}

void Player::collectItem() {
    // Item collection logic
}

void Player::applyPhysicsResponse(const Vec2& resolutionVector) {
    // Physics response handling
}
