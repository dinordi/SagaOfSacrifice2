#include "graphics/Camera.h"

Camera::Camera(int screenWidth, int screenHeight)
    : x(0), y(0), width(screenWidth), height(screenHeight)
{
}

Camera::~Camera()
{
}

void Camera::update(std::shared_ptr<Object> target)
{
    if (target)
    {
        // Center the camera on the target object
        x = target->getcollider().position.x - width / 2;
        y = target->getcollider().position.y - height / 2;
    }
}

Vec2 Camera::worldToScreen(float worldX, float worldY) const
{
    return {
        worldX - x,
        worldY - y
    };
}

bool Camera::isVisible(float worldX, float worldY, float objectWidth, float objectHeight) const
{
    // Check if the object is within the camera's view
    return (worldX + objectWidth >= x && worldX <= x + width &&
            worldY + objectHeight >= y && worldY <= y + height);
}

Vec2 Camera::getPosition() const
{
    return {x, y};
}

int Camera::getWidth() const
{
    return width;
}

int Camera::getHeight() const
{
    return height;
}
