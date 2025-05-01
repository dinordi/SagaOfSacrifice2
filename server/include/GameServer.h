#pragma once

#include "NetworkMessage.h"
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <chrono>
#include <thread>

// Forward declarations
class GameSession;
class Object;
class Player;
class RemotePlayer;
class CollisionManager;

// Constant tick rate for game logic updates (Hz)
const int SERVER_TICK_RATE = 60;

// Main server class that manages all client connections and game state
class GameServer {
public:
    GameServer(boost::asio::io_context& ioContext, int port);
    ~GameServer();
    
    // Start accepting client connections
    void start();
    
    // Stop the server
    void stop();
    
    // Add a client session
    void addSession(const std::string playerId, std::shared_ptr<GameSession> session);
    
    // Remove a client session
    void removeSession(const std::string playerId);
    
    // Broadcast a message to all clients
    void broadcastMessage(const NetworkMessage& message);
    
    // Broadcast a message to all clients except the sender
    void broadcastMessageExcept(const NetworkMessage& message, const std::string excludedPlayerId);
    
    // Send a message to a specific client
    bool sendMessageToPlayer(const NetworkMessage& message, const std::string targetPlayerId);
    
    // Check if a player is connected
    bool isPlayerConnected(const std::string playerId) const;

    // Start the game logic thread
    void startGameLoop();

    // Stop the game logic thread
    void stopGameLoop();

    // Update game state (called by the game logic thread)
    void updateGameState(uint64_t deltaTime);

    // Process player input message
    void processPlayerInput(const std::string& playerId, const NetworkMessage& message);

    // Create player object for a client
    void createPlayerObject(const std::string& playerId);

    // Remove player object when client disconnects
    void removePlayerObject(const std::string& playerId);

    // Get all game objects
    const std::vector<std::shared_ptr<Object>>& getGameObjects() const { return gameObjects_; }

private:
    // Start accepting client connections
    void startAccept();
    
    // Handle new client connection
    void handleAccept(std::shared_ptr<GameSession> newSession, const boost::system::error_code& error);

    // Game logic methods
    void createInitialGameObjects();
    void detectAndResolveCollisions();
    void sendGameStateToClients();

    // IO context reference
    boost::asio::io_context& ioContext_;
    
    // Acceptor for incoming connections
    boost::asio::ip::tcp::acceptor acceptor_;
    
    // Map of player ID to session
    std::map<std::string, std::shared_ptr<GameSession>> sessions_;
    
    // Mutex to protect access to the sessions map
    mutable std::mutex sessionsMutex_;
    
    // Server running flag
    bool running_;

    // Game state data
    std::vector<std::shared_ptr<Object>> gameObjects_;
    std::map<std::string, std::shared_ptr<Player>> players_;
    std::shared_ptr<CollisionManager> collisionManager_;

    // Game logic threading
    std::unique_ptr<std::thread> gameLoopThread_;
    bool gameLoopRunning_;
    std::mutex gameStateMutex_;

    // Last update time for delta calculation
    std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdateTime_;
};