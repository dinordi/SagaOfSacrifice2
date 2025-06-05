#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include "Vec2.h"

// Forward declarations
class Object;

// Represents a snapshot of an object's state for delta comparison
struct ObjectState {
    std::string id;
    uint8_t type;
    Vec2 position;
    Vec2 velocity;
    
    // Additional type-specific state
    union {
        struct {
            uint8_t animState;
            uint8_t direction;
            int16_t health; // For player and minotaur
        } player;
        
        struct {
            uint8_t tileIndex;
            uint32_t flags;
        } tile;
    };
    
    // Create state from an object
    static ObjectState fromObject(const std::shared_ptr<Object>& obj);
    
    // Check if this state differs from another state
    bool isDifferentFrom(const ObjectState& other) const;
};

// Manages tracking of delta states between updates
class DeltaStateTracker {
public:
    DeltaStateTracker();
    
    // Update stored state from current objects
    void updateState(const std::vector<std::shared_ptr<Object>>& objects);
    
    // Get objects that have changed since last update
    std::vector<std::shared_ptr<Object>> getChangedObjects(const std::vector<std::shared_ptr<Object>>& objects);
    
    // Check if an object ID exists in the latest state
    bool objectExists(const uint16_t objectId) const;
    
    // Get all object IDs in previous state
    std::vector<uint16_t> getAllObjectIds() const;
    
    // Clear all tracked states
    void clear();

private:
    // Map of object ID to its last known state
    std::map<uint16_t, ObjectState> previousObjectStates;
};
