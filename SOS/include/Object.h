// SOS1/include/Object.h

#ifndef OBJECT_H
#define OBJECT_H

#include <Vec2.h>

class Object {
public:
    Vec2 position;
    Vec2 velocity;

    virtual bool collisionWith(Object* other) = 0;
};

#endif // OBJECT_H