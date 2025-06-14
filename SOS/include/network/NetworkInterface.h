#pragma once

#include <string>
#include <functional>
#include <vector>
#include <cstdint>
#include "network/NetworkMessage.h"

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
    
    // Set the client ID for sending messages
    virtual void setClientId(const uint16_t clientId) = 0;
};