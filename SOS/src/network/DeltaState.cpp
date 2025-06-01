#include "network/DeltaState.h"
#include "object.h"
#include "objects/player.h"
#include "objects/tile.h"
#include "objects/minotaur.h"
#include <iostream>
#include <algorithm>

ObjectState ObjectState::fromObject(const std::shared_ptr<Object>& obj) {
    ObjectState state;
    
    // Basic properties
    state.id = obj->getObjID();
    state.type = static_cast<uint8_t>(obj->type);
    state.position = obj->getposition();
    state.velocity = obj->getvelocity();
    
    // Type-specific properties
    switch (obj->type) {
        case ObjectType::PLAYER: {
            Player* player = static_cast<Player*>(obj.get());
            state.player.animState = static_cast<uint8_t>(player->getAnimationState());
            state.player.direction = static_cast<uint8_t>(player->getDir());
            break;
        }
        case ObjectType::MINOTAUR: {
            Minotaur* minotaur = static_cast<Minotaur*>(obj.get());
            state.player.animState = static_cast<uint8_t>(minotaur->getAnimationState());
            state.player.direction = static_cast<uint8_t>(minotaur->getDir());
            break;
        }
        case ObjectType::TILE: {
            Tile* tile = static_cast<Tile*>(obj.get());
            state.tile.tileIndex = tile->gettileIndex();
            state.tile.flags = tile->getFlags();
            break;
        }
        default:
            // No additional properties for other types
            break;
    }
    
    return state;
}

bool ObjectState::isDifferentFrom(const ObjectState& other) const {
    // Different object IDs are obviously different
    if (id != other.id || type != other.type)
        return true;

    // Check if position or velocity has changed significantly
    // Using a small epsilon for float comparison to avoid network spam from tiny changes
    const float EPSILON = 0.001f;
    
    if (std::abs(position.x - other.position.x) > EPSILON ||
        std::abs(position.y - other.position.y) > EPSILON ||
        std::abs(velocity.x - other.velocity.x) > EPSILON ||
        std::abs(velocity.y - other.velocity.y) > EPSILON) {
        return true;
    }
    
    // Check type-specific properties
    switch (type) {
        case static_cast<uint8_t>(ObjectType::PLAYER):
        case static_cast<uint8_t>(ObjectType::MINOTAUR):
            if (player.animState != other.player.animState ||
                player.direction != other.player.direction) {
                return true;
            }
            break;
            
        case static_cast<uint8_t>(ObjectType::TILE):
            if (tile.tileIndex != other.tile.tileIndex ||
                tile.flags != other.tile.flags) {
                return true;
            }
            break;
            
        default:
            // For other types, we only compare position and velocity
            break;
    }
    
    // No differences detected
    return false;
}

DeltaStateTracker::DeltaStateTracker() {
    // Initialize empty
}

void DeltaStateTracker::updateState(const std::vector<std::shared_ptr<Object>>& objects) {
    // Clear previous state
    previousObjectStates.clear();
    
    // Store current state of all objects
    for (const auto& obj : objects) {
        if (!obj) continue;
        
        // Create state from object
        ObjectState state = ObjectState::fromObject(obj);
        
        // Store in map
        previousObjectStates[obj->getObjID()] = state;
    }
}

std::vector<std::shared_ptr<Object>> DeltaStateTracker::getChangedObjects(
    const std::vector<std::shared_ptr<Object>>& objects) {
    
    std::vector<std::shared_ptr<Object>> changedObjects;
    std::map<std::string, bool> seenObjects;
    
    // Find objects that are new or changed
    for (const auto& obj : objects) {
        if (!obj) continue;
        
        std::string objId = obj->getObjID();
        seenObjects[objId] = true;
        
        // Create current state
        ObjectState currentState = ObjectState::fromObject(obj);
        
        // Check if object exists in previous state
        auto it = previousObjectStates.find(objId);
        if (it == previousObjectStates.end()) {
            // New object
            changedObjects.push_back(obj);
        } else {
            // Existing object - check for changes
            if (currentState.isDifferentFrom(it->second)) {
                changedObjects.push_back(obj);
            }
        }
    }
    
    // Check for objects that have been removed
    for (const auto& [id, state] : previousObjectStates) {
        if (seenObjects.find(id) == seenObjects.end()) {
            // Object was removed - we need a special mechanism to inform clients
            // This is handled separately via the object removal system
        }
    }
    
    return changedObjects;
}

bool DeltaStateTracker::objectExists(const std::string& objectId) const {
    return previousObjectStates.find(objectId) != previousObjectStates.end();
}

std::vector<std::string> DeltaStateTracker::getAllObjectIds() const {
    std::vector<std::string> ids;
    ids.reserve(previousObjectStates.size());
    
    for (const auto& [id, _] : previousObjectStates) {
        ids.push_back(id);
    }
    
    return ids;
}

void DeltaStateTracker::clear() {
    previousObjectStates.clear();
}
