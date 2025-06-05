#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <ostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "sprite_data.h"


using json = nlohmann::json;

enum class FacingDirection {
    WEST,
    EAST,
    NORTH,
    SOUTH,
    NORTH_WEST,
    NORTH_EAST,
    SOUTH_WEST,
    SOUTH_EAST
};

// << overload for FacingDirection
inline std::ostream& operator<<(std::ostream& os, FacingDirection dir) {
    switch (dir) {
        case FacingDirection::WEST: os << "WEST"; break;
        case FacingDirection::EAST: os << "EAST"; break;
        case FacingDirection::NORTH: os << "NORTH"; break;
        case FacingDirection::SOUTH: os << "SOUTH"; break;
        case FacingDirection::NORTH_WEST: os << "NORTH_WEST"; break;
        case FacingDirection::NORTH_EAST: os << "NORTH_EAST"; break;
        case FacingDirection::SOUTH_WEST: os << "SOUTH_WEST"; break;
        case FacingDirection::SOUTH_EAST: os << "SOUTH_EAST"; break;
    }
    return os;
}

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

// << overload for AnimationState
inline std::ostream& operator<<(std::ostream& os, AnimationState state) {
    switch (state) {
        case AnimationState::IDLE: os << "IDLE"; break;
        case AnimationState::WALKING: os << "WALKING"; break;
        case AnimationState::RUNNING: os << "RUNNING"; break;
        case AnimationState::JUMPING: os << "JUMPING"; break;
        case AnimationState::FALLING: os << "FALLING"; break;
        case AnimationState::ATTACKING: os << "ATTACKING"; break;
        case AnimationState::HURT: os << "HURT"; break;
        case AnimationState::DYING: os << "DYING"; break;
        case AnimationState::CUSTOM: os << "CUSTOM"; break;
    }
    return os;
}

struct FacingDirFrames{
    int firstFrame; // First frame index for this direction
    int lastFrame;  // Last frame index for this direction
};

// Animation definition that stores information about a specific animation
struct AnimationDef {
    int frameCount;        // Number of frames in this animation
    uint32_t frameTime;    // Time per frame in milliseconds
    bool loop;             // Whether the animation should loop
    std::map<FacingDirection, FacingDirFrames> directionRows;

    // Constructor
    AnimationDef(int count = 1, uint32_t time = 100, bool doLoop = true)
        : frameCount(count), frameTime(time), loop(doLoop) {}
};

// Animation controller class to manage entity animations
class AnimationController {
public:
    AnimationController();

    // Cleanup shared resources
    static void cleanupSharedResources();
    
    void addSpriteSheet(const std::string& spriteSheetPath, AnimationState spriteState, uint32_t frameTime = 150);

    // Add an animation definition for a specific state
    void addAnimation(AnimationState state, const AnimationDef& def);
    
    // Set the current animation state
    void setState(AnimationState state);
    
    // Update the animation based on elapsed time
    void update(uint64_t deltaTime, FacingDirection dir = FacingDirection::EAST);
    
    // Get the current sprite index to use for rendering
    int getCurrentFrame(FacingDirection dir = FacingDirection::EAST) const;

    // Get the current animation state
    AnimationState getCurrentState() const;

    // Set direction row map
    void setDirectionRow(AnimationState state, FacingDirection dir, int firstFrame, int lastFrame);

    // Check if the animation has completed (useful for non-looping animations)
    bool isFinished() const;

    SpriteData* getCurrentSpriteData() const;
    
private:
    std::map<AnimationState, AnimationDef> animations;
    std::unordered_map<AnimationState, SpriteData*> spriteSheets;
    
    FacingDirection lastDirection;
    AnimationState currentState;
    AnimationState targetState;
    int currentFrame;
    uint32_t elapsedTime;
    bool finished;
};