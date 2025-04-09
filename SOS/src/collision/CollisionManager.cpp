#include "collision/CollisionManager.h"


std::vector<std::pair<Object*, Object*>> CollisionManager::detectCollisions(const std::vector<Object*>& gameObjects)
{
    std::vector<std::pair<Object*, Object*>> collisions;

    for (size_t i = 0; i < gameObjects.size(); ++i) {
        for (size_t j = i + 1; j < gameObjects.size(); ++j) {
            Object* objA = gameObjects[i];
            Object* objB = gameObjects[j];

            // Check for collision between objA and objB
            resolveCollision(objA, objB, CollisionInfo{});
            // Assuming resolveCollision modifies objA and objB to indicate a collision

        }
    }

    return collisions;
}

void CollisionManager::resolveCollision(Object* objA, Object* objB, const CollisionInfo& info)
{
    
}