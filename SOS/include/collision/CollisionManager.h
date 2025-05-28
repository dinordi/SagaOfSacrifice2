#ifndef COLLISION_MANAGER_H
#define COLLISION_MANAGER_H

#include <vector>
#include <utility>
#include <memory>
#include "object.h"
#include "CollisionInfo.h"
#include "CollisionHandler.h"
#include "objects/player.h"

class CollisionManager {
public:
    std::vector<std::pair<Object*, Object*>> detectCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects);
    std::vector<std::pair<Object*, Object*>> detectPlayerCollisions(const std::vector<std::shared_ptr<Object>>& gameObjects, Player* player);
    void resolveCollision(Object* objA, Object* objB, const CollisionInfo& info);
};

#endif // COLLISION_MANAGER_H
