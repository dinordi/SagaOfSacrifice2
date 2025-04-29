// SOS/src/game.cpp

#include "game.h"
#include "characters.h"
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include <iostream>
#include <set> // Add missing header for std::set

// External function declaration
extern uint32_t get_ticks(); // Declare the get_ticks function

Game::Game(PlayerInput* input) : running(true), input(input), multiplayerActive(false) {
    
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    // Initialize game objects here
    player = new Player(Vec2(500,100), new SpriteData(std::string("playermap"), 128, 128, 5));
    player->setInput(input);
    objects.push_back(player);

    Platform* floor = new Platform(500, 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor2 = new Platform(500 + (1*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor3 = new Platform(500 + (2*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor4 = new Platform(500 + (3*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    objects.push_back(floor);
    objects.push_back(floor2);

}

Game::~Game() {
    // Clean up game objects
    for(auto object : objects) {
        delete object;
    }
    objects.clear();
    
    delete collisionManager;
    
    // Shutdown multiplayer if active
    shutdownMultiplayer();
}

void Game::mapCharacters()
{
    // Get all the letters from a-z and 0-9
    std::vector<char> letters;
    for (char c = 'a'; c <= 'z'; ++c) {
        letters.push_back(c);
    }
    for (char c = '0'; c <= '9'; ++c) {
        letters.push_back(c);
    }
    int index = 0;
    for(char c : letters) {
        characterMap[c] = index++;
    }
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