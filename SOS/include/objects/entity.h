#pragma once
#include "object.h"
#include "sprite_data.h"


class Healthbar : public Actor
{
public:
    Healthbar(float x, float y, std::string tpsheet, int16_t maxHealth, bool enemy = true);
    void addSpriteSheet(std::string tpsheet);
    const SpriteData* getCurrentSpriteData() const;
    
    void setHealth(int16_t health);
    std::vector<SpriteRect> getSpriteRects() const { return spriteRects_; }
    std::vector<float> getOffsets(int count) const;
private:
    uint16_t maxHealth_;
    uint16_t currentHealth_;
    std::string spriteSheetPath_;
    std::vector<SpriteRect> spriteRects_; // Store sprite rects for this entity
    std::vector<float> offsets; // Offsets for each part of the healthbar
    bool enemy_; // True if this healthbar is for an enemy, false if for player
};

class Entity : public Object
{
public:
    Entity( BoxCollider collider, uint16_t objID, ObjectType type);
    
    bool isDead() const { return isDead_; }
    
    void update(float deltaTime) override; // Updating animation

    void updateHealthbar();
    Healthbar* getHealthbar() const { return healthbar_.get(); }
    int16_t getHealth() const { return health; }

    
    // Interpolation for remote entities
    void setTargetPosition(const Vec2& position) { targetPosition_ = position; }
    void setTargetVelocity(const Vec2& velocity) { targetVelocity_ = velocity; }
    void resetInterpolation() { interpolationTime_ = 0.0f; }
    const Vec2& getTargetPosition() const { return targetPosition_; }
    const Vec2& getTargetVelocity() const { return targetVelocity_; }
    float getInterpolationTime() const { return interpolationTime_; }
    void setIsRemote(bool isRemote) { isRemote_ = isRemote; }
    bool getIsRemote() const { return isRemote_; }
protected:
    bool isDead_;
    std::unique_ptr<Healthbar> healthbar_;
    int16_t health;
    // Interpolation state
    Vec2 targetPosition_;
    Vec2 targetVelocity_;
    float interpolationTime_ = 0.0f;
    bool isRemote_ = false;
};