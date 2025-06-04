#include "objects/entity.h"
#include "tile.h"
#include <iostream>

Entity::Entity( BoxCollider collider, std::string objID, ObjectType type) : Object( collider, type, objID), 
    isDead_(false),
    healthbar_(nullptr),
    health(100), // Default health value, can be set later
    targetPosition_(0, 0),
    targetVelocity_(0, 0),
    interpolationTime_(0.0f),
    isRemote_(false)
{

}

void Entity::update(float deltaTime) {
    // Update animation state
    updateAnimation(deltaTime*1000);
    Vec2 pos = getposition();
    Vec2 vel = getvelocity();
    if (isRemote_) {
        interpolationTime_ += deltaTime;
        float t = std::min(interpolationTime_ / 0.1f, 1.0f); // 0.1s default interp period
        // Only interpolate if we have a different target position
        if ((targetPosition_.x != pos.x || targetPosition_.y != pos.y) && t < 1.0f) {
            pos.x = pos.x + (targetPosition_.x - pos.x) * t;
            pos.y = pos.y + (targetPosition_.y - pos.y) * t;
            vel.x = vel.x + (targetVelocity_.x - vel.x) * t;
            vel.y = vel.y + (targetVelocity_.y - vel.y) * t;
        } else {
            pos += vel * deltaTime;
        }
        setposition(pos);
        setvelocity(vel);
    } else {
        pos += vel * deltaTime;
        setposition(pos);
    }
}

Healthbar::Healthbar(float x, float y, std::string tpsheet, uint16_t maxHealth, bool enemy)
    : Actor(Vec2(x,y), tpsheet, 0, ActorType::HEALTHBAR), enemy_(enemy)
{
    std::cout << "Healthbar created at position: (" << x << ", " << y << ") with max health: " << maxHealth << ", tpsheet: " << tpsheet << std::endl;
    this->spriteSheetPath_ = tpsheet;
    this->maxHealth_ = maxHealth;
    this->currentHealth_ = maxHealth; // Initialize current health to max health
    addSpriteSheet(tpsheet);
}

void Healthbar::addSpriteSheet(std::string tpsheet)
{
    std::cout << "Adding sprite sheet: " << tpsheet << std::endl;
    SpriteData* spriteData = SpriteData::getSharedInstance(tpsheet);
}

const SpriteData* Healthbar::getCurrentSpriteData() const
{
    return SpriteData::getSharedInstance(spriteSheetPath_);
}

void Healthbar::setHealth(int health)
{
    // Healthbar has 5 parts, each part is 20% of the health
    currentHealth_ = health;
    if (currentHealth_ > maxHealth_) {
        currentHealth_ = maxHealth_;
    }
    if (currentHealth_ < 0) {
        currentHealth_ = 0;
    }
    // Update the sprite rects based on the current health
    spriteRects_.clear();
    int parts = currentHealth_ / (maxHealth_ / 5);
    // Healthbar png has 10 parts, index 0-4 is 5 parts of blue health, index 5-9 is 5 parts of red health
    // index 10 is the empty health bar
    const SpriteData* spriteData = SpriteData::getSharedInstance(spriteSheetPath_);
    // If health type is friendly, use blue health bar
    int offset = 0; // 0 for blue health, 5 for red health
    if(enemy_) {
        offset = 5; // Red health bar for enemies
    }

    for (int i = 0; i < parts; ++i) {
        SpriteRect rect = spriteData->getSpriteRect(i + offset);
        spriteRects_.push_back(rect);
    }
    spriteRects_.push_back(spriteData->getSpriteRect(10)); // Add the empty health bar at the end
    // std::cout << "Healthbar updated: " << currentHealth_ << "/" << maxHealth_ << std::endl;
    // std::cout << "SpriteRects: " << spriteRects_.size() << std::endl;
}

std::vector<float> Healthbar::getOffsets(int count) const {
    std::vector<float> offsets;
    if(count > 1) {
        offsets.push_back(-30.0f);
        if (count > 2) {
            offsets.push_back(-15.0f);
        }
        if (count > 3) {
            offsets.push_back(0.0f);
        }
        if (count > 4) {
            offsets.push_back(15.0f);
        }
        if (count > 5) {
            offsets.push_back(30.0f);
        }
    }
    return offsets;
}

void Entity::updateHealthbar() {
    if (healthbar_) {
        // std::cout << "Updating healthbar for entity with ID: " << getObjID() << ", Health: " << health << std::endl;
        healthbar_->setHealth(this->health);
        healthbar_->setposition(getposition() + Vec2(0, -60)); // Position healthbar above entity
    }
}