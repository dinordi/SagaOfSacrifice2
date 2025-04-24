// SOS/src/game.cpp

#include "game.h"
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include <iostream>

Game::Game(PlayerInput* input) : running(true), input(input), multiplayerActive(false) {
    
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    // Initialize game objects here
    player = new Player(Vec2(500,100), new SpriteData(1, 128, 128));
    objects.push_back(player);

    Platform* floor = new Platform(4, 500, 850, new SpriteData(4, 128, 128));
    Platform* floor2 = new Platform(4, 700, 850, new SpriteData(4, 15, 15));
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

void Game::update(uint64_t deltaTime) {
    //Every second print "Hello World"
    static uint64_t timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1000.0f) {
        // std::cout << "Hello World" << std::endl;
        timeseconds = 0.0f;
    }
    
    // Handle input and update local player
    input->readInput();
    player->handleInput(input, deltaTime);
    
    // Update all local game objects
    for(auto object : objects) {
        object->update(deltaTime);
    }

    // Perform collision detection for local objects
    collisionManager->detectCollisions(objects);
    
    // Handle multiplayer functionality if active
    if (multiplayerActive && multiplayerManager) {
        // Update the network state
        multiplayerManager->update(deltaTime);
        
        // Update remote players
        updateRemotePlayers(deltaTime);
    }
}

void Game::render() {
    // Render game objects here
    
    // Render remote players if multiplayer is active
    if (multiplayerActive) {
        renderRemotePlayers();
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}

// Multiplayer functionality
bool Game::initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId) {
    if (multiplayerActive) {
        std::cout << "Multiplayer is already active" << std::endl;
        return true;
    }
    
    multiplayerManager = std::make_unique<MultiplayerManager>();
    
    bool success = multiplayerManager->initialize(serverAddress, serverPort, playerId);
    if (success) {
        // Set the local player reference
        multiplayerManager->setLocalPlayer(player);
        
        // Set up chat message handling
        multiplayerManager->setChatMessageHandler([this](const std::string& sender, const std::string& message) {
            std::cout << "Chat from " << sender << ": " << message << std::endl;
        });
        
        multiplayerActive = true;
        std::cout << "Multiplayer initialized successfully" << std::endl;
    } else {
        std::cerr << "Failed to initialize multiplayer" << std::endl;
        multiplayerManager.reset();
    }
    
    return success;
}

void Game::shutdownMultiplayer() {
    if (!multiplayerActive) return;
    
    if (multiplayerManager) {
        multiplayerManager->shutdown();
    }
    
    multiplayerManager.reset();
    multiplayerActive = false;
    
    std::cout << "Multiplayer shut down" << std::endl;
}

bool Game::isMultiplayerActive() const {
    return multiplayerActive && multiplayerManager && multiplayerManager->isConnected();
}

void Game::sendChatMessage(const std::string& message) {
    if (isMultiplayerActive()) {
        multiplayerManager->sendChatMessage(message);
    }
}

void Game::setChatMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    if (multiplayerManager) {
        multiplayerManager->setChatMessageHandler(handler);
    }
}

void Game::handleNetworkMessages() {
    // This is handled internally by the MultiplayerManager in its update() method
}

void Game::updateRemotePlayers(uint64_t deltaTime) {
    // The remote players are updated internally by the MultiplayerManager
    if (multiplayerManager) {
        const auto& remotePlayers = multiplayerManager->getRemotePlayers();
        mpl_objects.clear();
        for (const auto& pair : remotePlayers) {
            // Get the remote player
            RemotePlayer* remotePlayer = pair.second.get();
            
            mpl_objects.push_back(remotePlayer);
            
            // For debugging, just print the remote player positions
            std::cout << "Remote player " << remotePlayer->spriteData->ID
                      << " at position (" << remotePlayer->position.x
                      << ", " << remotePlayer->position.y << ")" << std::endl;
        }
    }
}

void Game::renderRemotePlayers() {
    // This function would integrate with your rendering system to draw remote players
    // Since we don't have direct access to the rendering system in this implementation,
    // this is a placeholder for where you would render each remote player
    
    if (multiplayerManager) {
        const auto& remotePlayers = multiplayerManager->getRemotePlayers();
        for (const auto& pair : remotePlayers) {
            // Get the remote player
            RemotePlayer* remotePlayer = pair.second.get();
            
            // objects.push_back(remotePlayer);
            
            // For debugging, just print the remote player positions
            // std::cout << "Remote player " << remotePlayer->getId() 
            //           << " at position (" << remotePlayer->getPosition().x 
            //           << ", " << remotePlayer->getPosition().y << ")" << std::endl;
        }
    }
}