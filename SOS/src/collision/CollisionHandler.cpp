#include "collision/CollisionHandler.h"
#include "objects/player.h"  // Include specific object headers
#include "enemy.h"
#include "tile.h"

#include <iostream>

CollisionHandler::CollisionHandler(Object* initiator, const CollisionInfo& info)
    : initiator(initiator), info(info) {}

void CollisionHandler::visit(Player* player) {
    // Handle collision between initiator and player
    if (initiator->type == ObjectType::TILE) {
        // Player collided with platform
        handleInteraction(player);
    } else if (initiator->type == ObjectType::MINOTAUR) {
        // Player collided with enemy
        handleInteraction(player);
    }
}

void CollisionHandler::visit(Enemy* enemy) {
    // Handle collision between initiator and enemy
    if (initiator->type == ObjectType::TILE) {
        // Enemy collided with platform
        handleInteraction(enemy);
    } else if (initiator->type == ObjectType::PLAYER) {
        // Enemy collided with player
        handleInteraction(enemy);
    } else if (initiator->type == ObjectType::MINOTAUR) {
        // Enemy collided with another enemy
        handleInteraction(enemy);
    }
}

void CollisionHandler::visit(Tile* platform) {
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
    if (initiator->type == ObjectType::TILE) {
        // Player landed on platform
        BoxCollider* pCollider = &player->getcollider();
        Vec2* pos = &pCollider->position;
        Vec2 vel = player->getvelocity();
        // Static cast to Platform to access platform-specific properties
        Tile* platform = static_cast<Tile*>(initiator);


        // Check flags for platform collision
        if (platform->hasFlag(Tile::BLOCKS_VERTICAL) && info.penetrationVector.y != 0) {
            // Coming from above
            pos->y -= info.penetrationVector.y;
        } else if (platform->hasFlag(Tile::BLOCKS_HORIZONTAL) && info.penetrationVector.x != 0) {
            // Side collision
            pos->x -= info.penetrationVector.x;
        }

        player->setcollider(*pCollider);
        player->setvelocity(vel);
    } else if (initiator->type == ObjectType::MINOTAUR) {
        // Player collided with enemy - cause damage
        // player->takeDamage(1); // Example damage amount
    }
}

void CollisionHandler::handleInteraction(Enemy* enemy) {
    if (initiator->type == ObjectType::TILE) {
        // Enemy landed on platform
        Tile* platform = static_cast<Tile*>(initiator);
        BoxCollider* pCollider = &enemy->getcollider();
        Vec2* pos = &pCollider->position;
        
        // Check flags for platform collision
        if (platform->hasFlag(Tile::BLOCKS_VERTICAL) && info.penetrationVector.y != 0) {
            // Coming from above
            pos->y -= info.penetrationVector.y;
        } else if (platform->hasFlag(Tile::BLOCKS_HORIZONTAL) && info.penetrationVector.x != 0) {
            // Side collision
            pos->x -= info.penetrationVector.x;
        }

    } else if (initiator->type == ObjectType::PLAYER) {
        // Enemy hit by player - might take damage depending on game logic
    } else if (initiator->type == ObjectType::MINOTAUR) {
        // Enemy collided with another enemy - handle accordingly
        // Enemies should not pass through each other
        if (initiator->getObjID() < enemy->getObjID()) {
            return; // Only one enemy should handle the collision
        }
        if(info.penetrationVector.x != 0) {
            BoxCollider* pCollider = &enemy->getcollider();
            Vec2* pos = &pCollider->position;
            Vec2* vel = &enemy->getvelocity();
            pos->x -= info.penetrationVector.x;
            vel->x = 0;
        } else if (info.penetrationVector.y != 0) {
            BoxCollider* pCollider = &enemy->getcollider();
            Vec2* pos = &pCollider->position;
            Vec2* vel = &enemy->getvelocity();
            pos->y -= info.penetrationVector.y;
            vel->y = 0;
        }
    }
}

void CollisionHandler::handleInteraction(Tile* platform) {
    // Usually platforms don't need to respond to collisions
    // but may have special behaviors like breaking
    // if (platform->isBreakable()) {
        // Handle breakable platforms
    // }
}