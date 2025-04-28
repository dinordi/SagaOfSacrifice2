#include "collision/CollisionManager.h"
#include <iostream>

std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(const std::vector<Object*>& gameObjects)
{
    std::vector<std::pair<Object*, Object*>> collisions;

    for (size_t i = 0; i < gameObjects.size(); ++i) {
        for (size_t j = i + 1; j < gameObjects.size(); ++j) {
            Object* objA = gameObjects[i];
            Object* objB = gameObjects[j];

            // Get the sprite dimensions for collision detection
            const SpriteRect spriteA = objA->spriteData->getSpriteRect(1);
            const SpriteRect spriteB = objB->spriteData->getSpriteRect(1);

            // if (!spriteA || !spriteB) 
            // {
            //     //NO SPRITE DATA
            //     std::cout << "No sprite data for one of the objects." << std::endl;
            //     continue;
            // }

            // Simple AABB collision detection
            float leftA = objA->position.x;
            float rightA = objA->position.x + spriteA.w;
            float topA = objA->position.y;
            float bottomA = objA->position.y + spriteA.h;

            float leftB = objB->position.x;
            float rightB = objB->position.x + spriteB.w;
            float topB = objB->position.y;
            float bottomB = objB->position.y + spriteB.h;

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
                    info.penetrationVector.x = (objA->position.x < objB->position.x) ? -overlapX : overlapX;
                    info.penetrationVector.y = 0;
                    // std::cout << "X PENETRATION SMALLER: " << overlapX << " " << overlapY << std::endl;
                } else {
                    // Y-axis penetration is smaller
                    info.penetrationVector.x = 0;
                    info.penetrationVector.y = (objA->position.y < objB->position.y) ? -overlapY : overlapY;
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