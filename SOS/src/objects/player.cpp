#include "objects/player.h"
#include <iostream>


Player::Player(int ID, int x, int y) : Object(ID, x, y) {
    // Initialize player-specific attributes here
    std::cout << "Player created with ID: " << ID << " at position (" << x << ", " << y << ")" << std::endl;
    this->position.x = x;
    this->position.y = y;

    SpriteData* sprite = new SpriteData();
    sprite->ID = ID;
    sprite->width = 128; // Example width
    sprite->height = 128; // Example height
    this->setSpriteData(sprite);
}

bool Player::collisionWith(Object* other) {
    // Implement collision detection logic here
    Platform* platform = dynamic_cast<Platform*>(other);
    if (platform) {
        // Check for collision with platform
        float playerBottom = this->position.y + this->getSpriteData()->height;
        float platformTop = platform->position.y;
        float playerRight = this->position.x + this->getSpriteData()->width;
        float platformLeft = platform->position.x;
        float playerLeft = this->position.x;
        float platformRight = platform->position.x + platform->getSpriteData()->width;

        if (playerBottom > platformTop && this->position.y < platformTop &&
            playerRight > platformLeft && playerLeft < platformRight) {
            // Collision detected
            return true;
        }
    }
    return false;
}


void Player::update(uint64_t deltaTime) {
    // Update player-specific logic here
    // For example, move the player based on input
    float deltaTimeInSeconds = static_cast<float>(deltaTime) / 1000.0f;
    this->velocity.y += GRAVITY * deltaTimeInSeconds; // Apply gravity
    this->velocity.x = 0; // Reset horizontal velocity

    this->position.x += this->velocity.x * deltaTime;
    this->position.y += this->velocity.y * deltaTime;
}