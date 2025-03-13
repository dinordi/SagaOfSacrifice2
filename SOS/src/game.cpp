// SOS/src/game.cpp

#include "game.h"

Game::Game() : running(true) {
    // Initialize game objects here
}

Game::~Game() {
    // Clean up game objects here
}

void Game::update(float deltaTime) {

    //Every second print "Hello World"
    static float timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1.0f) {
        std::cout << "Hello World" << std::endl;
        timeseconds = 0.0f;
    }

    // Update game logic here
    for (Object* object : objects) {
        // Update each object
    }
}

void Game::render() {
    // Render game objects here
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*> Game::getObjects() const {
    return objects;
}