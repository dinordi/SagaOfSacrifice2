// SOS/src/game.cpp

#include "game.h"

Game::Game() : running(true) {
    // Initialize game objects here
    Player* player = new Player(Vec2(500,100), new SpriteData(1, 128, 128));
    objects.push_back(player);

    Platform* floor = new Platform(4, 500, 900);
    objects.push_back(floor);
}

Game::~Game() {
    // Clean up game objects here
}

void Game::update(uint64_t deltaTime) {

    //Every second print "Hello World"
    static uint64_t timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1000.0f) {
        // std::cout << "Hello World" << std::endl;
        timeseconds = 0.0f;
    }

    // Update game logic here
    for (Object* object : objects) {
        object->update(deltaTime);
        // Check for collisions with other objects
        for (Object* other : objects) {
            if (object != other && object->collisionWith(other)) {
                // Handle collision
                // std::cout << "Collision detected!" << std::endl;
                if(object->type == ObjectType::ENTITY && other->type == ObjectType::PLATFORM) {
                    // Handle platform collision with player
                    Platform* platform = static_cast<Platform*>(other);
                    object->handlePlatformCollision(platform);
                }
            }
        }
    }
}

void Game::render() {
    // Render game objects here
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}