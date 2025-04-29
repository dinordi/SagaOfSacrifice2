#pragma once

#include "NetworkMessage.h"
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <functional>
#include <memory>
#include <queue>

// Forward declaration
class GameServer;

// Class to handle a client connection session
class GameSession : public std::enable_shared_from_this<GameSession> {
public:
    GameSession(boost::asio::ip::tcp::socket socket, GameServer& server);
    ~GameSession();
    
    // Start the session
    void start();
    
    // Send a message to the client
    void sendMessage(const NetworkMessage& message);
    
    // Get the player ID
    const std::string getPlayerId() const { return playerId_; }
    
    // Check if connected
    bool isConnected() const { return connected_; }
    
    // Public accessor for the socket
    boost::asio::ip::tcp::socket& socket() { return socket_; }

private:
    // Read message from client
    void startRead();
    
    // Handle a message from client
    void handleMessage(const NetworkMessage& message);
    
    // Process different message types
    void processConnect(const NetworkMessage& message);
    void processDisconnect(const NetworkMessage& message);
    void processPosition(const NetworkMessage& message);
    void processChat(const NetworkMessage& message);
    
    // Helper functions
    void asyncWrite(const std::vector<uint8_t>& data);
    void handleWrite(const boost::system::error_code& error, size_t bytesTransferred);
    
    // Serialize/deserialize network messages
    std::vector<uint8_t> serializeMessage(const NetworkMessage& message);
    NetworkMessage deserializeMessage(const std::vector<uint8_t>& data);
    
    // Socket for this connection
    boost::asio::ip::tcp::socket socket_;
    
    // Reference to server
    GameServer& server_;
    
    // Read buffer
    enum { max_buffer_size = 1024 };
    std::vector<uint8_t> readBuffer_;
    
    // Player ID for this session
    std::string playerId_;
    
    // Connection status
    bool connected_;
    
    // Message header structure
    struct MessageHeader {
        uint32_t size;
    };
};