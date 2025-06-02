#pragma once

#include "objects/entity.h"
#include "sprite_data.h"
#include "objects/tile.h"
#include "playerInput.h"
#include "animation.h"

class Player : public Entity {

public:
    Player(int x, int y, std::string objID);
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
    
    // Remote player functionality
    void setTargetPosition(const Vec2& position) { targetPosition = position; }
    void setTargetVelocity(const Vec2& velocity) { targetVelocity = velocity; }
    void resetInterpolation() { interpolationTime = 0.0f; }
    void setOrientation(float orientation) { this->orientation = orientation; }
    void setState(int state) { this->state = state; }
    
    // Getters for remote player functionality
    float getOrientation() const { return orientation; }
    int getState() const { return state; }
    const Vec2& getTargetPosition() const { return targetPosition; }
    const Vec2& getTargetVelocity() const { return targetVelocity; }
    float getInterpolationTime() const { return interpolationTime; }
    
    // Set if player is controlled locally or remotely
    void setIsRemote(bool isRemote) { this->isRemote = isRemote; }
    bool getIsRemote() const { return isRemote; }
    
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
    
    // Remote player state
    bool isRemote = false;
    float orientation = 0.0f;
    int state = 0;
    
    // Client-side interpolation variables
    Vec2 targetPosition;
    Vec2 targetVelocity;
    float interpolationTime = 0.0f;
};
