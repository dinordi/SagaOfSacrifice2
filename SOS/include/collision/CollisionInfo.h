#ifndef COLLISION_INFO_H
#define COLLISION_INFO_H

#include "Vec2.h"

struct CollisionInfo {
    Vec2 penetrationVector;
    Vec2 contactPoint;
};

#endif // COLLISION_INFO_H
