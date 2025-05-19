#include "collision/CollisionManager.h"
#include <iostream>

std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects)
{
    std::vector<std::pair<Object*, Object*>> collisions;

    for (size_t i = 0; i < gameObjects.size(); ++i) {
        for (size_t j = i + 1; j < gameObjects.size(); ++j) {
            Object* objA = gameObjects[i].get();
            Object* objB = gameObjects[j].get();

            // Get the sprite dimensions for collision detection
            const SpriteRect spriteA = objA->getCurrentSpriteData()->getSpriteRect(1);
            const SpriteRect spriteB = objB->getCurrentSpriteData()->getSpriteRect(1);

            // if (!spriteA || !spriteB) 
            // {
            //     //NO SPRITE DATA
            //     std::cout << "No sprite data for one of the objects." << std::endl;
            //     continue;
            // }
            Vec2 posA = objA->getposition();
            Vec2 posB = objB->getposition();

            // Simple AABB collision detection
            float leftA = posA.x;
            float rightA = posA.x + spriteA.w;
            float topA = posA.y;
            float bottomA = posA.y + spriteA.h;

            float leftB = posB.x;
            float rightB = posB.x + spriteB.w;
            float topB = posB.y;
            float bottomB = posB.y + spriteB.h;

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
                
                // if(objA->getObjID() == "petalinux" || objB->getObjID() == "petalinux")
                // {
                //     std::cout << "Collision detected between " << objA->getObjID() << " and " << objB->getObjID() << std::endl;
                // }

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