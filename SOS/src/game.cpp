// SOS/src/game.cpp

#include "game.h"
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
    if (isPaused) {
        return;
    }

    // Update game objects
    player->update(deltaTime);

    input->readInput();
    player->handleInput(input, deltaTime);    
    
    // Update multiplayer if active
    if (isMultiplayerActive()) {
        multiplayerManager->update(deltaTime);
        
        // Get and update remote players
        const auto& remotePlayers = multiplayerManager->getRemotePlayers();

        // Debug remote players
        static uint64_t lastDebugTime = 0;
        // std::cout << ("[Game] Get ticks: " + std::to_string(get_ticks()));
        if (get_ticks() - lastDebugTime > 3000) { // Every 3 seconds
            std::cout << "[Game] Current game objects: " << objects.size() << std::endl;
            std::cout << "[Game] Remote players from multiplayer manager: " << remotePlayers.size() << std::endl;
            
            // Print remote player IDs and positions
            for (const auto& pair : remotePlayers) {
                std::cout << "[Game] Remote player ID: " << static_cast<int>(pair.first) 
                          << " at position: (" << pair.second->position.x << ", " << pair.second->position.y << ")" << std::endl;
            }
            
            lastDebugTime = get_ticks();
        }
        
        // Make sure all remote players are in the objects list
        updateRemotePlayers(remotePlayers);
    }
    else
    {
        std::cout << "No multiplayer" << std::endl;
    }

    // Handle other game logic...
    collisionManager->detectCollisions(objects);
}

// void Game::render() {
//     // Render game objects here
    
//     // Render remote players if multiplayer is active
//     // if (multiplayerActive) {
//     //     renderRemotePlayers();
//     // }
// }

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}

// Multiplayer functionality
bool Game::initializeMultiplayer(const std::string& serverAddress, int serverPort, const uint8_t playerId) {
    // Create multiplayer manager if it doesn't exist
    if (!multiplayerManager) {
        multiplayerManager = std::make_unique<MultiplayerManager>();
    }
    
    // Connect to server
    bool success = multiplayerManager->initialize(serverAddress, serverPort, playerId);
    if (success) {
        std::cout << "[Game] Multiplayer initialized successfully" << std::endl;
        
        // Set the local player for the multiplayer manager
        multiplayerManager->setLocalPlayer(player);
        std::cout << "[Game] Set local player in multiplayer manager" << std::endl;
        
        // Set up chat message handler if needed
        multiplayerManager->setChatMessageHandler([this](const uint8_t senderId, const std::string& message) {
            std::cout << "[Game] Chat from " << senderId << ": " << message << std::endl;
            // Handle chat message in game UI
        });
    } else {
        std::cerr << "[Game] Failed to initialize multiplayer" << std::endl;
    }
    
    return success;
}

void Game::shutdownMultiplayer() {
    if (multiplayerManager) {
        std::cout << "[Game] Shutting down multiplayer" << std::endl;
        multiplayerManager->shutdown();
    }
}

bool Game::isMultiplayerActive() const {
    return multiplayerManager && multiplayerManager->isConnected();
}

void Game::sendChatMessage(const std::string& message) {
    if (isMultiplayerActive()) {
        multiplayerManager->sendChatMessage(message);
    }
}

void Game::setChatMessageHandler(std::function<void(const uint8_t, const std::string&)> handler) {
    if (multiplayerManager) {
        multiplayerManager->setChatMessageHandler(handler);
    }
}

void Game::handleNetworkMessages() {
    // This is handled internally by the MultiplayerManager in its update() method
}

void Game::updateRemotePlayers(const std::map<uint8_t, std::unique_ptr<RemotePlayer>>& remotePlayers) {
    // First, identify remote players already in our objects list
    std::set<uint8_t> existingIds;
    std::vector<Object*> remotePlayerObjects;
    
    for (auto* obj : objects) {
        if (obj->type == ObjectType::ENTITY && obj != player) {
            // Assuming RemotePlayer has its ID stored in the SpriteData ID field
            uint8_t id = (obj->spriteData->ID);
            existingIds.insert(id);
            remotePlayerObjects.push_back(obj);
        }
    }
    
    // Add any new remote players to the objects list
    for (const auto& pair : remotePlayers) {
        const uint8_t id = pair.first;
        const auto& remotePlayer = pair.second;
        
        if (existingIds.find(id) == existingIds.end()) {
            std::cout << "[Game] Adding new remote player to objects list: " << static_cast<int>(id) << std::endl;
            
            // Add this remote player to our game objects
            Object* newPlayerObj = remotePlayer.get();
            objects.push_back(newPlayerObj);
            
            // Update tracking
            existingIds.insert(id);
        } else {
            // std::cout << "[Game] Remote player already in objects list: " << id << std::endl;
        }
    }
    
    // Remove any remote player objects that are no longer in the multiplayer manager
    // for (auto it = objects.begin(); it != objects.end();) {
    //     Object* obj = *it;
        
    //     if (obj->type == ObjectType::ENTITY && obj != player) {
    //         std::string id = std::to_string(obj->spriteData->ID);
            
    //         if (remotePlayers.find(id) == remotePlayers.end()) {
    //             std::cout << "[Game] Removing disconnected remote player from objects list: " << id << std::endl;
    //             it = objects.erase(it);
    //         } else {
    //             ++it;
    //         }
    //     } else {
    //         ++it;
    //     }
    // }
}

// void Game::renderRemotePlayers() {
//     // This function would integrate with your rendering system to draw remote players
//     // Since we don't have direct access to the rendering system in this implementation,
//     // this is a placeholder for where you would render each remote player
    
//     if (multiplayerManager) {
//         const auto& remotePlayers = multiplayerManager->getRemotePlayers();
//         for (const auto& pair : remotePlayers) {
//             // Get the remote player
//             RemotePlayer* remotePlayer = pair.second.get();
            
//             // objects.push_back(remotePlayer);
            
//             // For debugging, just print the remote player positions
//             // std::cout << "Remote player " << remotePlayer->getId() 
//             //           << " at position (" << remotePlayer->getPosition().x 
//             //           << ", " << remotePlayer->getPosition().y << ")" << std::endl;
//         }
//     }
// }