#include "../include/GameServer.h"
#include "../include/GameSession.h"
#include <iostream>
#include <boost/bind.hpp>

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

void GameServer::addSession(const std::string& playerId, std::shared_ptr<GameSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_[playerId] = session;
    
    // Notify other players about the new player
    NetworkMessage connectMsg;
    connectMsg.type = MessageType::CONNECT;
    connectMsg.senderId = playerId;
    
    // Include player name or other info
    std::string playerInfo = "Player_" + playerId;
    connectMsg.data.assign(playerInfo.begin(), playerInfo.end());
    
    // Broadcast to all existing clients except the new one
    for (auto& pair : sessions_) {
        if (pair.first != playerId) {
            session->sendMessage(connectMsg);
        }
    }
}

void GameServer::removeSession(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.erase(playerId);
}

void GameServer::broadcastMessage(const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& pair : sessions_) {
        pair.second->sendMessage(message);
    }
}

void GameServer::broadcastMessageExcept(const NetworkMessage& message, const std::string& excludedPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& pair : sessions_) {
        if (pair.first != excludedPlayerId) {
            pair.second->sendMessage(message);
        }
    }
}

bool GameServer::sendMessageToPlayer(const NetworkMessage& message, const std::string& targetPlayerId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(targetPlayerId);
    if (it == sessions_.end()) {
        return false;
    }
    
    it->second->sendMessage(message);
    return true;
}

bool GameServer::isPlayerConnected(const std::string& playerId) const {
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