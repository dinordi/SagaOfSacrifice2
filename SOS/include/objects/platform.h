#pragma once

#include "object.h"
#include "sprite_data.h"


class Platform : public Object {
public:
    Platform(int ID, int x, int y, SpriteData* spData);
    // Platform(int ID, int x, int y);
    void update(uint64_t deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    bool isBreakable() const;
};