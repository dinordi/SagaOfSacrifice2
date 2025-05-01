#include "../include/GameServer.h"
#include "../include/GameSession.h"
#include <iostream>
#include <boost/bind.hpp>
#include <string>
#include <thread>
#include <chrono>

#include "object.h"

// Include object headers needed for game logic 
// Using forward includes to avoid dependency issues
class Object;
class Player;
class Platform;
class RemotePlayer;
class CollisionManager;

using namespace boost::placeholders;

GameServer::GameServer(boost::asio::io_context& ioContext, int port)
    : ioContext_(ioContext),
      acceptor_(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      running_(false),
      gameLoopRunning_(false) {
    
    std::cout << "Game server created on port " << port << std::endl;
    
    // Create collision manager
    collisionManager_ = std::make_shared<CollisionManager>();
}

GameServer::~GameServer() {
    stop();
}

void GameServer::start() {
    if (running_) {
        std::cerr << "Server is already running" << std::endl;
        return;
    }
    
    running_ = true;
    std::cout << "Game server started" << std::endl;
    
    // Initialize game world with static objects
    createInitialGameObjects();
    
    // Start game logic thread
    startGameLoop();
    
    // Start accepting connections
    startAccept();
}

void GameServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Stop game logic thread
    stopGameLoop();
    
    // Close the acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    // Close all sessions
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& pair : sessions_) {
        // Let the session handle its own cleanup
    }
    
    sessions_.clear();
    std::cout << "Game server stopped" << std::endl;
}

void GameServer::addSession(const std::string playerId, std::shared_ptr<GameSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_[playerId] = session;
    
    std::cout << "[Server] Player added: " << playerId << ", current player count: " << sessions_.size() << std::endl;
    std::cout << "[Server] Current players: ";
    for (const auto& pair : sessions_) {
        std::cout << pair.first << " ";
    }
    std::cout << std::endl;
    
    // Notify the new player about all existing players
    for (auto& pair : sessions_) {
        if (pair.first != playerId) {
            std::cout << "[Server] Sending existing player " << (pair.first) << " info to new player " << (playerId) << std::endl;
            
            NetworkMessage existingPlayerMsg;
            existingPlayerMsg.type = MessageType::CONNECT;
            existingPlayerMsg.senderId = pair.first;
            
            // Include player name or other info
            std::string playerInfo = "Player_" + pair.first;
            existingPlayerMsg.data.assign(playerInfo.begin(), playerInfo.end());
            
            // Send to the new player
            session->sendMessage(existingPlayerMsg);
        }
    }
    
    // Now notify all existing players about the new player
    NetworkMessage connectMsg;
    connectMsg.type = MessageType::CONNECT;
    connectMsg.senderId = playerId;
    
    // Include player name or other info
    std::string playerInfo = "Player_" + playerId;
    connectMsg.data.assign(playerInfo.begin(), playerInfo.end());
    
    // Broadcast to all existing clients except the new one
    std::cout << "[Server] Broadcasting new player " << (playerId) << " to all other players" << std::endl;
    broadcastMessageExcept(connectMsg, playerId);
    std::cout << "[Server] Broadcasted connect message to all players except " << (playerId) << std::endl;
}

void GameServer::removeSession(const std::string playerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(playerId);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        std::cout << "[Server] Player removed: " << (playerId) << ", remaining player count: " << sessions_.size() << std::endl;
    }
}

void GameServer::broadcastMessage(const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    // std::cout << "[Server] Broadcasting message type " << static_cast<int>(message.type)
    //           << " from " << message.senderId << " to all " << sessions_.size() << " players" << std::endl;
    
    for (auto& pair : sessions_) {
        pair.second->sendMessage(message);
    }
}

void GameServer::broadcastMessageExcept(const NetworkMessage& message, const std::string excludedPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    int sentCount = 0;
    // std::cout << "[Server Broadcast] Acquiring lock for broadcast (excluding " << (excludedPlayerId) << ")" << std::endl; // Added log

    for (auto& pair : sessions_) {
        if (pair.first != excludedPlayerId) {
            try {
                if (pair.second) { // Check if session pointer is valid
                    pair.second->sendMessage(message);
                    sentCount++;
                } else {
                    std::cerr << "[Server Broadcast] Error: Session pointer is null for player ID (in map): " << (pair.first) << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Server Broadcast] Exception while sending to player " << (pair.first) << ": " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[Server Broadcast] Unknown exception while sending to player " << (pair.first) << std::endl;
            }
        }
    }

    // std::cout << "[Server Broadcast] Message type " << static_cast<int>(message.type)
    //           << " from " << message.senderId << " sent to " << sentCount << " players (excluding " << excludedPlayerId << ")" << std::endl;
}

bool GameServer::sendMessageToPlayer(const NetworkMessage& message, const std::string targetPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(targetPlayerId);
    if (it == sessions_.end()) {
        return false;
    }
    
    it->second->sendMessage(message);
    return true;
}

bool GameServer::isPlayerConnected(const std::string playerId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.find(playerId) != sessions_.end();
}

void GameServer::startAccept() {
    if (!running_) return;
    
    // Create a new session
    auto newSession = std::make_shared<GameSession>(
        boost::asio::ip::tcp::socket(ioContext_),
        *this
    );
    
    // Accept a new connection
    acceptor_.async_accept(
        newSession->socket(),
        boost::bind(&GameServer::handleAccept, this, newSession, boost::asio::placeholders::error)
    );
}

void GameServer::handleAccept(std::shared_ptr<GameSession> newSession, const boost::system::error_code& error) {
    if (!error) {
        // Start the new session
        std::cout << "New client connected from: " 
            << newSession->socket().remote_endpoint().address().to_string() 
            << ":" << newSession->socket().remote_endpoint().port() << std::endl;
        
        // Start the session (it will add itself to the sessions map when it gets the CONNECT message)
        newSession->start();
    } else {
        std::cerr << "Accept error: " << error.message() << std::endl;
    }
    
    // Continue accepting connections
    startAccept();
}

// New methods for game logic

void GameServer::startGameLoop() {
    if (gameLoopRunning_) return;
    
    gameLoopRunning_ = true;
    lastUpdateTime_ = std::chrono::high_resolution_clock::now();
    
    // Start the game loop in a separate thread
    gameLoopThread_ = std::make_unique<std::thread>([this]() {
        std::cout << "[Server] Game loop started" << std::endl;
        
        const auto tickDuration = std::chrono::milliseconds(1000 / SERVER_TICK_RATE);
        
        while (gameLoopRunning_) {
            auto now = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime_).count();
            lastUpdateTime_ = now;
            
            // Update game state
            updateGameState(deltaTime);
            
            // Sleep until next tick
            std::this_thread::sleep_for(tickDuration);
        }
        
        std::cout << "[Server] Game loop stopped" << std::endl;
    });
}

void GameServer::stopGameLoop() {
    if (!gameLoopRunning_) return;
    
    gameLoopRunning_ = false;
    
    // Wait for the game loop thread to finish
    if (gameLoopThread_ && gameLoopThread_->joinable()) {
        gameLoopThread_->join();
    }
}

void GameServer::updateGameState(uint64_t deltaTime) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Update all game objects
    for (auto& obj : gameObjects_) {
        obj->update(deltaTime);
    }
    
    // Detect and resolve collisions
    detectAndResolveCollisions();
    
    // Send game state to clients
    sendGameStateToClients();
}

void GameServer::createInitialGameObjects() {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Clear existing objects
    gameObjects_.clear();
    players_.clear();
    
    // In a real implementation, we would:
    // 1. Create platforms
    // 2. Create static objects
    // 3. Load level data
    
    // For now, just log that we're initializing
    std::cout << "[Server] Creating initial game objects" << std::endl;
    
    // Will implement platform creation here once we include the Platform class
    // This would be similar to:
    // Platform* floor = new Platform(500, 850, new SpriteData(std::string("tiles"), 128, 128, 4), generateRandomPlayerId());
    // gameObjects_.push_back(std::shared_ptr<Object>(floor));
}

void GameServer::detectAndResolveCollisions() {
    // Using the collision manager to detect and resolve collisions
    // Will implement once we have full access to the CollisionManager class
    
    // In a real implementation, we would:
    // auto collisions = collisionManager_->detectCollisions(gameObjects_);
    // collisions will automatically resolve during detection in the current implementation
}

void GameServer::sendGameStateToClients() {
    // Create a game state update message
    NetworkMessage stateMsg;
    stateMsg.type = MessageType::GAME_STATE;
    stateMsg.senderId = "SERVER";
    
    // Serialize the game state (positions, velocities, etc.)
    // For each object in gameObjects_, add its data to the message
    
    // For now, we'll just send a simple ping message since we don't have the full
    // serialization structure implemented
    
    // Broadcast the game state to all clients
    broadcastMessage(stateMsg);
}

void GameServer::processPlayerInput(const std::string& playerId, const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Find the player's object
    auto playerIt = players_.find(playerId);
    if (playerIt == players_.end()) {
        std::cerr << "[Server] Received input for unknown player: " << playerId << std::endl;
        return;
    }
    
    auto player = playerIt->second;
    
    // Process the input data
    // In a real implementation, we would:
    // 1. Deserialize the input data (keystate, mouse position, etc.)
    // 2. Update the player's state based on the input
    // 3. Let physics take over in the next update
    
    // For now, just log that we received input
    std::cout << "[Server] Processing input for player: " << playerId << std::endl;
    
    // Echo input message to all clients (temporary, will replace with proper game state)
    broadcastMessageExcept(message, playerId);
}

void GameServer::createPlayerObject(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Remove any existing player with this ID
    removePlayerObject(playerId);
    
    // In a real implementation, we would:
    // 1. Create a new Player object
    // 2. Add it to the game objects list
    // 3. Add it to the players map
    
    std::cout << "[Server] Created player object for: " << playerId << std::endl;
    
    // Will implement once we have access to the Player class
    // This would be similar to:
    // auto player = std::make_shared<Player>(Vec2(500, 100), new SpriteData(std::string("playermap"), 128, 128, 5), playerId);
    // gameObjects_.push_back(player);
    // players_[playerId] = player;
}

void GameServer::removePlayerObject(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Find and remove from players map
    auto playerIt = players_.find(playerId);
    if (playerIt != players_.end()) {
        players_.erase(playerIt);
    }
    
    // Find and remove from game objects vector
    auto objIt = std::find_if(gameObjects_.begin(), gameObjects_.end(), 
        [&playerId](const std::shared_ptr<Object>& obj) {
            // In a real implementation, we would check if this is the player's object
            // For now, just log that we're removing the player
            return false; // Will implement proper comparison once we have access to the Object class
        });
    
    if (objIt != gameObjects_.end()) {
        gameObjects_.erase(objIt);
    }
    
    std::cout << "[Server] Removed player object for: " << playerId << std::endl;
}