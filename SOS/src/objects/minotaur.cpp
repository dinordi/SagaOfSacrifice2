#include "objects/minotaur.h"


Minotaur::Minotaur(BoxCollider collider, std::string objID) : Enemy(BoxCollider(0, 0, 96, 96), objID)
 {

    setvelocity(Vec2(0, 0)); // Initialize velocity to zero


    addSpriteSheet(AnimationState::IDLE, new SpriteData("minotaurus_idle", 192, 192, 2), 100, true);
    // addAnimation(AnimationState::IDLE, 0, 1, getCurrentSpriteData()->columns, 250, true);        // Idle animation (1 frames)
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::WALKING, new SpriteData("minotaurus_walk", 192, 192, 8), 150, true);
    // addAnimation(AnimationState::WALKING, 0, 8, 9, 150, true);      // Walking animation (3 frames)
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 3);

    addSpriteSheet(AnimationState::ATTACKING, new SpriteData("minotaurus_slash", 384, 384, 5), 80, false);

    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 1);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 2);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 3);

    // Set initial state
    setAnimationState(AnimationState::IDLE);
}