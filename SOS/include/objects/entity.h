#pragma once
#include "object.h"
#include "sprite_data.h"


class Healthbar : public Actor
{
public:
    Healthbar(float x, float y, std::string tpsheet, uint16_t maxHealth, bool enemy = true);
    void addSpriteSheet(std::string tpsheet);
    const SpriteData* getCurrentSpriteData() const;
    
    void setHealth(int health);
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
    Entity( BoxCollider collider, std::string objID, ObjectType type);
    
    bool isDead() const { return isDead_; }
    
    void update(float deltaTime) override; // Updating animation

    void updateHealthbar();
    Healthbar* getHealthbar() const { return healthbar_; }
    
protected:
    bool isDead_;
    Healthbar* healthbar_;
    int health; 
};