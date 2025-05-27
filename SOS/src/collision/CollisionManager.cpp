#include "collision/CollisionManager.h"
#include <iostream>
#include "objects/tile.h"

std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects)
{
    std::vector<std::pair<Object*, Object*>> collisions;
    int collisionChecks = 0;
    for (size_t i = 0; i < gameObjects.size(); ++i) {
        for (size_t j = i + 1; j < gameObjects.size(); ++j) {
            Object* objA = gameObjects[i].get();
            Object* objB = gameObjects[j].get();

            if(!objA->isCollidable())
            {
                continue; // Skip non-collidable objects
            }
            if(!objB->isCollidable())
            {
                continue; // Skip non-collidable objects
            }
            
            BoxCollider* pColliderA = &objA->getcollider();
            BoxCollider* pColliderB = &objB->getcollider();
            Vec2 posA = pColliderA->position;
            Vec2 posB = pColliderB->position;
            
            // Skip collision if objects are too far apart (broad phase)
            float maxDistance = 200.0f; // Adjust based on your game
            float distanceSquared = (posA.x - posB.x) * (posA.x - posB.x) + 
            (posA.y - posB.y) * (posA.y - posB.y);
            if (distanceSquared > maxDistance * maxDistance) {
                continue; // Skip expensive collision check
            }
            
            collisionChecks++;
            // Get the sprite dimensions for collision detection
            const SpriteRect spriteA = objA->getCurrentSpriteData()->getSpriteRect(1);
            const SpriteRect spriteB = objB->getCurrentSpriteData()->getSpriteRect(1);

            // if (!spriteA || !spriteB) 
            // {
            //     //NO SPRITE DATA
            //     std::cout << "No sprite data for one of the objects." << std::endl;
            //     continue;
            // }

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
                    // std::cout << "X PENETRATION SMALLER: " << overlapX << " " << overlapY << std::endl;
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
            }
        }
    }
    std::cout << "[CollisionManager] Collision checks performed: " << collisionChecks << std::endl;
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