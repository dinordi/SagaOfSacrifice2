#include "collision/CollisionManager.h"
#include <iostream>
#include "objects/tile.h"

std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects)
{
    std::vector<std::pair<Object*, Object*>> collisions;
    int collisionChecks = 0;
    int objChecks = 0;
    
    // Create spatial grid with 200-pixel cells (matches our broad-phase distance check)
    SpatialGrid grid(200.0f);
    
    // Separate dynamic and static objects
    std::vector<Object*> dynamicObjects;
    
    // First pass: identify all collidable objects and add to appropriate collections
    for (const auto& obj : gameObjects) {
        if (!obj || !obj->isCollidable()) continue;
        
        // Add to grid
        grid.addObject(obj.get());
        
        // Only track dynamic objects separately (non-tiles)
        if (obj->type != ObjectType::TILE) {
            dynamicObjects.push_back(obj.get());
        }
    }
    
    // Second pass: check dynamic objects against potential colliders
    for (Object* dynamicObj : dynamicObjects) {
        // Get potential colliders for this object from the grid
        auto potentialColliders = grid.getPotentialColliders(dynamicObj);
        
        // Check against each potential collider
        for (Object* otherObj : potentialColliders) {
            objChecks++;
            
            // Skip tile-vs-tile collisions (already filtered by dynamic objects list)
            if (dynamicObj->type == ObjectType::TILE && otherObj->type == ObjectType::TILE) {
                continue;
            }
            
            // Check and resolve collision
            if (checkAndResolveCollision(dynamicObj, otherObj, collisions, collisionChecks)) {
                // Collision was detected and resolved
            }
        }
    }
    
    // std::cout << "[CollisionManager] Collision checks performed: " << collisionChecks 
    //           << ", object checks: " << objChecks 
    //           << ", size of object list: " << gameObjects.size() << std::endl;
    return collisions;
}

std::vector<std::pair<Object*, Object*>> CollisionManager::detectPlayerCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects, Player* player)
{
    if (!player) {
        return {};
    }
    
    std::vector<std::pair<Object*, Object*>> collisions;
    int collisionChecks = 0;
    int objChecks = 0;
    
    // Create spatial grid
    SpatialGrid grid(200.0f);
    
    // Add all collidable objects to the grid
    for (const auto& obj : gameObjects) {
        if (!obj || !obj->isCollidable()) continue;
        grid.addObject(obj.get());
    }
    
    // Get potential colliders for the player
    auto potentialColliders = grid.getPotentialColliders(player);
    
    // Check player against each potential collider
    for (Object* otherObj : potentialColliders) {
        objChecks++;
        
        // Skip self-collision
        if (player->getObjID() == otherObj->getObjID()) {
            continue;
        }
        
        // Check and resolve collision
        if (checkAndResolveCollision(player, otherObj, collisions, collisionChecks)) {
            // Collision was detected and resolved
        }
    }
    
    // std::cout << "[CollisionManager] Player collision checks performed: " << collisionChecks 
    //           << ", object checks: " << objChecks << std::endl;
    return collisions;
}

void CollisionManager::resolveCollision(Object* objA, Object* objB, const CollisionInfo& info)
{
    // Create a collision handler for objA
    CollisionHandler handlerA(objA, info);
    
    // Let objB accept the visitor
    objB->accept(handlerA);
    
    // Create a collision handler for objB with reversed collision info
    CollisionInfo reversedInfo = info;
    reversedInfo.penetrationVector = Vec2(-info.penetrationVector.x, -info.penetrationVector.y);
    CollisionHandler handlerB(objB, reversedInfo);
    
    // Let objA accept the visitor
    objA->accept(handlerB);
}

bool CollisionManager::checkAndResolveCollision(Object* objA, Object* objB, 
                                      std::vector<std::pair<Object*, Object*>>& collisions, 
                                      int& collisionChecks) {
    BoxCollider* pColliderA = &objA->getcollider();
    BoxCollider* pColliderB = &objB->getcollider();
    Vec2 posA = pColliderA->position;
    Vec2 posB = pColliderB->position;
    
    // Skip collision if objects are too far apart (broad phase)
    float maxDistance = 200.0f; // Adjust based on your game
    float distanceSquared = (posA.x - posB.x) * (posA.x - posB.x) + 
                           (posA.y - posB.y) * (posA.y - posB.y);
    if (distanceSquared > maxDistance * maxDistance) {
        return false; // Skip expensive collision check
    }
    
    collisionChecks++;
    
    // Simple AABB collision detection
    float leftA = posA.x;
    float rightA = posA.x + pColliderA->size.x;
    float topA = posA.y;
    float bottomA = posA.y + pColliderA->size.y;

    float leftB = posB.x;
    float rightB = posB.x + pColliderB->size.x;
    float topB = posB.y;
    float bottomB = posB.y + pColliderB->size.y;

    // Check if the two AABBs intersect
    if (leftA <= rightB && rightA >= leftB && topA <= bottomB && bottomA >= topB) {
        // Calculate collision info
        CollisionInfo info;
        
        // Calculate penetration depth in each axis
        float overlapX = std::min(rightA - leftB, rightB - leftA);
        float overlapY = std::min(bottomA - topB, bottomB - topA);
        
        // Set penetration vector to the minimal displacement
        if (overlapX < overlapY) {
            // X-axis penetration is smaller
            info.penetrationVector.x = (posA.x < posB.x) ? -overlapX : overlapX;
            info.penetrationVector.y = 0;
        } else {
            // Y-axis penetration is smaller
            info.penetrationVector.x = 0;
            info.penetrationVector.y = (posA.y < posB.y) ? -overlapY : overlapY;
        }
        
        // Calculate approximate contact point (center of overlap area)
        info.contactPoint.x = (std::max(leftA, leftB) + std::min(rightA, rightB)) / 2;
        info.contactPoint.y = (std::max(topA, topB) + std::min(bottomA, bottomB)) / 2;
        
        // Resolve the collision
        resolveCollision(objA, objB, info);
        
        // Add to the collision list
        collisions.push_back(std::make_pair(objA, objB));
        return true;
    }
    
    return false;
}