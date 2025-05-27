#include "objects/entity.h"
#include "tile.h"

Entity::Entity( BoxCollider collider, std::string objID, ObjectType type) : Object( collider, type, objID) {}

void Entity::update(float deltaTime) {
    // Update animation state
    updateAnimation(deltaTime*1000);
}