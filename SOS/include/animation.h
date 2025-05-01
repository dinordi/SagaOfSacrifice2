#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

// Animation states representing different actions
enum class AnimationState {
    IDLE,
    WALKING,
    RUNNING,
    JUMPING,
    FALLING,
    ATTACKING,
    HURT,
    DYING,
    CUSTOM // For any special animations
};

// Animation definition that stores information about a specific animation
struct AnimationDef {
    int startFrame;        // First frame in the spritesheet for this animation
    int frameCount;        // Number of frames in this animation
    int framesPerRow;      // Frames per row in the spritesheet (usually same as columns in SpriteData)
    uint32_t frameTime;    // Time per frame in milliseconds
    bool loop;             // Whether the animation should loop
    
    // Constructor
    AnimationDef(int start = 0, int count = 1, int fpr = 1, uint32_t time = 100, bool doLoop = true)
        : startFrame(start), frameCount(count), framesPerRow(fpr), frameTime(time), loop(doLoop) {}
};

// Animation controller class to manage entity animations
class AnimationController {
public:
    AnimationController();
    
    // Add an animation definition for a specific state
    void addAnimation(AnimationState state, const AnimationDef& def);
    
    // Set the current animation state
    void setState(AnimationState state);
    
    // Update the animation based on elapsed time
    void update(uint64_t deltaTime);
    
    // Get the current sprite index to use for rendering
    int getCurrentFrame() const;
    
    // Check if the animation has completed (useful for non-looping animations)
    bool isFinished() const;
    
private:
    std::unordered_map<AnimationState, AnimationDef> animations;
    AnimationState currentState;
    int currentFrame;
    uint32_t elapsedTime;
    bool finished;
};