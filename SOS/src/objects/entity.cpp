#include "objects/entity.h"
#include "platform.h"

Entity::Entity( Vec2 pos, SpriteData* spData) : Object( pos, ObjectType::ENTITY, spData) {}

void Entity::handlePlatformCollision(Platform* platform)
{
    // Implement collision handling logic here
    // For example, adjust the object's position based on the platform's position
    // SpriteData* platformSprite = platform->getSpriteData();
    // SpriteData* entitySprite = this->getSpriteData();

    float objectBottom = this->position.y + this->spriteData->height;
    float platformTop = platform->position.y;
    float objectRight = this->position.x + this->spriteData->width;
    float platformLeft = platform->position.x;
    float objectLeft = this->position.x;
    float platformRight = platform->position.x + platform->spriteData->width;

    if (objectBottom > platformTop && this->position.y < platformTop &&
        objectRight > platformLeft && objectLeft < platformRight) {
        // Collision detected
        // Adjust object's position to be on top of the platform
        this->position.y = platformTop - this->spriteData->height; // Place object on top of the platform
        this->velocity.y = 0; // Reset vertical velocity
        this->isOnGround = true; // Set object state to on ground
    }
}