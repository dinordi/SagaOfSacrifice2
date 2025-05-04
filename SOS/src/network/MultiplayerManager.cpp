#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include "interfaces/playerInput.h"  // Add player input header
#include <iostream>
#include <cstring>

// External function declaration
extern uint32_t get_ticks();

// RemotePlayer implementation
RemotePlayer::RemotePlayer(const std::string& id) 
    : Object(Vec2(0,0), ObjectType::ENTITY, new SpriteData(std::string("playermap"), 128, 128, 5), id), 
      id_(id),
      interpolationTime_(0.0f),
      targetPosition_(Vec2(0, 0)),
      targetVelocity_(Vec2(0, 0)) {
    // Note: we're using a default sprite ID of 0 since we can't directly convert strings to sprite IDs
}

void RemotePlayer::update(uint64_t deltaTime) {
    // Smooth interpolation between current position and target position
    float dt = deltaTime / 1000.0f;  // Convert to seconds
    
    // Update interpolation timer
    interpolationTime_ += dt;
    float t = std::min(interpolationTime_ / InterpolationPeriod, 1.0f);

    Vec2 velocity_ = getvelocity();
    Vec2 position_ = getposition();
    
    // Only interpolate if we have a different target position
    if ((targetPosition_.x != position_.x || targetPosition_.y != position_.y) && t < 1.0f) {
        // Linear interpolation
        position_.x = position_.x + (targetPosition_.x - position_.x) * t;
        position_.y = position_.y + (targetPosition_.y - position_.y) * t;
        
        // Update velocity based on target velocity
        velocity_.x = velocity_.x + (targetVelocity_.x - velocity_.x) * t;
        velocity_.y = velocity_.y + (targetVelocity_.y - velocity_.y) * t;
    } else {
        // We've reached the target or never started interpolating, apply velocity directly
        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
    }

    // Update the object's position
    setposition(position_);
    setvelocity(velocity_);
}

void RemotePlayer::setOrientation(float orientation) {
    orientation_ = orientation;
}

void RemotePlayer::setState(int state) {
    state_ = state;
}

void RemotePlayer::setTargetPosition(const Vec2& position) {
    targetPosition_ = position;
}

void RemotePlayer::setTargetVelocity(const Vec2& velocity) {
    targetVelocity_ = velocity;
}

void RemotePlayer::resetInterpolation() {
    interpolationTime_ = 0.0f;
}

void RemotePlayer::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

// MultiplayerManager implementation
MultiplayerManager::MultiplayerManager()
    : localPlayer_(nullptr), 
      playerInput_(nullptr),
      lastUpdateTime_(0),
      lastSentInputTime_(0.0f),
      inputSequenceNumber_(0) {
    // Create the network interface
    network_ = std::make_unique<AsioNetworkClient>();
}

MultiplayerManager::~MultiplayerManager() {
    // Don't automatically shutdown in the destructor as it can cause premature DISCONNECT messages
    // The owner of this object should call shutdown() explicitly when ready
    std::cout << "[Client] MultiplayerManager destructor called, network will be cleaned up but no DISCONNECT sent" << std::endl;
    
    // Just clean up resources without sending DISCONNECT
    if (network_ && network_->isConnected()) {
        network_->disconnect();
    }
    
    remotePlayers_.clear();
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
    std::cout << "[Client DEBUG] MultiplayerManager::shutdown() called" << std::endl;
    std::cout << "[Client DEBUG] Stack trace/debugging info would be useful here" << std::endl;
    
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

    // Update all remote players
    for (auto& pair : remotePlayers_) {
        pair.second->update(deltaTime);
    }
    
    // Send player input periodically (primary control method now)
    if (playerInput_ && localPlayer_ && lastUpdateTime >= UpdateInterval) {
        sendPlayerInput();
        lastUpdateTime_ = deltaTime;
    }
}

void MultiplayerManager::setLocalPlayer(Player* player) {
    localPlayer_ = player;
    std::cout << "[Client] Local player set: " << (player ? "yes" : "no") << std::endl;
}

void MultiplayerManager::setPlayerInput(PlayerInput* input) {
    playerInput_ = input;
    std::cout << "[Client] Player input set: " << (input ? "yes" : "no") << std::endl;
}

void MultiplayerManager::sendPlayerState() {
    // This method is retained for backwards compatibility
    // In the new model, the server is authoritative, so we primarily send input
    // But we can still send position updates occasionally as a sync check
    
    if (!localPlayer_ || !network_ || !network_->isConnected()) {
        return;
    }
    
    NetworkMessage posMsg;
    posMsg.type = MessageType::PLAYER_POSITION;
    posMsg.senderId = playerId_;
    posMsg.data = serializePlayerState(localPlayer_);
    
    network_->sendMessage(posMsg);
}

void MultiplayerManager::sendPlayerInput() {
    if (!playerInput_ || !network_ || !network_->isConnected()) {
        return;
    }
    
    // Serialize player input state
    NetworkMessage inputMsg;
    inputMsg.type = MessageType::PLAYER_INPUT;
    inputMsg.senderId = playerId_;
    inputMsg.data = serializePlayerInput(playerInput_);
    
    // Send to server
    network_->sendMessage(inputMsg);
    
    // Update sequence number for client-side prediction
    inputSequenceNumber_++;
    lastSentInputTime_ = get_ticks() / 1000.0f;  // Convert to seconds
}

void MultiplayerManager::sendPlayerAction(int actionType) {
    if (!network_ || !network_->isConnected()) {
        return;
    }
    
    // Send special player actions like jumping, attacking, etc.
    NetworkMessage actionMsg;
    actionMsg.type = MessageType::PLAYER_ACTION;
    actionMsg.senderId = playerId_;
    actionMsg.data = {static_cast<uint8_t>(actionType)};
    
    network_->sendMessage(actionMsg);
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
    
    // Find or create the remote player
    auto it = remotePlayers_.find(message.senderId);
    if (it == remotePlayers_.end()) {
        auto newPlayer = std::make_unique<RemotePlayer>(message.senderId);
        it = remotePlayers_.emplace(message.senderId, std::move(newPlayer)).first;
    }
    
    // Get pointer to remote player
    RemotePlayer* remotePlayer = it->second.get();
    
    // Deserialize the position data
    deserializePlayerState(message.data, remotePlayer);
    
    // Reset interpolation timer whenever we get a new position update
    remotePlayer->resetInterpolation();
    
    // Store the target position and velocity for interpolation
    remotePlayer->setTargetPosition(remotePlayer->getposition());
    remotePlayer->setTargetVelocity(remotePlayer->getvelocity());
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
    
    // Process action
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
    // This now contains authoritative position/physics data from the server
    
    // Parse the game state data
    processGameState(message.data);
}

void MultiplayerManager::processGameState(const std::vector<uint8_t>& gameStateData) {
    // This would parse the game state data and update all game objects
    // For now, just log the event since we don't have the full serialization format defined
    std::cout << "Game state update received from server (" << gameStateData.size() << " bytes)" << std::endl;
    
    // In a full implementation, this would:
    // 1. Update positions of all game objects
    // 2. Reconcile client-side predictions
    // 3. Update game state (scores, health, etc.)
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
    player->setposition(pos);
    player->setvelocity(vel);
}

std::vector<uint8_t> MultiplayerManager::serializePlayerInput(const PlayerInput* input) {
    std::vector<uint8_t> data;
    
    // Need to cast away const because PlayerInput getters aren't const methods
    PlayerInput* nonConstInput = const_cast<PlayerInput*>(input);
    
    uint8_t inputState = 0;
    if (nonConstInput->get_left()) inputState |= 1;
    if (nonConstInput->get_right()) inputState |= 2;
    if (nonConstInput->get_jump()) inputState |= 4;
    if (nonConstInput->get_attack()) inputState |= 8;
    
    // Add input state
    data.push_back(inputState);
    
    // Add sequence number (32-bit)
    data.push_back((inputSequenceNumber_ >> 24) & 0xFF);
    data.push_back((inputSequenceNumber_ >> 16) & 0xFF);
    data.push_back((inputSequenceNumber_ >> 8) & 0xFF);
    data.push_back(inputSequenceNumber_ & 0xFF);
    
    return data;
}