#pragma once

#include "object.h"
#include "sprite_data.h"


class Platform : public Object {
public:
    Platform(int ID, int x, int y);
    bool collisionWith(Object* other) override;
    void update(uint64_t deltaTime) override;
};