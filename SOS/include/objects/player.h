#pragma once

#include "objects/entity.h"
#include "sprite_data.h"
#include "objects/platform.h"
#include "playerInput.h"
#include "animation.h"

class Player : public Entity {

public:
    Player(Vec2 pos, std::string objID);
    void setInput(PlayerInput* input) { this->input = input; }
    void update(float deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    void handleInput(PlayerInput* input, float deltaTime);
    void takeDamage(int amount);
    void collectItem();
    void applyPhysicsResponse(const Vec2& resolutionVector);
    
private:
    PlayerInput* input;
    // Define player animation states
    void setupAnimations();
    void updateAnimationState();
    bool isMoving() const;
    
    int health;
    bool isAttacking;
    bool isJumping;
    uint32_t attackTimer;
};