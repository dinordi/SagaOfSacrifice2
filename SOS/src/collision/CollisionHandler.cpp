#include "collision/CollisionHandler.h"
#include "player.h"  // Include specific object headers
#include "Enemy.h"
#include "platform.h"

#include <iostream>

CollisionHandler::CollisionHandler(Object* initiator, const CollisionInfo& info)
    : initiator(initiator), info(info) {}

void CollisionHandler::visit(Player* player) {
    // Handle collision between initiator and player
    if (initiator->type == ObjectType::PLATFORM) {
        // Player collided with platform
        handleInteraction(player);
    } else if (initiator->type == ObjectType::ENEMY) {
        // Player collided with enemy
        handleInteraction(player);
    }
}

void CollisionHandler::visit(Enemy* enemy) {
    // Handle collision between initiator and enemy
    if (initiator->type == ObjectType::PLATFORM) {
        // Enemy collided with platform
        handleInteraction(enemy);
    } else if (initiator->type == ObjectType::PLAYER) {
        // Enemy collided with player
        handleInteraction(enemy);
    }
}

void CollisionHandler::visit(Platform* platform) {
    // Platforms generally don't need to respond to collisions 
    // They are usually static, but might have special behaviors
    handleInteraction(platform);
}

void CollisionHandler::visit(RemotePlayer* remotePlayer) {
    // Handle collision between initiator and remote player
    // if (initiator->type == ObjectType::PLATFORM) {
    //     // Remote player collided with platform
    //     handleInteraction(remotePlayer);
    // } else if (initiator->type == ObjectType::ENEMY) {
    //     // Remote player collided with enemy
    //     handleInteraction(remotePlayer);
    // }
}

void CollisionHandler::handleInteraction(Player* player) {
    if (initiator->type == ObjectType::PLATFORM) {
        // Player landed on platform
        if (info.penetrationVector.y > 0) {
            // Coming from above
            player->position.y -= info.penetrationVector.y;
            player->velocity.y = 0;
            // Set the player as being on ground
            player->setisOnGround(true);
        } else if (info.penetrationVector.x != 0) {
            // Side collision
            player->position.x += info.penetrationVector.x;
        } else if (info.penetrationVector.y < 0) {
            // Hitting head on platform
            // player->position.y += info.penetrationVector.y;
            // player->velocity.y = 0;
        }
    } else if (initiator->type == ObjectType::ENEMY) {
        // Player collided with enemy - cause damage
        // player->takeDamage(1); // Example damage amount
    }
}

void CollisionHandler::handleInteraction(Enemy* enemy) {
    if (initiator->type == ObjectType::PLATFORM) {
        // Enemy landed on platform
        if (info.penetrationVector.y < 0) {
            enemy->position.y += info.penetrationVector.y;
            enemy->velocity.y = 0;
        } else if (info.penetrationVector.x != 0) {
            // Side collision - reverse direction
            // enemy->reverseDirection();
        }
    } else if (initiator->type == ObjectType::PLAYER) {
        // Enemy hit by player - might take damage depending on game logic
    }
}

void CollisionHandler::handleInteraction(Platform* platform) {
    // Usually platforms don't need to respond to collisions
    // but may have special behaviors like breaking
    // if (platform->isBreakable()) {
        // Handle breakable platforms
    // }
}