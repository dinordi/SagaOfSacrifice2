#pragma once

#include "objects/entity.h"
#include "sprite_data.h"
#include "objects/tile.h"
#include "playerInput.h"
#include "animation.h"

class Player : public Entity {

public:
    Player(int x, int y, std::string objID, int layer = 3);
    void setInput(PlayerInput* input) { this->input = input; }
    void update(float deltaTime) override;
    void accept(CollisionVisitor& visitor) override;
    void handleInput(PlayerInput* input, float deltaTime);
    void takeDamage(int amount);
    void collectItem();
    void applyPhysicsResponse(const Vec2& resolutionVector);
    
    // Attack methods
    void attack();
    bool checkAttackHit(Object* target);
    bool isAttacking() const { return isAttackActive; }
    int getAttackDamage() const { return attackDamage; }
    float getAttackRange() const { return attackRange; }
    
    
private:
    PlayerInput* input;
    // Define player animation states
    void setupAnimations();
    void updateAnimationState();
    bool isMoving() const;
    void updateDirectionFromVelocity(const Vec2& vel);
    
    int health;
    bool isAttackActive;
    bool isJumping;
    float attackTimer;
    
    // Attack properties
    int attackDamage = 20;
    float attackRange = 100.0f;
    
};
