#pragma once

#include "NetworkInterface.h"
#include "object.h" // Fixed case sensitivity issue
#include "objects/player.h"
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>

// Forward declaration
class PlayerInput;

// Handles all multiplayer functionality for the client
class MultiplayerManager {
public:
    MultiplayerManager();
    ~MultiplayerManager();

    // Initialize the multiplayer system
    bool initialize(const std::string& serverAddress, int serverPort, const std::string& playerId);
    
    // Shutdown multiplayer system
    void shutdown();
    
    // Update network state - should be called once per frame
    void update(float deltaTime);
    
    // Set the local player
    void setLocalPlayer(Player* player);
    
    // Set the player input handler
    void setPlayerInput(PlayerInput* input);
    
    // Send player input state to server
    void sendPlayerInput();
    
    // Send player position (kept for backwards compatibility, less important now)
    void sendPlayerState();
    
    // Send player action
    void sendPlayerAction(int actionType);
    
    // Send enemy state update to server (e.g., when enemy dies)
    void sendEnemyStateUpdate(const std::string& enemyId, bool isDead, int currentHealth);
    void handleEnemyStateMessage(const NetworkMessage& message);

    // Check if connected to server
    bool isConnected() const;
    
    // Get all remote players
    const std::map<std::string, std::shared_ptr<Player>>& getRemotePlayers() const;
    
    // Send a chat message
    void sendChatMessage(const std::string& message);
    
    // Set the chat message handler
    void setChatMessageHandler(std::function<void(const std::string& senderId, const std::string& message)> handler);
    
    // Process game state update from server
    void processGameState(const std::vector<uint8_t>& gameStateData);
    
    // Process delta game state update (only changed objects)
    void processGameStateDelta(const std::vector<uint8_t>& gameStateData);
    
    // Process part of a multi-packet game state update
    void processGameStatePart(const std::vector<uint8_t>& gameStateData);

private:
    // Handle network messages received from the server
    void handleNetworkMessage(const NetworkMessage& message);
    
    // Helper methods for specific message types
    void handlePlayerPositionMessage(const NetworkMessage& message);
    void handlePlayerActionMessage(const NetworkMessage& message);
    void handleGameStateMessage(const NetworkMessage& message);
    void handleChatMessage(const NetworkMessage& message);
    void handlePlayerConnectMessage(const NetworkMessage& message);
    void handlePlayerDisconnectMessage(const NetworkMessage& message);
    void handlePlayerAssignMessage(const NetworkMessage& message);

    std::shared_ptr<Object> updateEntityPosition(const std::string& playerId, const Vec2& position, const Vec2& velocity);
    
    // Serialize/deserialize player state
    std::vector<uint8_t> serializePlayerState(const Player* player);
    void deserializePlayerState(const std::vector<uint8_t>& data, Player* player);

    // Deserialize object from game state data
    std::shared_ptr<Object> deserializeObject(const std::vector<uint8_t>& data, size_t& pos);
    
    // Serialize player input
    std::vector<uint8_t> serializePlayerInput(const PlayerInput* input);
    
    // Network interface
    std::unique_ptr<NetworkInterface> network_;
    
    // Local player reference (not owned)
    Player* localPlayer_;
    
    // Player input reference (not owned)
    PlayerInput* playerInput_;
    
    // Remote players - now using Player class instead of RemotePlayer
    std::map<std::string, std::shared_ptr<Player>> remotePlayers_;
    
    // Player ID
    std::string playerId_;
    
    // Chat message handler
    std::function<void(const std::string& senderId, const std::string& message)> chatHandler_;
    
    // Last time we sent a player update
    uint64_t lastUpdateTime_;
    
    // Client-side prediction state
    float lastSentInputTime_;
    uint32_t inputSequenceNumber_;

    //Base path for atlas
    std::filesystem::path atlasBasePath_;

    // Multi-part game state handling
    struct PartialGameState {
        uint16_t totalObjectCount;
        bool complete;
        std::vector<std::vector<uint8_t>> parts;
        std::vector<uint16_t> packetIndices;
        std::chrono::steady_clock::time_point lastUpdateTime;
    };
    std::unique_ptr<PartialGameState> partialGameState_;
};