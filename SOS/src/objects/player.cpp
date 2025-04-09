#include "objects/player.h"
#include <iostream>


Player::Player( Vec2 pos, SpriteData* spData) : Entity(pos, spData) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << spriteData->ID << " at position (" << position.x << ", " << position.y << ")" << std::endl;
}

bool Player::collisionWith(Object* other) {
    // Implement collision detection logic here
    Platform* platform = dynamic_cast<Platform*>(other);
    if (platform) {

        Vec2 start = this->position;
        Vec2 end = this->position + this->velocity;

        // Check for collision with platform
        // float playerBottom = this->position.y + this->spriteData->height;
        float platformTop = platform->position.y;
        // float playerRight = this->position.x + this->spriteData->width;
        float platformLeft = platform->position.x;
        // float playerLeft = this->position.x;
        float platformRight = platform->position.x + platform->spriteData->width;
        float platformBottom = platform->position.y + platform->spriteData->height;

        if (end.x + this->spriteData->width > platformLeft &&
            end.x < platformRight &&
            end.y + this->spriteData->height > platformTop && 
            end.y < platformBottom) {
            // Collision detected
            // std::cout << "Collision with platform detected!" << std::endl;
            // Adjust player position to be on top of the platform
            // this->position.y = platformTop - this->getSpriteData()->height; // Place player on top of the platform
            // this->velocity.y = 0; // Reset vertical velocity
            // this->isOnGround = true; // Set player state to on ground
            return true;
        }
    }
    return false;
}


void Player::update(uint64_t deltaTime) {
    // Update player-specific logic here

    // Prints velocity.y every second
    static uint64_t timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1000.0f) {
        std::cout << "Player velocity.y: " << this->velocity.y << std::endl;
        timeseconds = 0.0f;
    }
    // For example, move the player based on input
    float deltaTimeInSeconds = static_cast<float>(deltaTime) / 1000.0f;
    this->velocity.y += GRAVITY * deltaTimeInSeconds; // Apply gravity
    this->velocity.x = 0; // Reset horizontal velocity

    this->position.x += this->velocity.x * deltaTime;
    this->position.y += this->velocity.y * deltaTime;
}