#pragma once

#include "NetworkMessage.h"
#include "network/DeltaState.h"
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
    EmbeddedServer(int port, const std::filesystem::path& basePath);
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
    void addPlayer(const uint16_t playerId);
    
    // Send a player to the client
    void sendPlayerToClient(const uint16_t playerId, Player* player);
    
    // Remove a player from the server
    void removePlayer(const uint16_t playerId);
    
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
    NetworkMessage deserializeMessage(const std::vector<uint8_t>& data, const uint16_t clientId);
    void serializeObject(const std::shared_ptr<Object>& object, std::vector<uint8_t>& data);

    // Game logic methods
    void createInitialGameObjects();
    void updateGameState(float deltaTime);
    void detectAndResolveCollisions();
    void sendGameStateToClients();
    void sendFullGameStateToClient(const uint16_t playerId);
    void sendPartialGameState(const std::vector<std::shared_ptr<Object>>& objects, 
                              size_t startIndex, size_t count, 
                              bool isFirstPacket, bool isLastPacket);
    void sendPartialGameStateToClient(const std::vector<std::shared_ptr<Object>>& objects, 
                                     size_t startIndex, size_t count, 
                                     bool isFirstPacket, bool isLastPacket,
                                     uint16_t playerId);
    
    // Helper methods for game state updates
    std::vector<std::shared_ptr<Object>> collectObjectsToSend(const std::vector<std::shared_ptr<Object>>& allObjects);
    size_t calculateMessageSize(const std::vector<std::shared_ptr<Object>>& objectsToSend);
    void sendSplitGameState(const std::vector<std::shared_ptr<Object>>& objectsToSend, size_t estimatedSize);
    void sendSplitGameStateToClient(const std::vector<std::shared_ptr<Object>>& objectsToSend, 
                                   size_t estimatedSize, uint16_t playerId);
    void sendSingleGameStatePacket(const std::vector<std::shared_ptr<Object>>& objectsToSend);
    void sendSingleGameStatePacketToClient(const std::vector<std::shared_ptr<Object>>& objectsToSend, 
                                          uint16_t playerId);
    void sendMinimalHeartbeat();
    
    // Process player input message
    void processPlayerInput(const uint16_t playerId, const NetworkMessage& message);
    void processPlayerPosition(const uint16_t playerId, const NetworkMessage& message);
    void processEnemyState(const uint16_t playerId, const NetworkMessage& message);
    
    // Send info messages
    void sendEnemyStateToClients(const uint16_t enemyId, bool isDead, int16_t health);

    // Server configuration
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> gameLoopRunning_;
    
    // Network components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> io_thread_;
    std::map<uint16_t, std::shared_ptr<boost::asio::ip::tcp::socket>> clientSockets_;
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
    
    // Delta state tracking
    DeltaStateTracker deltaTracker_;
    
    // Maximum game state packet size (to avoid overflow)
    static constexpr size_t MAX_GAMESTATE_PACKET_SIZE = 1024 * 4; // 4 KB
};


// Message header structure
struct MessageHeader {
    uint32_t size;
};