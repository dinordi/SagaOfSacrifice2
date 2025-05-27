#include "animation.h"
#include <iostream>
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

    if (animations.find(state) == animations.end()) {
        std::cerr << "Animation state " << static_cast<int>(state) << " not found!" << std::endl;
        return; // No animation defined for this state
    }
    AnimationDef def = animations.find(state)->second;
    auto dirIt = def.directionRows.find(lastDirection);
    if (dirIt == def.directionRows.end()) {
        std::cerr << "Direction " << static_cast<int>(lastDirection) << " not found in animation definition!" << std::endl;
        return;
    }
    FacingDirFrames& fdf = dirIt->second;

    if (state != currentState || finished) {
        currentState = state;
        currentFrame = fdf.firstFrame;
        elapsedTime = 0;
        finished = false;
    }
}

void AnimationController::update(uint64_t deltaTime, FacingDirection dir) {
    if (animations.find(currentState) == animations.end()) {
        // No animation found for this state, use default
        return;
    }
    lastDirection = dir; // Update last direction

    const AnimationDef& currentAnim = animations[currentState];
    elapsedTime += deltaTime;

    // Time to advance to the next frame?
    if (elapsedTime >= currentAnim.frameTime) {
        int totalFramesPassed = elapsedTime / currentAnim.frameTime;
        elapsedTime %= currentAnim.frameTime;
        
        int frameOffset = (currentFrame) + totalFramesPassed;
        
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
        
        // std::cout << "Animation updated: " << currentState << ", Frame offset: " << frameOffset << std::endl;
        // Set the actual frame index
        // Set the actual frame index - use safe map access
        auto dirIt = currentAnim.directionRows.find(dir);
        if (dirIt != currentAnim.directionRows.end()) {
            currentFrame = frameOffset + dirIt->second.firstFrame;
        } else {
            // Fallback if direction not found
            currentFrame = frameOffset;
        }
    }
}

int AnimationController::getCurrentFrame(FacingDirection dir) const {
    auto it = animations.find(currentState);
    if (it == animations.end()) {
        // std::cout << "No animation found for state: " << (currentState) << std::endl;
        return 0;
    }

    const AnimationDef& def = it->second;

    int baseFrame = currentFrame;

    int rowOffset = 0;
    auto dirIt = def.directionRows.find(dir);
    if (dirIt != def.directionRows.end()) {
        // dirIt has FacingDirFrames with firstFrame and lastFrame
        const FacingDirFrames& frames = dirIt->second;
        // Calculate the row offset based on the current frame
        rowOffset = (frames.firstFrame + (baseFrame - frames.firstFrame));
    }

    return rowOffset;
}

bool AnimationController::isFinished() const {
    return finished;
}

AnimationState AnimationController::getCurrentState() const {
    return currentState;
}

void AnimationController::setDirectionRow(AnimationState state, FacingDirection dir, int firstFrame, int lastFrame) {
    auto it = animations.find(state);
    if (it != animations.end()) {
        it->second.directionRows[dir] = {
            firstFrame,
            lastFrame
        };
    }
}

SpriteData* AnimationController::getCurrentSpriteData() const {
    auto it = spriteSheets.find(currentState);
    if (it != spriteSheets.end()) {
        return (it->second);
    }
    // Return an empty SpriteData if no sprite sheet is found
    return nullptr;
}

void AnimationController::addSpriteSheet(const std::string& spriteSheetPath, AnimationState spriteState, uint32_t frameTime) {
    // Create a new SpriteData object and add it to the spriteSheets map
    SpriteData* spriteData = new SpriteData(spriteSheetPath);
    spriteSheets[spriteState] = spriteData;
    AnimationDef def;
    def.frameCount = spriteData->spriteRects.size() / 4;
    // std::cout << "Adding sprite sheet for state: " << spriteState << " with frame count: " << def.frameCount << std::endl;
    def.frameTime = frameTime; // Default frame time, can be adjusted later
    def.loop = true; // Default to looping animations
    animations[spriteState] = def;
}