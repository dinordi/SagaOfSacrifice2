#pragma once

#include "Vec2.h"
#include "object.h"

class Camera {
public:
    Camera(int screenWidth, int screenHeight);
    ~Camera();

    // Update camera position to follow target object
    void update(std::shared_ptr<Object> target);
    
    // Convert world coordinates to screen coordinates
    Vec2 worldToScreen(float worldX, float worldY) const;
    
    // Check if a rectangle in world coordinates is visible on screen
    bool isVisible(float x, float y, float width, float height) const;
    
    // Get camera position
    Vec2 getPosition() const;
    
    // Get camera dimensions
    int getWidth() const;
    int getHeight() const;

private:
    float x, y;             // Camera position in world coordinates
    int width, height;      // Camera dimensions
};
