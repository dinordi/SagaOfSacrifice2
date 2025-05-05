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

Game::Game(PlayerInput* input, std::string playerID) : running(true), input(input), multiplayerActive(false) {
    
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    // Initialize game objects here
    player = new Player(Vec2(500,100), new SpriteData(std::string("playermap"), 128, 128, 5), playerID);
    player->setInput(input);
    objects.push_back(player);

    Platform* floor = new Platform(500, 850, new SpriteData(std::string("tiles"), 128, 128, 4), generateRandomPlayerId());
    Platform* floor2 = new Platform(500 + (1*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4), generateRandomPlayerId());
    Platform* floor3 = new Platform(500 + (2*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4), generateRandomPlayerId());
    Platform* floor4 = new Platform(500 + (3*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4), generateRandomPlayerId());
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

std::string Game::generateRandomPlayerId() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int idLength = 8;
    std::string result;
    result.reserve(idLength);
    
    for (int i = 0; i < idLength; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return result;
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
    for(auto object : objects) {
        object->update(deltaTime);
    }

    collisionManager->detectCollisions(objects);
    // Handle multiplayer functionality if active
    if (multiplayerActive && multiplayerManager) {
        // Update the network state
        multiplayerManager->update(deltaTime);
        
        // Update remote players
        updateRemotePlayers(multiplayerManager->getRemotePlayers());
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}

bool Game::initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId) {
    if (!multiplayerManager) {
        multiplayerManager = std::make_unique<MultiplayerManager>();
    }
    
    bool success = multiplayerManager->initialize(serverAddress, serverPort, playerId);
    
    if (success) {
        multiplayerActive = true;
        multiplayerManager->setLocalPlayer(player);
        std::cout << "[Game] Multiplayer initialized successfully" << std::endl;
    } else {
        std::cerr << "[Game] Failed to initialize multiplayer" << std::endl;
    }
    
    return success;
}

void Game::shutdownMultiplayer() {
    if (multiplayerManager && multiplayerActive) {
        multiplayerManager->shutdown();
        multiplayerActive = false;
        std::cout << "[Game] Multiplayer shut down" << std::endl;
    }
}

bool Game::isMultiplayerActive() const {
    return multiplayerActive && multiplayerManager && multiplayerManager->isConnected();
}

std::vector<Actor*>& Game::getActors() {
    return actors;
}

void Game::sendChatMessage(const std::string& message) {
    if (multiplayerActive && multiplayerManager) {
        multiplayerManager->sendChatMessage(message);
    }
}

void Game::setChatMessageHandler(std::function<void(const std::string& sender, const std::string& message)> handler) {
    if (multiplayerManager) {
        multiplayerManager->setChatMessageHandler(handler);
    }
}

void Game::updateRemotePlayers(const std::map<std::string, std::unique_ptr<RemotePlayer>>& remotePlayers) {

    // loop through all remote players
    for (const auto& pair : remotePlayers) {
        //Find the remote player in the gameobjects
        //If not found, create a new one, otherwise continue
        auto it = std::find_if(objects.begin(), objects.end(), [&](Object* obj) {
            // std::cout << "obj id: " << obj->spriteData->getid_() 
            //         << "remote id: " << pair.first << std::endl;
            return obj->getObjID() == pair.first; // Assuming Object has a getId() method
        });
        if (it == objects.end()) {
            // Create a new remote player
            std::cout << "[Game] Creating new remote player: " << pair.first << std::endl;
            RemotePlayer* remotePlayer = new RemotePlayer(pair.first);
            remotePlayer->setposition(pair.second->getposition());
            remotePlayer->setvelocity(pair.second->getvelocity());
            objects.push_back(remotePlayer);
        } else {
            // Update existing remote player
            (*it)->setposition(pair.second->getposition());
            (*it)->setvelocity(pair.second->getvelocity());
        }
    
    }

}

void Game::handleNetworkMessages() {
    // This method can be implemented later if needed
    // Currently it's only declared in the header but not called
}

void Game::drawWord(const std::string& word, int x, int y) {
    // This method can be implemented later if needed
    // Currently it's only declared in the header but not called
}