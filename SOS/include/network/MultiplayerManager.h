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
    void update(uint64_t deltaTime);
    
    // Set the local player
    void setLocalPlayer(Player* player);
    
    // Send local player state to the server
    void sendPlayerState();
    
    // Check if connected to server
    bool isConnected() const;
    
    // Get all remote players
    const std::map<std::string, std::unique_ptr<RemotePlayer>>& getRemotePlayers() const;
    
    // Send a chat message
    void sendChatMessage(const std::string& message);
    
    // Set the chat message handler
    void setChatMessageHandler(std::function<void(const std::string& senderId, const std::string& message)> handler);

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
    
    // Serialize/deserialize player state
    std::vector<uint8_t> serializePlayerState(const Player* player);
    void deserializePlayerState(const std::vector<uint8_t>& data, RemotePlayer* player);
    
    // Network interface
    std::unique_ptr<NetworkInterface> network_;
    
    // Local player reference (not owned)
    Player* localPlayer_;
    
    // Remote players
    std::map<std::string, std::unique_ptr<RemotePlayer>> remotePlayers_;
    
    // Player ID
    std::string playerId_;
    
    // Chat message handler
    std::function<void(const std::string& senderId, const std::string& message)> chatHandler_;
    
    // Last time we sent a player update
    uint64_t lastUpdateTime_;
    
    // How often to send updates (in ms)
    static constexpr uint64_t UpdateInterval = 50;  // 20 updates per second
};

// RemotePlayer class to represent other players in the game
class RemotePlayer : public Object
{
public:
    RemotePlayer(const std::string& id);
    
    void update(uint64_t deltaTime) override;
    
    // Setters
    // void setPosition(const Vec2& position);
    // void setVelocity(const Vec2& velocity);
    void setOrientation(float orientation);
    void setState(int state);
    
    void accept(CollisionVisitor& visitor) override;

    // Getters
    // const std::string& getId() const { return id_; }
    // const Vec2& getPosition() const { return position_; }
    // const Vec2& getVelocity() const { return velocity_; }
    float getOrientation() const { return orientation_; }
    int getState() const { return state_; }
    
private:
    // std::string id_;
    // Vec2 position_;
    // Vec2 velocity_;
    float orientation_;
    int state_;
    const std::string id_;
};