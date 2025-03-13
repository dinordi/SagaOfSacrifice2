// SOS/src/physicsEngine.cpp

#include "engine.h"

void PhysicsEngine::update(float deltaTime) {
    for (Object* object : objects) {
        // Update object positions based on velocity and deltaTime
        object->position += object->velocity * deltaTime;
    }
    handleCollisions();
}

void PhysicsEngine::addObject(Object* object) {
    objects.push_back(object);
}

void PhysicsEngine::removeObject(Object* object) {
    objects.erase(std::remove(objects.begin(), objects.end(), object), objects.end());
}

void PhysicsEngine::handleCollisions() {
    for (size_t i = 0; i < objects.size(); ++i) {
        for (size_t j = i + 1; j < objects.size(); ++j) {
            if (objects[i]->collisionWith(objects[j])) {
                // Handle collision resolution
            }
        }
    }
}