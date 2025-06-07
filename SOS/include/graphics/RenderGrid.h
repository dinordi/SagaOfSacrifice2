#ifndef RENDER_GRID_H
#define RENDER_GRID_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cmath>
#include "object.h"
#include "graphics/Camera.h"

// Spatial grid optimized for rendering operations
class RenderGrid {
private:
    float cellSize_;
    std::unordered_map<int64_t, std::vector<std::shared_ptr<Object>>> cells_;
    
    // Separate storage for static and dynamic objects
    std::unordered_map<int64_t, std::vector<std::shared_ptr<Object>>> staticCells_;
    std::unordered_map<int64_t, std::vector<std::shared_ptr<Object>>> dynamicCells_;
    
    // Track which objects are static vs dynamic
    std::unordered_set<std::shared_ptr<Object>> staticObjects_;
    
    // Convert position to grid cell key
    int64_t getCellKey(float x, float y) const {
        int gridX = static_cast<int>(std::floor(x / cellSize_));
        int gridY = static_cast<int>(std::floor(y / cellSize_));
        return (static_cast<int64_t>(gridX) << 32) | static_cast<uint32_t>(gridY);
    }
    
    // Get all cell keys that intersect with a rectangle
    std::vector<int64_t> getCellKeysInRegion(float x, float y, float width, float height) const {
        std::vector<int64_t> keys;
        
        int startX = static_cast<int>(std::floor(x / cellSize_));
        int startY = static_cast<int>(std::floor(y / cellSize_));
        int endX = static_cast<int>(std::floor((x + width) / cellSize_));
        int endY = static_cast<int>(std::floor((y + height) / cellSize_));
        
        for (int gridX = startX; gridX <= endX; gridX++) {
            for (int gridY = startY; gridY <= endY; gridY++) {
                keys.push_back((static_cast<int64_t>(gridX) << 32) | static_cast<uint32_t>(gridY));
            }
        }
        
        return keys;
    }
    
    void addObjectToCells(std::shared_ptr<Object> obj, std::unordered_map<int64_t, std::vector<std::shared_ptr<Object>>>& targetCells) {
        if (!obj || !obj->getCurrentSpriteData()) return;
        
        // Get object bounds
        const BoxCollider& collider = obj->getcollider();
        Vec2 pos = collider.position;
        Vec2 size = collider.size;
        
        // If size is zero, use sprite dimensions
        if (size.x <= 0 || size.y <= 0) {
            const SpriteRect& spriteRect = obj->getCurrentSpriteData()->getSpriteRect(0);
            size.x = spriteRect.w;
            size.y = spriteRect.h;
        }
        
        // Get all cells this object overlaps with
        auto cellKeys = getCellKeysInRegion(pos.x, pos.y, size.x, size.y);
        
        // Add object to all overlapping cells
        for (int64_t key : cellKeys) {
            targetCells[key].push_back(obj);
        }
    }
    
public:
    RenderGrid(float cellSize = 512.0f) : cellSize_(cellSize) {}
    
    void clear() {
        cells_.clear();
        staticCells_.clear();
        dynamicCells_.clear();
    }
    
    void clearDynamic() {
        dynamicCells_.clear();
    }
    
    // Mark an object as static (tiles, background elements that don't move)
    void markAsStatic(std::shared_ptr<Object> obj) {
        staticObjects_.insert(obj);
    }
    
    // Add all objects to the grid, separating static and dynamic
    void populateGrid(const std::vector<std::shared_ptr<Object>>& objects) {
        clear();
        
        for (const auto& obj : objects) {
            if (!obj) continue;
            
            if (staticObjects_.find(obj) != staticObjects_.end()) {
                // Add to static grid
                addObjectToCells(obj, staticCells_);
            } else {
                // Add to dynamic grid
                addObjectToCells(obj, dynamicCells_);
            }
        }
    }
    
    // Update only dynamic objects (called every frame)
    void updateDynamicObjects(const std::vector<std::shared_ptr<Object>>& objects) {
        clearDynamic();
        
        for (const auto& obj : objects) {
            if (!obj) continue;
            
            // Only update objects that aren't marked as static
            if (staticObjects_.find(obj) == staticObjects_.end()) {
                addObjectToCells(obj, dynamicCells_);
            }
        }
    }
    
    // Get all objects visible in the camera's viewport
    std::vector<std::shared_ptr<Object>> getVisibleObjects(const Camera* camera) const {
        if (!camera) return {};
        
        std::vector<std::shared_ptr<Object>> result;
        std::unordered_set<std::shared_ptr<Object>> uniqueObjects;
        
        // Get camera bounds
        Vec2 camPos = camera->getPosition();
        float camWidth = camera->getWidth();
        float camHeight = camera->getHeight();
        
        // Add some padding to avoid popping at edges
        float padding = cellSize_ * 0.5f;
        float queryX = camPos.x - padding;
        float queryY = camPos.y - padding;
        float queryWidth = camWidth + (padding * 2);
        float queryHeight = camHeight + (padding * 2);
        
        // Get all cells that intersect with the camera viewport
        auto cellKeys = getCellKeysInRegion(queryX, queryY, queryWidth, queryHeight);
        
        // Collect objects from static cells
        for (int64_t key : cellKeys) {
            auto it = staticCells_.find(key);
            if (it != staticCells_.end()) {
                for (const auto& obj : it->second) {
                    if (uniqueObjects.find(obj) == uniqueObjects.end()) {
                        uniqueObjects.insert(obj);
                        result.push_back(obj);
                    }
                }
            }
        }
        
        // Collect objects from dynamic cells
        for (int64_t key : cellKeys) {
            auto it = dynamicCells_.find(key);
            if (it != dynamicCells_.end()) {
                for (const auto& obj : it->second) {
                    if (uniqueObjects.find(obj) == uniqueObjects.end()) {
                        uniqueObjects.insert(obj);
                        result.push_back(obj);
                    }
                }
            }
        }
        
        return result;
    }
    
    // Get objects in a specific layer (for layered rendering)
    std::vector<std::shared_ptr<Object>> getVisibleObjectsByLayer(const Camera* camera, int layer) const {
        auto visibleObjects = getVisibleObjects(camera);
        std::vector<std::shared_ptr<Object>> layerObjects;
        
        for (const auto& obj : visibleObjects) {
            // Assuming objects have a getLayer() method or we determine layer by type
            // For now, we'll use a simple heuristic based on object type
            if (obj->getLayer() == layer) {
                layerObjects.push_back(obj);
            }
        }
        
        return layerObjects;
    }
    
    float getCellSize() const { return cellSize_; }
    
    // Debug information
    size_t getStaticCellCount() const { return staticCells_.size(); }
    size_t getDynamicCellCount() const { return dynamicCells_.size(); }
    size_t getStaticObjectCount() const { return staticObjects_.size(); }
};

#endif // RENDER_GRID_H
