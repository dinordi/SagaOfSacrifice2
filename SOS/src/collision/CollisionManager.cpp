#include "collision/CollisionManager.h"
#include <iostream>
#include "objects/tile.h"

std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(Game& game)
{
    std::vector<std::pair<Object*, Object*>> collisions;
    int collisionChecks = 0;
    int objChecks = 0;

    // Lock spatial grid access (thread safety)
    std::lock_guard<std::mutex> lock(game.getSpatialGridMutex());
    
    // Get dynamic objects from Game class
    const auto& dynamicObjects = game.getDynamicObjects(); // vector<Object*> of vector<shared_ptr<Object>>
    auto spatialGrid = game.getSpatialGrid(); 
    // Main collision detection loop
    for (const auto& obj : dynamicObjects) {
        if (!obj) continue;

        Object* dynamicObj = obj.get(); // if shared_ptr, anders direct Object*

        // Get potential colliders for this object from the grid
        auto potentialColliders = spatialGrid->getPotentialColliders(dynamicObj);

        // Check against each potential collider
        for (Object* otherObj : potentialColliders) {
            objChecks++;

            // Skip tile-vs-tile collisions
            if (dynamicObj->type == ObjectType::TILE && otherObj->type == ObjectType::TILE) {
                continue;
            }

            // Check and resolve collision
            if (checkAndResolveCollision(dynamicObj, otherObj, collisions, collisionChecks)) {
                // Collision detected and resolved
            }
        }
    }

    //Optional: performance debug log
    // std::cout << "[CollisionManager] Collision checks performed: " << collisionChecks 
    //           << ", object checks: " << objChecks 
    //           << ", dynamic objects: " << dynamicObjects.size() << std::endl;

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