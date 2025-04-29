#include "objects/player.h"
#include <iostream>


Player::Player( Vec2 pos, SpriteData* spData) : Entity(pos, spData) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << spriteData->getid_() << " at position (" << pos.x << ", " << pos.y << ")" << std::endl;
    setisOnGround(false);
}

void Player::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Player::update(uint64_t deltaTime) {
    // Update player-specific logic here
    Vec2 pos = getposition();
    Vec2 vel = getvelocity();
    // float deltaTimeSeconds = static_cast<float>(deltaTime) / 1000.0f;
    float deltaTimeF = static_cast<float>(deltaTime);
    // Prints velocity.y every second
    static uint64_t timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1000.0f) {
        // std::cout << "Player velocity.y: " << this->velocity.y << std::endl;
        timeseconds = 0.0f;
    }
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
    
    vel.x = 0; // Reset horizontal velocity
    // static uint32_t timems = 0;
    // timems += deltaTime;
    // if (timems >= 500) {
    //     std::cout << "Player position: (" 
    //     << pos.x << ", " << pos.y << ") (" 
    //     << vel.x << ", " << vel.y << ") deltatime: " 
    //     << deltaTime << ", vel.y: " << vel.y << std::endl;
    //     timems = 0;
    // }
    setvelocity(vel); // Update velocity
    setposition(pos); // Update position
    setisOnGround(false); // Reset ground state

}

void Player::handleInput(PlayerInput* input, uint64_t deltaTime) {
    // Handle player input here
    Vec2 vel = getvelocity();
    if (input->get_jump()) {
        // Apply jump force
        vel.y -= 2.0f; // Example jump force
    }
    if (input->get_left()) {
        vel.x -= 1.0f; // Move left
    }
    if (input->get_right()) {
        vel.x += 1.0f; // Move right
    }
    setvelocity(vel);
}
