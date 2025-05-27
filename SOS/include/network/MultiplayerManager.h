#pragma once

#include "NetworkInterface.h"
#include "object.h" // Fixed case sensitivity issue
#include "objects/player.h"
#include <memory>
#include <string>
#include <map>
#include <vector>

// Forward declaration
class RemotePlayer;
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
    
    // Check if connected to server
    bool isConnected() const;
    
    // Get all remote players
    const std::map<std::string, std::unique_ptr<RemotePlayer>>& getRemotePlayers() const;
    
    // Send a chat message
    void sendChatMessage(const std::string& message);
    
    // Set the chat message handler
    void setChatMessageHandler(std::function<void(const std::string& senderId, const std::string& message)> handler);
    
    // Process game state update from server
    void processGameState(const std::vector<uint8_t>& gameStateData);

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

    std::shared_ptr<Object> updateEntityPosition(const std::string& playerId, const Vec2& position, const Vec2& velocity);
    
    // Serialize/deserialize player state
    std::vector<uint8_t> serializePlayerState(const Player* player);
    void deserializePlayerState(const std::vector<uint8_t>& data, RemotePlayer* player);
    
    // Serialize player input
    std::vector<uint8_t> serializePlayerInput(const PlayerInput* input);
    
    // Network interface
    std::unique_ptr<NetworkInterface> network_;
    
    // Local player reference (not owned)
    Player* localPlayer_;
    
    // Player input reference (not owned)
    PlayerInput* playerInput_;
    
    // Remote players
    std::map<std::string, std::unique_ptr<RemotePlayer>> remotePlayers_;
    
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
};

// RemotePlayer class to represent other players in the game
class RemotePlayer : public Object
{
public:
    RemotePlayer(const std::string& id);
    
    void update(float deltaTime) override;
    
    // Setters
    void setOrientation(float orientation);
    void setState(int state);
    void setTargetPosition(const Vec2& position);
    void setTargetVelocity(const Vec2& velocity);
    void resetInterpolation();
    
    void accept(CollisionVisitor& visitor) override;

    // Getters
    float getOrientation() const { return orientation_; }
    int getState() const { return state_; }
    const Vec2& getTargetPosition() const { return targetPosition_; }
    const Vec2& getTargetVelocity() const { return targetVelocity_; }
    float getInterpolationTime() const { return interpolationTime_; }
    
private:
    float orientation_;
    int state_;
    const std::string id_;
    
    // Client-side interpolation variables
    Vec2 targetPosition_;
    Vec2 targetVelocity_;
    float interpolationTime_;
};