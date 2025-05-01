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

void AnimationController::update(uint64_t deltaTime) {
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

int AnimationController::getCurrentFrame() const {
    return currentFrame;
}

bool AnimationController::isFinished() const {
    return finished;
}