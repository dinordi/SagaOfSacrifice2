#pragma once

#include <string>
#include <functional>
#include <vector>

// Enum for different types of network messages
enum class MessageType {
    PLAYER_POSITION,   // Position update of a player
    PLAYER_ACTION,     // Special action (jump, attack, etc.)
    PLAYER_INPUT,      // Player input state (new type for server-controlled physics)
    GAME_STATE,        // Complete or partial game state update
    CHAT_MESSAGE,      // Text chat
    CONNECT,           // Player connected
    DISCONNECT,        // Player disconnected
    PING               // Network ping/pong
};

// Base message structure
struct NetworkMessage {
    MessageType type;
    std::string senderId;
    std::vector<uint8_t> data;
};

// Network interface class that will be implemented for client
class NetworkInterface {
public:
    virtual ~NetworkInterface() = default;

    // Connect to a server
    virtual bool connect(const std::string& host, int port) = 0;
    
    // Disconnect from the server
    virtual void disconnect() = 0;
    
    // Send a message to the server
    virtual bool sendMessage(const NetworkMessage& message) = 0;
    
    // Set a callback to handle received messages
    virtual void setMessageHandler(std::function<void(const NetworkMessage&)> handler) = 0;
    
    // Process received messages (should be called regularly in game loop)
    virtual void update() = 0;
    
    // Check if connected to the server
    virtual bool isConnected() const = 0;
};