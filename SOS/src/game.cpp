// SOS/src/game.cpp

#include "game.h"
#include "characters.h"
#include "LocalServerManager.h"
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include <iostream>
#include <set> // Add missing header for std::set

// External function declaration
extern uint32_t get_ticks(); // Declare the get_ticks function

// Default port for local server in single-player mode
const int LOCAL_SERVER_PORT = 8081;

Game::Game(PlayerInput* input, std::string playerID) : running(true), input(input), multiplayerActive(false), usingSinglePlayerServer(false) {
    // Initialize the local server manager
    localServerManager = std::make_unique<LocalServerManager>();
    
    // Initialize network manager (will be connected later)
    multiplayerManager = std::make_unique<MultiplayerManager>();
    
    // Initialize collision manager (used for local prediction only)
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    
    // Initialize game objects
    // Note: in server-authoritative mode, object creation will be managed by the server
    // but we still need to initialize the local player for input and rendering
    player = new Player(Vec2(500,100), new SpriteData(std::string("playermap"), 128, 128, 5), playerID);
    player->setInput(input);
    objects.push_back(player);
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
    // Process local input
    input->readInput();
    
    // If multiplayer is active, update network state
    if (multiplayerActive && multiplayerManager) {
        // Update the network state
        multiplayerManager->update(deltaTime);
        
        // Send player input to server
        multiplayerManager->setPlayerInput(input);
        
        // In the server-authoritative model:
        // 1. We still apply local input immediately for responsive feel
        // 2. But the server will correct our position if needed
        predictLocalPlayerMovement(deltaTime);
        
        // Update remote players based on server data
        updateRemotePlayers(multiplayerManager->getRemotePlayers());
    } else {
        // If multiplayer is not active and we're not using single player server,
        // fall back to local-only update for debugging/development
        player->handleInput(input, deltaTime);
        for(auto object : objects) {
            object->update(deltaTime);
        }
        
        collisionManager->detectCollisions(objects);
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}

bool Game::initializeSinglePlayer(std::filesystem::path server_path) {
    // Initialize multiplayer with local server
    if (usingSinglePlayerServer) {
        return true; // Already initialized
    }
    
    std::cout << "[Game] Setting up single player with local server" << std::endl;
    
    // Start the local server
    if (!localServerManager->startLocalServer(LOCAL_SERVER_PORT, server_path)) {
        std::cerr << "[Game] Failed to start local server" << std::endl;
        return false;
    }
    
    // Give the server a moment to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Connect to the local server
    std::string playerId = player->getObjID();
    if (!initializeMultiplayer("localhost", LOCAL_SERVER_PORT, playerId)) {
        std::cerr << "[Game] Failed to connect to local server" << std::endl;
        localServerManager->stopLocalServer();
        return false;
    }
    
    usingSinglePlayerServer = true;
    std::cout << "[Game] Single player mode with local server initialized" << std::endl;
    
    return true;
}

bool Game::initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId) {
    if (!multiplayerManager) {
        multiplayerManager = std::make_unique<MultiplayerManager>();
    }
    
    bool success = multiplayerManager->initialize(serverAddress, serverPort, playerId);
    
    if (success) {
        multiplayerActive = true;
        multiplayerManager->setLocalPlayer(player);
        multiplayerManager->setPlayerInput(input);
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
    
    if (usingSinglePlayerServer && localServerManager) {
        localServerManager->stopLocalServer();
        usingSinglePlayerServer = false;
        std::cout << "[Game] Local server shut down" << std::endl;
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
            return obj->getObjID() == pair.first;
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
    // This method is no longer needed as the MultiplayerManager handles message processing internally
}

void Game::drawWord(const std::string& word, int x, int y) {
    // This method can be implemented later if needed
}

// New methods for server-authoritative gameplay

void Game::predictLocalPlayerMovement(uint64_t deltaTime) {
    // Apply local input immediately for responsive gameplay
    // This is a simple client-side prediction that will be corrected by the server if needed
    player->handleInput(input, deltaTime);
    player->update(deltaTime);
}

void Game::handleServerStateUpdate(const std::vector<uint8_t>& stateData) {
    // Process the server's authoritative state update
    // This would update all game objects including the player
    // In a full implementation, this would include entity creation/deletion, positions, etc.
    
    // For now, just log that we received a state update
    std::cout << "[Game] Received server state update (" << stateData.size() << " bytes)" << std::endl;
}

void Game::reconcileWithServerState() {
    // Compare the server's authoritative state with our predicted state
    // and correct any discrepancies
    
    // This would happen after receiving a server state update that includes
    // the client's input sequence number
}