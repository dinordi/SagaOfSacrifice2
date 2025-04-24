#pragma once

#include "NetworkMessage.h"
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <string>
#include <mutex>

// Forward declaration
class GameSession;

// Main server class that manages all client connections
class GameServer {
public:
    GameServer(boost::asio::io_context& ioContext, int port);
    ~GameServer();
    
    // Start accepting client connections
    void start();
    
    // Stop the server
    void stop();
    
    // Add a client session
    void addSession(const std::string& playerId, std::shared_ptr<GameSession> session);
    
    // Remove a client session
    void removeSession(const std::string& playerId);
    
    // Broadcast a message to all clients
    void broadcastMessage(const NetworkMessage& message);
    
    // Broadcast a message to all clients except the sender
    void broadcastMessageExcept(const NetworkMessage& message, const std::string& excludedPlayerId);
    
    // Send a message to a specific client
    bool sendMessageToPlayer(const NetworkMessage& message, const std::string& targetPlayerId);
    
    // Check if a player is connected
    bool isPlayerConnected(const std::string& playerId) const;

private:
    // Start accepting client connections
    void startAccept();
    
    // Handle new client connection
    void handleAccept(std::shared_ptr<GameSession> newSession, const boost::system::error_code& error);
    
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
};