#include "objects/player.h"
#include <iostream>


Player::Player( BoxCollider collider, std::string objID) : Entity(collider, objID),
    health(100), isAttacking(false), isJumping(false), attackTimer(0.0f) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << objID << " at position (" << collider.position.x << ", " << collider.position.y << ")" << std::endl;
    setvelocity(Vec2(0, 0)); // Initialize velocity to zero
    // Setup player animations
    setupAnimations();
}

void Player::setupAnimations() {
    // Define player animations based on sprite sheet layout
    // Parameters: (AnimationState, startFrame, frameCount, framesPerRow, frameTime, loop)

    // Example animation setup - adjust these based on your actual sprite sheet

    addSpriteSheet(AnimationState::IDLE, new SpriteData("wolfman_idle", 128, 128, 2), 100, true);
    // addAnimation(AnimationState::IDLE, 0, 1, getCurrentSpriteData()->columns, 250, true);        // Idle animation (1 frames)
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::WALKING, new SpriteData("wolfman_walk", 128, 128, 8), 150, true);
    // addAnimation(AnimationState::WALKING, 0, 8, 9, 150, true);      // Walking animation (3 frames)
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::ATTACKING, new SpriteData("wolfman_slash", 384, 384, 5), 80, false);

    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 3);

    // Set initial state
    setAnimationState(AnimationState::IDLE);
}

void Player::updateAnimationState() {
    AudioManager& audioManager = AudioManager::Instance();

    if(isAttacking) {
        setAnimationState(AnimationState::ATTACKING);
        // play attack sound once
        //AudioManager::getInstance().playSound("attack_sound", 0.5f, false);
        return;
    }
    if (isMoving()) {
        setAnimationState(AnimationState::WALKING);
        // play walking sound once
        
        audioManager.playSound("walking");
        

    } else {
        setAnimationState(AnimationState::IDLE);
        audioManager.stopSound("walking");
    
    
    }


}

bool Player::isMoving() const {
    // Check if the player is moving based on velocity
    Vec2 vel = getvelocity();

    // Define an epsilon for floating point comparison
    const float EPSILON = 0.001f;

    // Compare with epsilon to handle floating-point precision issues
    return (std::abs(vel.x) > EPSILON || std::abs(vel.y) > EPSILON);
}

void Player::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Player::update(float deltaTime) {
    // Update player-specific logic here
    // handleInput(input, deltaTime); // Handle input

    BoxCollider* pColl = &getcollider();
    Vec2* pos = &pColl->position;
    Vec2 vel = getvelocity();

    // Prints velocity.y every second
    static uint64_t timems = 0.0f;
    timems += deltaTime;

    pos->x += vel.x * deltaTime;
    pos->y += vel.y * deltaTime;


    // Set direction based on horizontal and vertical velocity
    if (vel.x > 0) {
        dir = FacingDirection::EAST;
    } else if (vel.x < 0) {
        dir = FacingDirection::WEST;
    } else if (vel.y > 0) {
        dir = FacingDirection::SOUTH;
    } else if (vel.y < 0) {
        dir = FacingDirection::NORTH;
    } else if (vel.x < 0 && vel.y < 0) {
        dir = FacingDirection::NORTH_WEST;
    } else if (vel.x > 0 && vel.y < 0) {
        dir = FacingDirection::NORTH_EAST;
    } else if (vel.x < 0 && vel.y > 0) {
        dir = FacingDirection::SOUTH_WEST;
    } else if (vel.x > 0 && vel.y > 0) {
        dir = FacingDirection::SOUTH_EAST;
    }

    // Handle attack timer
    if (isAttacking) {
        attackTimer += deltaTime;
        if (attackTimer >= 0.4f) { // Attack animation time (ms)
            isAttacking = false;
            attackTimer = 0.0f;
        }
    }


    // Update animation state based on player state
    updateAnimationState();

    // Update the animation controller
    updateAnimation(deltaTime*1000); // Convert deltaTime to milliseconds

    vel.x = 0; // Reset horizontal velocity
    vel.y = 0; // Reset vertical velocity

    setvelocity(vel); // Update velocity
    setcollider(*pColl); // Update position
}

void Player::handleInput(PlayerInput* input, float deltaTime) {

    // Handle player input here
    Vec2 vel = getvelocity();

    float movementSpeed = 300.0f; // Set movement speed

    if (input->get_left()) {
        vel.x = -movementSpeed; // Move left
    }
    if (input->get_right()) {
        // std::cout << "get_right" << std::endl;
        vel.x = movementSpeed; // Move right
    }
    if (input->get_down()) {
        vel.y = movementSpeed; // Move down
    }
    if (input->get_up()) {
        vel.y = -movementSpeed; // Move up
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
        // trigger death sfx
    } else {
        // Briefly show hurt animation
        setAnimationState(AnimationState::HURT);
        // trigger hurt sfx
    }
}

void Player::collectItem() {
    // Item collection logic
}

void Player::applyPhysicsResponse(const Vec2& resolutionVector) {
    // Physics response handling
}
