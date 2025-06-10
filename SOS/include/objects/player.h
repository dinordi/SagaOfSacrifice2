#pragma once

#include "objects/entity.h"
#include "sprite_data.h"
#include "objects/tile.h"
#include "playerInput.h"
#include "animation.h"
#include "AudioManager.h"
class Player : public Entity {

public:
    Player(int x, int y, uint16_t objID, int layer = 8);
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
    
    bool isAttackActive;
    float attackTimer;
    
    AudioManager &audio = AudioManager::Instance();

    // Attack properties
    int attackDamage = 20;
    float attackRange = 100.0f;
    
};
