#include "animation.h"

AnimationController::AnimationController()
    : currentState(AnimationState::IDLE), currentFrame(0), elapsedTime(0), finished(false) {
    // Default constructor - start with IDLE state
}

void AnimationController::addAnimation(AnimationState state, const AnimationDef& def) {
    animations[state] = def;
}

void AnimationController::setState(AnimationState state) {
    // Only change state if it's different from current state
    // or if the current animation is finished
    if (state != currentState || finished) {
        currentState = state;
        currentFrame = animations[state].startFrame;
        elapsedTime = 0;
        finished = false;
    }
}

void AnimationController::update(uint64_t deltaTime, FacingDirection dir) {
    if (animations.find(currentState) == animations.end()) {
        // No animation found for this state, use default
        return;
    }

    const AnimationDef& currentAnim = animations[currentState];
    elapsedTime += deltaTime;

    // Time to advance to the next frame?
    if (elapsedTime >= currentAnim.frameTime) {
        int totalFramesPassed = elapsedTime / currentAnim.frameTime;
        elapsedTime %= currentAnim.frameTime;
        
        int frameOffset = (currentFrame - currentAnim.startFrame) + totalFramesPassed;
        
        // Handle looping or stopping animations
        if (frameOffset >= currentAnim.frameCount) {
            if (currentAnim.loop) {
                // Loop back to start
                frameOffset = frameOffset % currentAnim.frameCount;
            } else {
                // Stop at last frame
                frameOffset = currentAnim.frameCount - 1;
                finished = true;
            }
        }
        
        // Set the actual frame index
        currentFrame = currentAnim.startFrame + frameOffset;
    }
}

int AnimationController::getCurrentFrame(FacingDirection dir) const {
    auto it = animations.find(currentState);
    if (it == animations.end()) {
        return 0;
    }

    const AnimationDef& def = it->second;

    int baseFrame = def.startFrame + (currentFrame % def.frameCount);

    int rowOffset = 0;
    auto dirIt = def.directionRows.find(dir);
    if (dirIt != def.directionRows.end()) {
        rowOffset = dirIt->second * def.framesPerRow;
    }
    return baseFrame + rowOffset;
}

bool AnimationController::isFinished() const {
    return finished;
}

AnimationState AnimationController::getCurrentState() const {
    return currentState;
}

void AnimationController::setDirectionRow(AnimationState state, FacingDirection dir, int row) {
    auto it = animations.find(state);
    if (it != animations.end()) {
        it->second.directionRows[dir] = row;
    }
}