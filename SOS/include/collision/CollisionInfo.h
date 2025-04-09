#ifndef COLLISION_INFO_H
#define COLLISION_INFO_H

#include "Vector2D.h"

struct CollisionInfo {
    Vector2D penetrationVector;
    Vector2D contactPoint;
};

#endif // COLLISION_INFO_H
