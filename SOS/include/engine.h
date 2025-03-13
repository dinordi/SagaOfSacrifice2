// SOS/include/physicsEngine.h

#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include "game.h"
#include "Object.h"
#include <algorithm>

class PhysicsEngine {
public:
    void update(float deltaTime);
    void addObject(Object* object);
    void removeObject(Object* object);

private:
    void handleCollisions();
    std::vector<Object*> objects;
};

#endif // PHYSICS_ENGINE_H