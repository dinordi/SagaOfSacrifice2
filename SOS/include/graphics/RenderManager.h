#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <SDL3/SDL.h>
#include "graphics/RenderGrid.h"
#include "graphics/Camera.h"
#include "object.h"

// Manages rendering optimizations and object culling
class RenderManager {
private:
    RenderGrid renderGrid_;
    bool gridInitialized_;
    
    // Performance tracking
    mutable size_t lastFrameObjectCount_;
    mutable size_t totalObjectCount_;
    
    // Layer-based rendering support
    enum RenderLayer {
        BACKGROUND_TILES = 0,
        PLATFORMS = 1,
        ENTITIES = 2,
        EFFECTS = 3,
        UI = 4
    };
    
public:
    RenderManager(float cellSize = 512.0f) 
        : renderGrid_(cellSize), gridInitialized_(false), 
          lastFrameObjectCount_(0), totalObjectCount_(0) {}
    
    // Initialize the render grid with all objects
    void initializeGrid(const std::vector<std::shared_ptr<Object>>& allObjects) {
        // Classify objects as static or dynamic
        for (const auto& obj : allObjects) {
            if (!obj) continue;
            
            // Mark tiles and other static objects
            if (isStaticObject(obj)) {
                renderGrid_.markAsStatic(obj);
            }
        }
        
        // Populate the grid
        renderGrid_.populateGrid(allObjects);
        gridInitialized_ = true;
        totalObjectCount_ = allObjects.size();
    }
    
    // Update dynamic objects (call every frame)
    void updateDynamicObjects(const std::vector<std::shared_ptr<Object>>& allObjects) {
        if (!gridInitialized_) {
            initializeGrid(allObjects);
            return;
        }
        
        // Only update dynamic objects for better performance
        renderGrid_.updateDynamicObjects(allObjects);
    }
    
    // Get objects that should be rendered this frame
    std::vector<std::shared_ptr<Object>> getCulledObjects(const Camera* camera) const {
        if (!gridInitialized_ || !camera) {
            return {};
        }
        
        auto visibleObjects = renderGrid_.getVisibleObjects(camera);
        lastFrameObjectCount_ = visibleObjects.size();
        return visibleObjects;
    }
    
    // Get objects for a specific render layer
    std::vector<std::shared_ptr<Object>> getCulledObjectsByLayer(const Camera* camera, int layer) const {
        if (!gridInitialized_ || !camera) {
            return {};
        }
        
        return renderGrid_.getVisibleObjectsByLayer(camera, layer);
    }
    
    // Render objects with layered approach
    void renderLayered(SDL_Renderer* renderer, const Camera* camera, 
                      const std::unordered_map<std::string, SDL_Texture*>& spriteMap) const {
        
        // Render in layer order for proper depth sorting
        for (int layer = BACKGROUND_TILES; layer <= EFFECTS; layer++) {
            auto layerObjects = getCulledObjectsByLayer(camera, layer);
            renderObjectList(renderer, camera, layerObjects, spriteMap);
        }
    }
    
    // Render a list of objects (helper function)
    void renderObjectList(SDL_Renderer* renderer, const Camera* camera,
                         const std::vector<std::shared_ptr<Object>>& objects,
                         const std::unordered_map<std::string, SDL_Texture*>& spriteMap) const {
        
        for (const auto& entity : objects) {
            if (!entity || !entity->getCurrentSpriteData()) continue;
            
            // Final viewport culling check (more precise than grid culling)
            float entityX = entity->getcollider().position.x - (entity->getCurrentSpriteData()->getSpriteRect(0).w / 2);
            float entityY = entity->getcollider().position.y - (entity->getCurrentSpriteData()->getSpriteRect(0).h / 2);
            float entityWidth = entity->getCurrentSpriteData()->getSpriteRect(0).w;
            float entityHeight = entity->getCurrentSpriteData()->getSpriteRect(0).h;
            
            if (!camera->isVisible(entityX, entityY, entityWidth, entityHeight)) {
                continue;
            }
            
            // Get sprite texture
            SDL_Texture* sprite_tex = nullptr;
            auto it = spriteMap.find(entity->getCurrentSpriteData()->getid_());
            if (it != spriteMap.end()) {
                sprite_tex = it->second;
            } else {
                // Fallback texture
                auto fallback = spriteMap.find("fatbat.png");
                if (fallback != spriteMap.end()) {
                    sprite_tex = fallback->second;
                }
            }
            
            if (!sprite_tex) continue;
            
            // Render the sprite
            renderSprite(renderer, camera, entity, sprite_tex);
        }
    }
    
    // Render a single sprite (extracted for reusability)
    void renderSprite(SDL_Renderer* renderer, const Camera* camera, 
                     std::shared_ptr<Object> entity, SDL_Texture* texture) const {
        
        int spriteIndex = entity->getCurrentSpriteIndex();
        const SpriteRect& currentSpriteRect = entity->getCurrentSpriteData()->getSpriteRect(spriteIndex);
        
        SDL_FRect srcRect = {
            static_cast<float>(currentSpriteRect.x),
            static_cast<float>(currentSpriteRect.y),
            static_cast<float>(currentSpriteRect.w),
            static_cast<float>(currentSpriteRect.h)
        };
        
        // Transform to screen coordinates
        Vec2 screenPos = camera->worldToScreen(
            entity->getcollider().position.x - (currentSpriteRect.w / 2),
            entity->getcollider().position.y - (currentSpriteRect.h / 2)
        );
        
        SDL_FRect destRect = {
            screenPos.x,
            screenPos.y,
            static_cast<float>(currentSpriteRect.w),
            static_cast<float>(currentSpriteRect.h)
        };
        
        SDL_RenderTexture(renderer, texture, &srcRect, &destRect);
    }
    
    // Performance monitoring
    size_t getLastFrameObjectCount() const { return lastFrameObjectCount_; }
    size_t getTotalObjectCount() const { return totalObjectCount_; }
    float getCullRatio() const { 
        if (totalObjectCount_ == 0) return 0.0f;
        return static_cast<float>(lastFrameObjectCount_) / static_cast<float>(totalObjectCount_);
    }
    
    // Debug information
    void logPerformanceStats() const {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, 
            "RenderManager: %zu/%zu objects rendered (%.1f%% culled)", 
            lastFrameObjectCount_, totalObjectCount_, 
            (1.0f - getCullRatio()) * 100.0f);
    }
    
private:
    // Determine if an object should be treated as static
    bool isStaticObject(std::shared_ptr<Object> obj) const {
        if (!obj) return false;
        
        // Check object type - tiles and platforms are typically static
        ObjectType objType = obj->type;
        
        // Assuming these object type constants exist
        // You may need to adjust these based on your actual object type enum
        return (objType == ObjectType::TILE );
    }
};

#endif // RENDER_MANAGER_H
