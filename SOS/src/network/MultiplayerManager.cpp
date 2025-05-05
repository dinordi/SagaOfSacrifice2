#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include <iostream>
#include <cstring>

// External function declaration
extern uint32_t get_ticks();

// RemotePlayer implementation
RemotePlayer::RemotePlayer(const std::string& id) : Object(Vec2(0,0), ObjectType::ENTITY, new SpriteData(std::string("playermap"), 128, 128, 5), id), id_(id) {
    // Note: we're using a default sprite ID of 0 since we can't directly convert strings to sprite IDs
}

void RemotePlayer::update(uint64_t deltaTime) {
    // Simple linear interpolation for position based on velocity
    // position_.x += velocity_.x * (deltaTime / 1000.0f);
    // position_.y += velocity_.y * (deltaTime / 1000.0f);
}

// void RemotePlayer::setPosition(const Vec2& position) {
//     position_ = position;
// }

// void RemotePlayer::setVelocity(const Vec2& velocity) {
//     velocity_ = velocity;
// }

void RemotePlayer::setOrientation(float orientation) {
    orientation_ = orientation;
}

void RemotePlayer::setState(int state) {
    state_ = state;
}

void RemotePlayer::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

// MultiplayerManager implementation
MultiplayerManager::MultiplayerManager()
    : localPlayer_(nullptr), lastUpdateTime_(0) {
    // Create the network interface
    network_ = std::make_unique<AsioNetworkClient>();
}

MultiplayerManager::~MultiplayerManager() {
    shutdown();
}

bool MultiplayerManager::initialize(const std::string& serverAddress, int serverPort, const std::string& playerId) {
    // Store player ID
    playerId_ = playerId;
    
    // Set message handler
    network_->setMessageHandler([this](const NetworkMessage& msg) {
        handleNetworkMessage(msg);
    });
    
    // Connect to server
    bool connected = network_->connect(serverAddress, serverPort);
    
    if (connected) {
        std::cout << "[Client] Connected to multiplayer server at " << serverAddress << ":" << serverPort << std::endl;
        
        // Send a connection message to the server
        NetworkMessage connectMsg;
        connectMsg.type = MessageType::CONNECT;
        connectMsg.senderId = playerId_;
        
        // Add player name to connection data (could include more info)
        std::string playerName = "Player_" + playerId_;
        connectMsg.data.assign(playerName.begin(), playerName.end());
        
        std::cout << "[Client] Sending CONNECT message with player ID: " << playerId_ << std::endl;
        network_->sendMessage(connectMsg);
    } else {
        std::cerr << "[Client] Failed to connect to multiplayer server" << std::endl;
    }
    
    return connected;
}

void MultiplayerManager::shutdown() {
    if (network_ && network_->isConnected()) {
        // Notify server about disconnection
        NetworkMessage disconnectMsg;
        disconnectMsg.type = MessageType::DISCONNECT;
        disconnectMsg.senderId = playerId_;
        
        std::cout << "[Client] Sending DISCONNECT message" << std::endl;
        network_->sendMessage(disconnectMsg);
        
        // Disconnect from server
        network_->disconnect();
        std::cout << "[Client] Disconnected from server" << std::endl;
    }
    
    // Clear remote players
    std::cout << "[Client] Clearing " << remotePlayers_.size() << " remote players" << std::endl;
    remotePlayers_.clear();
}

void MultiplayerManager::update(uint64_t deltaTime) {
    if (!network_ || !network_->isConnected()) {
        std::cout << "No network in MPManager update" << std::endl;
        return;
    }
    
    // Process incoming messages
    network_->update();
    
    static uint64_t lastUpdateTime = 0;
    lastUpdateTime += deltaTime;

    // Debug: output current remote players count
    // if (lastUpdateTime > 2000) { // Every 5 seconds approximately
    //     std::cout << "[Client] Current remote players: " << remotePlayers_.size() << std::endl;
    //     for (const auto& pair : remotePlayers_) {
    //         std::cout << "[Client] Remote player ID: " << pair.first 
    //                   << " at position: (" << pair.second->getposition().x << ", " << pair.second->getposition().y << ")" << std::endl;
    //     }
    //     lastUpdateTime = 0;
    // }
    
    // Update all remote players
    for (auto& pair : remotePlayers_) {
        pair.second->update(deltaTime);
    }
    
    // Send player state periodically
    if (localPlayer_ && lastUpdateTime >= UpdateInterval) {
        sendPlayerState();
        lastUpdateTime_ = deltaTime;
    }
}

void MultiplayerManager::setLocalPlayer(Player* player) {
    localPlayer_ = player;
    std::cout << "[Client] Local player set: " << (player ? "yes" : "no") << std::endl;
}

void MultiplayerManager::sendPlayerState() {
    if (!localPlayer_ || !network_ || !network_->isConnected()) {
        return;
    }
    
    NetworkMessage posMsg;
    posMsg.type = MessageType::PLAYER_POSITION;
    posMsg.senderId = playerId_;
    posMsg.data = serializePlayerState(localPlayer_);
    
    // Only print debug every few seconds to avoid flooding
    // static uint64_t lastLogTime = 0;
    // uint64_t currentTime = get_ticks();
    // if (currentTime - lastLogTime > 5000) { // Log every 5 seconds
    //     std::cout << "[Client] Sending player position: ("
    //               << localPlayer_->getposition().x << ", " << localPlayer_->getposition().y << ")" << std::endl;
    //     lastLogTime = currentTime;
    // }
    
    network_->sendMessage(posMsg);
}

bool MultiplayerManager::isConnected() const {
    return network_ && network_->isConnected();
}

const std::map<std::string, std::unique_ptr<RemotePlayer>>& MultiplayerManager::getRemotePlayers() const {
    return remotePlayers_;
}

void MultiplayerManager::sendChatMessage(const std::string& message) {
    if (!network_ || !network_->isConnected()) {
        return;
    }
    
    NetworkMessage chatMsg;
    chatMsg.type = MessageType::CHAT_MESSAGE;
    chatMsg.senderId = playerId_;
    chatMsg.data.assign(message.begin(), message.end());
    
    network_->sendMessage(chatMsg);
}

void MultiplayerManager::setChatMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    chatHandler_ = handler;
}

void MultiplayerManager::handleNetworkMessage(const NetworkMessage& message) {
    // std::cout << "[Client] Received message type: " << static_cast<int>(message.type) 
    //           << " from sender: " << message.senderId << std::endl;
    
    switch (message.type) {
        case MessageType::PLAYER_POSITION:
            handlePlayerPositionMessage(message);
            break;
        case MessageType::PLAYER_ACTION:
            handlePlayerActionMessage(message);
            break;
        case MessageType::GAME_STATE:
            handleGameStateMessage(message);
            break;
        case MessageType::CHAT_MESSAGE:
            handleChatMessage(message);
            break;
        case MessageType::CONNECT:
            handlePlayerConnectMessage(message);
            break;
        case MessageType::DISCONNECT:
            handlePlayerDisconnectMessage(message);
            break;
        default:
            std::cerr << "[Client] Unknown message type received: " << static_cast<int>(message.type) << std::endl;
            break;
    }
}

void MultiplayerManager::handlePlayerPositionMessage(const NetworkMessage& message) {
    // Ignore our own messages
    if (message.senderId == playerId_) {
        return;
    }
    
    // std::cout << "[Client] Received position update from player: " << message.senderId << std::endl;
    
    // Find or create the remote player
    auto it = remotePlayers_.find(message.senderId);
    if (it == remotePlayers_.end()) {
        // std::cout << "[Client] Creating new remote player for ID: " << message.senderId << std::endl;
        auto newPlayer = std::make_unique<RemotePlayer>(message.senderId);
        it = remotePlayers_.emplace(message.senderId, std::move(newPlayer)).first;
    }
    
    // Debug: check data size
    // std::cout << "[Client] Position message data size: " << message.data.size() << " bytes" << std::endl;
    
    // Update the remote player's state with the received data
    deserializePlayerState(message.data, it->second.get());
}

void MultiplayerManager::handlePlayerActionMessage(const NetworkMessage& message) {
    // Handle special player actions like jumping, attacking, etc.
    if (message.senderId == playerId_) {
        return; // Ignore our own actions
    }
    
    // Find the remote player
    auto it = remotePlayers_.find(message.senderId);
    if (it == remotePlayers_.end()) {
        return; // Player not found
    }
    
    // Process action (simplified example)
    if (message.data.size() >= 1) {
        int actionType = message.data[0];
        // Handle different action types
        switch (actionType) {
            case 1: // Jump
                it->second->setState(1); // Set jumping state
                break;
            case 2: // Attack
                it->second->setState(2); // Set attacking state
                break;
            // Add more action types as needed
        }
    }
}

void MultiplayerManager::handleGameStateMessage(const NetworkMessage& message) {
    // Process game state updates from the server
    // This could include information about game objects, world state, etc.
    
    // Example: Parse game state information (would be more complex in a real game)
    // For now, just log the event
    std::cout << "Game state update received from server" << std::endl;
}

void MultiplayerManager::handleChatMessage(const NetworkMessage& message) {
    if (chatHandler_) {
        // Convert binary data to string
        std::string chatMessage(message.data.begin(), message.data.end());
        chatHandler_(message.senderId, chatMessage);
    }
}

void MultiplayerManager::handlePlayerConnectMessage(const NetworkMessage& message) {
    // A new player has connected
    // If it's not us, add them to our remote players list
    if (message.senderId != playerId_) {
        std::string playerName(message.data.begin(), message.data.end());
        std::cout << "Player connected: " << playerName << std::endl;
        
        // Create a new remote player
        auto newPlayer = std::make_unique<RemotePlayer>(message.senderId);
        remotePlayers_.emplace(message.senderId, std::move(newPlayer));
    }
}

void MultiplayerManager::handlePlayerDisconnectMessage(const NetworkMessage& message) {
    // A player has disconnected
    // Remove them from our remote players list
    if (message.senderId != playerId_) {
        std::cout << "Player disconnected: " << message.senderId << std::endl;
        remotePlayers_.erase(message.senderId);
    }
}

std::vector<uint8_t> MultiplayerManager::serializePlayerState(const Player* player) {
    std::vector<uint8_t> data;
    
    // Get player position and velocity
    const Vec2& pos = player->getposition();
    const Vec2& vel = player->getvelocity();
    
    // Allocate space for the data (2 Vec2s = 4 floats = 16 bytes)
    data.resize(16);
    
    // Copy position (8 bytes)
    std::memcpy(&data[0], &pos.x, sizeof(float));
    std::memcpy(&data[4], &pos.y, sizeof(float));
    
    // Copy velocity (8 bytes)
    std::memcpy(&data[8], &vel.x, sizeof(float));
    std::memcpy(&data[12], &vel.y, sizeof(float));
    
    return data;
}

void MultiplayerManager::deserializePlayerState(const std::vector<uint8_t>& data, RemotePlayer* player) {
    if (data.size() < 16) {
        std::cerr << "Invalid player state data size" << std::endl;
        return;
    }
    
    // Extract position
    Vec2 pos;
    std::memcpy(&pos.x, &data[0], sizeof(float));
    std::memcpy(&pos.y, &data[4], sizeof(float));
    
    // Extract velocity
    Vec2 vel;
    std::memcpy(&vel.x, &data[8], sizeof(float));
    std::memcpy(&vel.y, &data[12], sizeof(float));
    
    // Update player state
    player->getposition() = (pos);
    player->getvelocity() = (vel);
    // std::cout << "Updated remote player " << player->spriteData->ID
    //           << " position: (" << pos.x << ", " << pos.y << ")"
    //           << " velocity: (" << vel.x << ", " << vel.y << ")" << std::endl;
}