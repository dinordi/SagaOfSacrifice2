// SOS/include/physicsEngine.h

#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include "game.h"
#include "object.h"
#include <algorithm>

class PhysicsEngine {
public:
    void update(float deltaTime, std::vector<Object*>& objects);

private:
    void handleCollisions(std::vector<Object*>& objects);
};

#endif // PHYSICS_ENGINE_H