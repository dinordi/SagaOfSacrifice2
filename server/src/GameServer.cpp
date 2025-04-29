#include "../include/GameServer.h"
#include "../include/GameSession.h"
#include <iostream>
#include <boost/bind.hpp>
#include <string>

GameServer::GameServer(boost::asio::io_context& ioContext, int port)
    : ioContext_(ioContext),
      acceptor_(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      running_(false) {
    std::cout << "Game server created on port " << port << std::endl;
}

GameServer::~GameServer() {
    stop();
}

void GameServer::start() {
    if (running_) {
        std::cerr << "Server is already running" << std::endl;
        return;
    }
    
    running_ = true;
    std::cout << "Game server started" << std::endl;
    
    startAccept();
}

void GameServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Close the acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    // Close all sessions
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& pair : sessions_) {
        // Let the session handle its own cleanup
    }
    
    sessions_.clear();
    std::cout << "Game server stopped" << std::endl;
}

void GameServer::addSession(const std::string playerId, std::shared_ptr<GameSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_[playerId] = session;
    
    std::cout << "[Server] Player added: " << playerId << ", current player count: " << sessions_.size() << std::endl;
    std::cout << "[Server] Current players: ";
    for (const auto& pair : sessions_) {
        std::cout << pair.first << " ";
    }
    std::cout << std::endl;
    
    // Notify the new player about all existing players
    for (auto& pair : sessions_) {
        if (pair.first != playerId) {
            std::cout << "[Server] Sending existing player " << (pair.first) << " info to new player " << (playerId) << std::endl;
            
            NetworkMessage existingPlayerMsg;
            existingPlayerMsg.type = MessageType::CONNECT;
            existingPlayerMsg.senderId = pair.first;
            
            // Include player name or other info
            std::string playerInfo = "Player_" + pair.first;
            existingPlayerMsg.data.assign(playerInfo.begin(), playerInfo.end());
            
            // Send to the new player
            session->sendMessage(existingPlayerMsg);
        }
    }
    
    // Now notify all existing players about the new player
    NetworkMessage connectMsg;
    connectMsg.type = MessageType::CONNECT;
    connectMsg.senderId = playerId;
    
    // Include player name or other info
    std::string playerInfo = "Player_" + playerId;
    connectMsg.data.assign(playerInfo.begin(), playerInfo.end());
    
    // Broadcast to all existing clients except the new one
    std::cout << "[Server] Broadcasting new player " << (playerId) << " to all other players" << std::endl;
    // broadcastMessageExcept(connectMsg, playerId);
    std::cout << "[Server] Broadcasted connect message to all players except " << (playerId) << std::endl;
}

void GameServer::removeSession(const std::string playerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(playerId);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        std::cout << "[Server] Player removed: " << (playerId) << ", remaining player count: " << sessions_.size() << std::endl;
    }
}

void GameServer::broadcastMessage(const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    // std::cout << "[Server] Broadcasting message type " << static_cast<int>(message.type)
    //           << " from " << message.senderId << " to all " << sessions_.size() << " players" << std::endl;
    
    for (auto& pair : sessions_) {
        pair.second->sendMessage(message);
    }
}

void GameServer::broadcastMessageExcept(const NetworkMessage& message, const std::string excludedPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    int sentCount = 0;
    // std::cout << "[Server Broadcast] Acquiring lock for broadcast (excluding " << (excludedPlayerId) << ")" << std::endl; // Added log

    for (auto& pair : sessions_) {
        if (pair.first != excludedPlayerId) {
            // std::cout << "[Server Broadcast] Attempting to send type " << static_cast<int>(message.type)
            //           << " to player: " << (pair.first) << std::endl; // Added log
            try {
                if (pair.second) { // Check if session pointer is valid
                    pair.second->sendMessage(message);
                    // std::cout << "[Server Broadcast] Successfully queued message for player: " << (pair.first) << std::endl; // Added log
                    sentCount++;
                } else {
                    std::cerr << "[Server Broadcast] Error: Session pointer is null for player ID (in map): " << (pair.first) << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Server Broadcast] Exception while sending to player " << (pair.first) << ": " << e.what() << std::endl;
                // Decide how to handle this - maybe remove the session?
            } catch (...) {
                std::cerr << "[Server Broadcast] Unknown exception while sending to player " << (pair.first) << std::endl;
                // Decide how to handle this
            }
        }
    }

    // std::cout << "[Server Broadcast] Releasing lock. Message type " << static_cast<int>(message.type)
    //           << " from " << message.senderId << " sent to " << sentCount << " players (excluding " << excludedPlayerId << ")" << std::endl; // Modified log
}

bool GameServer::sendMessageToPlayer(const NetworkMessage& message, const std::string targetPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(targetPlayerId);
    if (it == sessions_.end()) {
        return false;
    }
    
    it->second->sendMessage(message);
    // std::cout << "Message sent to player " << (targetPlayerId) << std::endl;
    return true;
}

bool GameServer::isPlayerConnected(const std::string playerId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.find(playerId) != sessions_.end();
}

void GameServer::startAccept() {
    if (!running_) return;
    
    // Create a new session
    auto newSession = std::make_shared<GameSession>(
        boost::asio::ip::tcp::socket(ioContext_),
        *this
    );
    
    // Accept a new connection
    acceptor_.async_accept(
        newSession->socket(),
        boost::bind(&GameServer::handleAccept, this, newSession, boost::asio::placeholders::error)
    );
}

void GameServer::handleAccept(std::shared_ptr<GameSession> newSession, const boost::system::error_code& error) {
    if (!error) {
        // Start the new session
        std::cout << "New client connected from: " 
            << newSession->socket().remote_endpoint().address().to_string() 
            << ":" << newSession->socket().remote_endpoint().port() << std::endl;
        
        // Start the session (it will add itself to the sessions map when it gets the CONNECT message)
        newSession->start();
    } else {
        std::cerr << "Accept error: " << error.message() << std::endl;
    }
    
    // Continue accepting connections
    startAccept();
}