#include "objects/player.h"
#include <iostream>



Player::Player( int x, int y, std::string objID) : Entity(BoxCollider(Vec2(x,y), Vec2(64,64)), objID, ObjectType::PLAYER),
    health(100), isAttacking(false), isJumping(false), attackTimer(0.0f) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << objID << " at position (" << x << ", " << y << ")" << std::endl;
    setvelocity(Vec2(0, 0)); // Initialize velocity to zero
    // Setup player animations
    setupAnimations();
}

void Player::setupAnimations() {
    // Define player animations based on sprite sheet layout
    // Parameters: (AnimationState, startFrame, frameCount, framesPerRow, frameTime, loop)

    // Example animation setup - adjust these based on your actual sprite sheet
    std::filesystem::path base = std::filesystem::current_path();
    std::string temp = base.string();
    std::size_t pos = temp.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        temp = temp.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = std::filesystem::path(temp);
    basePath /= "SOS/assets/spriteatlas";
    std::cout << "Got base path for player" << std::endl;

    addSpriteSheet(AnimationState::IDLE, basePath / "wolfman_idle.tpsheet");        // Idle animation (1 frames)
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 2,3);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 0, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 6,7);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 4,5);

    addSpriteSheet(AnimationState::WALKING, basePath / "wolfman_walk.tpsheet");
    // addAnimation(AnimationState::WALKING, 0, 8, 9, 150, true);      // Walking animation (3 frames)
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0,7);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 8,15);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 16, 23);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 24, 31);

    addSpriteSheet(AnimationState::ATTACKING, basePath / "wolfman_slash.tpsheet", 80);

    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0,4);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 5,9);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 10,14);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 15,19);

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


    *pos += vel * deltaTime; // Update position based on velocity and delta time


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
    // updateAnimation(deltaTime*1000); // Convert deltaTime to milliseconds
    Entity::update(deltaTime); // Call base class update

    setvelocity(vel); // Update velocity
    setcollider(*pColl); // Update position
}

void Player::handleInput(PlayerInput* input, float deltaTime) {

    // Handle player input here
    Vec2 vel(0,0);

    float movementSpeed = 300.0f; // Set movement speed

    if (input->get_left()) {
        vel.x = -movementSpeed; // Move left
    } else if (input->get_right()) {
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
    // std::cout << "Player velocity: " << vel.x << ", " << vel.y << std::endl;
    setvelocity(vel);
}

void Player::takeDamage(int amount) {
    health -= amount;
    if (health <= 0) {
        // Handle player death
        // setAnimationState(AnimationState::DYING);
    } else {
        // Briefly show hurt animation
        // setAnimationState(AnimationState::HURT);
    }
}

void Player::collectItem() {
    // Item collection logic
}

void Player::applyPhysicsResponse(const Vec2& resolutionVector) {
    // Physics response handling
}
