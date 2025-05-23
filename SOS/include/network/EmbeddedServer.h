#pragma once

#include "NetworkMessage.h"
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdint>
#include <functional>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/asio.hpp>

#include "interfaces/playerInput.h"
#include "level_manager.h"
// Forward declarations
class Object;
class Player;
class CollisionManager;


/**
 * Embedded server that runs in the same process as the client.
 * This server can run in a separate thread and handle multiplayer
 * functionality without requiring an external server executable.
 */
class EmbeddedServer {
public:
    EmbeddedServer(int port);
    ~EmbeddedServer();
    
    // Start the server
    void start();
    
    // Stop the server
    void stop();
    
    // Run the server (blocking method called in server thread)
    void run();
    
    // Check if the server is running
    bool isRunning() const;
    
    // Add a player to the server
    void addPlayer(const std::string& playerId);
    
    // Remove a player from the server
    void removePlayer(const std::string& playerId);
    
    // Process incoming network message
    void processMessage(const NetworkMessage& message);
    
    // Set callback for when a message needs to be sent to clients
    void setMessageCallback(std::function<void(const NetworkMessage&)> callback);

private:
    // Network handling methods
    void startAccept();
    void handleAccept(const boost::system::error_code& error, std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void handleClientConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void handleRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
                    const boost::system::error_code& error, 
                    size_t bytes_transferred);
    bool sendToClient(std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
                     const NetworkMessage& message);
    // Deserialize message from binary data
    NetworkMessage deserializeMessage(const std::vector<uint8_t>& data, const std::string& clientId);

    // Game logic methods
    void createInitialGameObjects();
    void updateGameState(float deltaTime);
    void detectAndResolveCollisions();
    void sendGameStateToClients();
    
    // Process player input message
    void processPlayerInput(const std::string& playerId, const NetworkMessage& message);
    void processPlayerPosition(const std::string& playerId, const NetworkMessage& message);
    
    // Server configuration
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> gameLoopRunning_;
    
    // Network components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> io_thread_;
    std::map<std::string, std::shared_ptr<boost::asio::ip::tcp::socket>> clientSockets_;
    std::mutex clientSocketsMutex_;
    
    // Game state data
    //std::vector<std::shared_ptr<Object>> gameObjects_;
    //std::map<std::string, std::shared_ptr<Player>> players_;
    std::shared_ptr<LevelManager> levelManager_;
    std::shared_ptr<CollisionManager> collisionManager_;
    
    // Thread management
    std::unique_ptr<std::thread> gameLoopThread_;
    std::mutex gameStateMutex_;
    
    // Callback for sending messages to clients
    std::function<void(const NetworkMessage&)> messageCallback_;

    // buffers for outgoing messages
private:
    std::mutex outgoing_buffers_mutex_;
    std::vector<std::shared_ptr<std::vector<uint8_t>>> outgoing_buffers_;
    
    // Last update time for delta calculation
    std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdateTime_;
};


// Message header structure
struct MessageHeader {
    uint32_t size;
};