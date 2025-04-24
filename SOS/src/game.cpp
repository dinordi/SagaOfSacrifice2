// SOS/src/game.cpp

#include "game.h"
#include "characters.h"

Game::Game(PlayerInput* input) : running(true), input(input) {
    
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    // Initialize game objects here
    player = new Player(Vec2(500,100), new SpriteData(1, 128, 128));
    objects.push_back(player);

    Platform* floor = new Platform(500, 850, new SpriteData(5, 128, 128));
    Platform* floor2 = new Platform(700, 850, new SpriteData(5, 15, 15));
    objects.push_back(floor);
    objects.push_back(floor2);

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
    input->readInput();
    player->handleInput(input, deltaTime);
    // Update player input here
    // For example, check if the jump button is pressed
    // if (input->get_jump()) {
    //     // Handle jump action
    //     Player* player = static_cast<Player*>(objects[0]);
    //     player->velocity.y -= 2.0f; // Apply jump force
    // }
    for(auto object : objects) {
        object->update(deltaTime);
    }

    collisionManager->detectCollisions(objects);
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