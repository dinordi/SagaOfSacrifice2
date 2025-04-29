#include "objects/player.h"
#include <iostream>


Player::Player( Vec2 pos, SpriteData* spData, std::string objID) : Entity(pos, spData, objID), 
    health(100), isAttacking(false), isJumping(false), attackTimer(0) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << spriteData->getid_() << " at position (" << pos.x << ", " << pos.y << ")" << std::endl;
    setisOnGround(false);
    
    // Setup player animations
    setupAnimations();
}

void Player::setupAnimations() {
    // Define player animations based on sprite sheet layout
    // Parameters: (AnimationState, startFrame, frameCount, framesPerRow, frameTime, loop)
    
    // Example animation setup - adjust these based on your actual sprite sheet
    addAnimation(AnimationState::IDLE, 0, 2, spriteData->columns, 150, true);        // Idle animation (4 frames)
    addAnimation(AnimationState::WALKING, 2, 3, spriteData->columns, 100, true);      // Walking animation (6 frames)
    // addAnimation(AnimationState::JUMPING, 10, 2, spriteData->columns, 200, false);    // Jumping animation (2 frames)
    // addAnimation(AnimationState::FALLING, 12, 2, spriteData->columns, 150, true);     // Falling animation (2 frames)
    // addAnimation(AnimationState::ATTACKING, 14, 3, spriteData->columns, 80, false);   // Attack animation (3 frames)
    
    // Set initial state
    setAnimationState(AnimationState::IDLE);
}

void Player::updateAnimationState() {
    Vec2 vel = getvelocity();
    
    // Determine the appropriate animation state based on player's current condition
    // if (isAttacking) {
    //     setAnimationState(AnimationState::ATTACKING);
    //     return;
    // }
    
    if (!getisOnGround()) {
        if (vel.y < 0) {
            // Moving upward
            // setAnimationState(AnimationState::JUMPING);
        } else {
            // Falling down
            // setAnimationState(AnimationState::FALLING);
        }
    } else {
        // On ground
        if (isMoving()) {
            setAnimationState(AnimationState::WALKING);
        } else {
            setAnimationState(AnimationState::IDLE);
        }
    }
}

bool Player::isMoving() const {
    // Check if player is moving horizontally
    return getvelocity().x != 0;
}

void Player::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Player::update(uint64_t deltaTime) {
    // Update player-specific logic here
    handleInput(input, deltaTime); // Handle input

    Vec2 pos = getposition();
    Vec2 vel = getvelocity();
    // float deltaTimeSeconds = static_cast<float>(deltaTime) / 1000.0f;
    float deltaTimeF = static_cast<float>(deltaTime);
    // Prints velocity.y every second
    static uint64_t timems = 0.0f;
    timems += deltaTime;

    if(getisOnGround() == false)
    {
        // For example, move the player based on input
        vel.y += GRAVITY * (deltaTimeF / 1000); // Apply gravity
    }
    if(vel.y > MAX_VELOCITY)
    {
        vel.y = MAX_VELOCITY; // Cap the velocity
    }

    pos.x += vel.x * deltaTimeF;
    pos.y += vel.y * deltaTimeF;

    
    // Set direction based on horizontal velocity
    if (vel.x > 0) {
        facingRight = true;
    } else if (vel.x < 0) {
        facingRight = false;
    }
    
    // Handle attack timer
    if (isAttacking) {
        attackTimer += deltaTime;
        if (attackTimer >= 400) { // Attack animation time (ms)
            isAttacking = false;
            attackTimer = 0;
        }
    }
    
    
    // Update animation state based on player state
    updateAnimationState();
    // Update the animation controller
    updateAnimation(deltaTime);

    vel.x = 0; // Reset horizontal velocity
    
    setvelocity(vel); // Update velocity
    setposition(pos); // Update position
    if(getisOnGround() == true && timems > 20)
    {
        setisOnGround(false); // Reset ground state
        timems = 0;
    }
}

void Player::handleInput(PlayerInput* input, uint64_t deltaTime) {
    
    // Handle player input here
    Vec2 vel = getvelocity();
    if (input->get_jump() && getisOnGround()) {
        // Apply jump force only if on ground
        vel.y = -2.0f; // Example jump force (negative y is up)
        isJumping = true;
    }
    
    if (input->get_left()) {
        vel.x = -0.3f; // Move left
        facingRight = false;
    }
    if (input->get_right()) {
        vel.x = 0.3f; // Move right
        facingRight = true;
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
