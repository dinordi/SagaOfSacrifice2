#ifndef COLLISION_MANAGER_H
#define COLLISION_MANAGER_H

#include <vector>
#include <utility>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include "object.h"
#include "CollisionInfo.h"
#include "CollisionHandler.h"
#include "objects/player.h"

class Game;

// Spatial grid for efficient collision detection
class SpatialGrid {
private:
    float cellSize_;
    std::unordered_map<int64_t, std::vector<Object*>> cells_;
    
    // Convert position to grid cell key
    int64_t getCellKey(float x, float y) {
        int gridX = static_cast<int>(std::floor(x / cellSize_));
        int gridY = static_cast<int>(std::floor(y / cellSize_));
        // Create a unique hash key combining x and y coordinates
        return (static_cast<int64_t>(gridX) << 32) | static_cast<uint32_t>(gridY);
    }
    
public:
    SpatialGrid(float cellSize = 200.0f) : cellSize_(cellSize) {}
    
    void clear() {
        cells_.clear();
    }
    
    // Add an object to all cells it overlaps with
    void addObject(Object* obj) {
        if (!obj) return;
        
        // Calculate which cells this object belongs to
        const BoxCollider& collider = obj->getcollider();
        Vec2 pos = collider.position;
        Vec2 size = collider.size;
        
        // Get all cells this object overlaps with
        int startX = static_cast<int>(std::floor(pos.x / cellSize_));
        int startY = static_cast<int>(std::floor(pos.y / cellSize_));
        int endX = static_cast<int>(std::floor((pos.x + size.x) / cellSize_));
        int endY = static_cast<int>(std::floor((pos.y + size.y) / cellSize_));
        
        // Add object to all overlapping cells
        for (int x = startX; x <= endX; x++) {
            for (int y = startY; y <= endY; y++) {
                int64_t key = (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
                cells_[key].push_back(obj);
            }
        }
    }
    
    // Get all objects that might collide with the given object
    std::vector<Object*> getPotentialColliders(Object* obj) {
        if (!obj) return {};
        
        std::vector<Object*> result;
        std::unordered_set<Object*> uniqueObjects; // To avoid duplicates
        
        const BoxCollider& collider = obj->getcollider();
        Vec2 pos = collider.position;
        Vec2 size = collider.size;
        
        // Get all cells this object overlaps with
        int startX = static_cast<int>(std::floor(pos.x / cellSize_));
        int startY = static_cast<int>(std::floor(pos.y / cellSize_));
        int endX = static_cast<int>(std::floor((pos.x + size.x) / cellSize_));
        int endY = static_cast<int>(std::floor((pos.y + size.y) / cellSize_));
        
        // Check all objects in overlapping cells
        for (int x = startX; x <= endX; x++) {
            for (int y = startY; y <= endY; y++) {
                int64_t key = (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
                
                auto it = cells_.find(key);
                if (it != cells_.end()) {
                    for (Object* other : it->second) {
                        if (other != obj && uniqueObjects.find(other) == uniqueObjects.end()) {
                            if(other->isCollidable())
                            {
                                uniqueObjects.insert(other);
                                result.push_back(other);
                            }
                        }
                    }
                }
            }
        }
        
        return result;
    }

    // Get all objects overlapping a given region (x1,y1)-(x2,y2)
std::vector<Object*> getObjectsInRegion(float minX, float minY, float maxX, float maxY) {
    std::vector<Object*> result;
    std::unordered_set<Object*> uniqueObjects;

    int startX = static_cast<int>(std::floor(minX / cellSize_));
    int startY = static_cast<int>(std::floor(minY / cellSize_));
    int endX = static_cast<int>(std::floor(maxX / cellSize_));
    int endY = static_cast<int>(std::floor(maxY / cellSize_));

    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            int64_t key = (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
            
            auto it = cells_.find(key);
            if (it != cells_.end()) {
                for (Object* other : it->second) {
                    if (uniqueObjects.find(other) == uniqueObjects.end()) {
                        uniqueObjects.insert(other);
                        result.push_back(other);
                    }
                }
            }
        }
    }

    return result;
}

};

class CollisionManager {
public:
    // Main collision detection function - takes Game reference
    static std::vector<std::pair<Object*, Object*>> detectCollisions(Game& game);
    
    // Player-specific collision detection function - takes Game reference  
    static std::vector<std::pair<Object*, Object*>> detectPlayerCollisions(Game& game);

private:
    static void resolveCollision(Object* objA, Object* objB, const CollisionInfo& info);
    static bool checkAndResolveCollision(Object* objA, Object* objB, 
                                       std::vector<std::pair<Object*, Object*>>& collisions, 
                                       int& collisionChecks);
};

#endif // COLLISION_MANAGER_H
