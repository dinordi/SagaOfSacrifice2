#include <iostream>
#include <cstring>
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "network/NetworkConfig.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include "interfaces/playerInput.h"  // Add player input header
#include "game.h"

// External function declaration
extern uint32_t get_ticks();

// RemotePlayer implementation
RemotePlayer::RemotePlayer(const std::string id) 
    : Object(BoxCollider(0,0,128,128), ObjectType::PLAYER, id),
      interpolationTime_(0.0f),
      targetPosition_(Vec2(0, 0)),
      targetVelocity_(Vec2(0, 0)) {
    
    std::filesystem::path base = std::filesystem::current_path();
    std::string temp = base.string();
    std::size_t pos = temp.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        temp = temp.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = std::filesystem::path(temp);
    basePath /= "SOS/assets/spriteatlas";

    addSpriteSheet(AnimationState::IDLE, basePath / "wolfman_idle.tpsheet");
    // addAnimation(AnimationState::IDLE, 0, 1, getCurrentSpriteData()->columns, 250, true);        // Idle animation (1 frames)
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::NORTH, 0, 1);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::WEST, 2,3);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::SOUTH, 4,5);
    animController.setDirectionRow(AnimationState::IDLE, FacingDirection::EAST, 6,7);

    addSpriteSheet(AnimationState::WALKING, basePath / "wolfman_walk.tpsheet");
    // addAnimation(AnimationState::WALKING, 0, 8, 9, 150, true);      // Walking animation (3 frames)
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::NORTH, 0,7);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::WEST, 8,15);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::SOUTH, 16, 23);
    animController.setDirectionRow(AnimationState::WALKING, FacingDirection::EAST, 24, 31);

    addSpriteSheet(AnimationState::ATTACKING, basePath / "wolfman_slash.tpsheet");

    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::NORTH, 0,4);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::WEST, 5,9);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::SOUTH, 10,14);
    animController.setDirectionRow(AnimationState::ATTACKING, FacingDirection::EAST, 15,19);

    // Set initial state
    setAnimationState(AnimationState::IDLE);
}

void RemotePlayer::update(float deltaTime) {
    // Smooth interpolation between current position and target position
    
    // Update interpolation timer
    interpolationTime_ += deltaTime;
    float t = std::min(interpolationTime_ / NetworkConfig::Client::InterpolationPeriod, 1.0f);

    Vec2 velocity_ = getvelocity();
    Vec2* position_ = &getcollider().position;
    
    // Only interpolate if we have a different target position
    if ((targetPosition_.x != position_->x || targetPosition_.y != position_->y) && t < 1.0f) {
        // Linear interpolation
        position_->x = position_->x + (targetPosition_.x - position_->x) * t;
        position_->y = position_->y + (targetPosition_.y - position_->y) * t;
        
        // Update velocity based on target velocity
        velocity_.x = velocity_.x + (targetVelocity_.x - velocity_.x) * t;
        velocity_.y = velocity_.y + (targetVelocity_.y - velocity_.y) * t;
    } else {
        // We've reached the target or never started interpolating, apply velocity directly
        position_->x += velocity_.x * deltaTime;
        position_->y += velocity_.y * deltaTime;
    }

    // Update the object's position
    setcollider(BoxCollider(*position_, getcollider().size));
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
      inputSequenceNumber_(0),
      partialGameState_(nullptr) {
    // Create the network interface
    network_ = std::make_unique<AsioNetworkClient>();
    std::filesystem::path base = std::filesystem::current_path();
    std::string temp = base.string();
    std::size_t pos = temp.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        temp = temp.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = std::filesystem::path(temp);
    basePath /= "SOS/assets/spriteatlas";
    atlasBasePath_ = basePath;  // Store base path for atlas
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
        // Sleep for a short time to ensure server is ready
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

void MultiplayerManager::update(float deltaTime) {
    if (!network_ || !network_->isConnected()) {
        std::cout << "No network in MPManager update" << std::endl;
        return;
    }
    
    // Process incoming messages
    network_->update();
    
    static uint64_t lastUpdateTime = 0;
    lastUpdateTime += static_cast<uint64_t>(deltaTime*1000);  // Convert to milliseconds

    // Check for timed-out partial game state updates
    if (partialGameState_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - partialGameState_->lastUpdateTime).count();
        
        // If we haven't received an update for more than 2 seconds, abandon this partial state
        if (elapsed > 2) {
            std::cerr << "[Client] Abandoning stale partial game state update" << std::endl;
            partialGameState_.reset();
        }
    }

    // Update all remote players
    for (auto& pair : remotePlayers_) {
        if(pair.second->getObjID() == playerId_) {
            // Skip updating the local player
            continue;
        }
        pair.second->update(deltaTime);
    }
    
    // Send player input periodically (primary control method now)
    if (playerInput_ && localPlayer_ && lastUpdateTime >= NetworkConfig::Client::UpdateInterval) {
        sendPlayerInput();
        sendPlayerState();
        lastUpdateTime_ = deltaTime;
    }
}

void MultiplayerManager::setLocalPlayer(Player* player) {
    localPlayer_ = player;
    // remotePlayers_[playerId_] = std::make_unique<RemotePlayer>(playerId_);
}

void MultiplayerManager::setPlayerInput(PlayerInput* input) {
    playerInput_ = input;
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
        case MessageType::GAME_STATE_DELTA:
            // Handle delta game state updates
            processGameStateDelta(message.data);
            break;
        case MessageType::GAME_STATE_PART:
            // Handle part of a multi-packet game state update
            processGameStatePart(message.data);
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
        std::cout << "[Client] New remote player added: " << message.senderId << std::endl;
    }
    
    if(it->second->getObjID() == playerId_) {
        // If this is our own player, skip processing
        return;
    }

    // Get pointer to remote player
    RemotePlayer* remotePlayer = it->second.get();
    
    // Deserialize the position data
    deserializePlayerState(message.data, remotePlayer);
    
    // Reset interpolation timer whenever we get a new position update
    remotePlayer->resetInterpolation();
    
    // Store the target position and velocity for interpolation
    remotePlayer->setTargetPosition(remotePlayer->getcollider().position);
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
    
    if (message.data.size() < 2) {
        std::cerr << "[Client] Game state data is too small: " << message.data.size() << " bytes" << std::endl;
        return;
    }
    
    // We now have different types of game state messages
    switch (message.type) {
        case MessageType::GAME_STATE:
            // Original full game state
            processGameState(message.data);
            break;
            
        case MessageType::GAME_STATE_DELTA:
            // Delta game state (only changed objects)
            processGameStateDelta(message.data);
            break;
            
        case MessageType::GAME_STATE_PART:
            // Part of a multi-packet game state update
            processGameStatePart(message.data);
            break;
            
        default:
            std::cerr << "[Client] Unknown game state message type: " << 
                static_cast<int>(message.type) << std::endl;
            break;
    }
}

void MultiplayerManager::processGameState(const std::vector<uint8_t>& gameStateData) {
    if (gameStateData.size() < 2) {
        std::cerr << "[Client] Invalid game state data received" << std::endl;
        return;
    }
    
    // The first 2 bytes contain the object count
    uint16_t objectCount = (static_cast<uint16_t>(gameStateData[0]) << 8) | 
                           static_cast<uint16_t>(gameStateData[1]);

    // std::cout << "[Client] Processing game state with " << objectCount 
    //           << " objects from server" << std::endl;
    
    // Current position in the data stream
    size_t pos = 2;
    
    // Maps to track objects we've seen in this update
    std::map<std::string, bool> objectsSeen;
    std::vector<std::shared_ptr<Object>> newObjects;
    
    // Process each object
    for (uint16_t i = 0; i < objectCount && pos < gameStateData.size(); i++) {
        // std::cout << "[Client] Processing object " << i + 1 << " of " << objectCount << ". Pos: " << pos << std::endl;
        std::shared_ptr<Object> newobj = deserializeObject(gameStateData, pos);
        if (!newobj) {
            continue; // Object deserialization failed, skip to next
        }
        newObjects.push_back(newobj);
        objectsSeen[newobj->getObjID()] = true;  // Mark this object as seen
    }
    // Add any new objects to the game
    if (Game* game = Game::getInstance()) {
        for (auto& obj : newObjects) {
            game->addObject(obj);
        }
    }
}

void MultiplayerManager::processGameStateDelta(const std::vector<uint8_t>& gameStateData) {
    if (gameStateData.size() < 2) {
        std::cerr << "[Client] Invalid delta game state data received" << std::endl;
        return;
    }
    
    // The first 2 bytes contain the object count
    uint16_t objectCount = (static_cast<uint16_t>(gameStateData[0]) << 8) | 
                           static_cast<uint16_t>(gameStateData[1]);
    
    // If it's 0, this is just a heartbeat message with no changes
    if (objectCount == 0) {
        return;
    }
    
    // std::cout << "[Client] Processing delta game state with " << objectCount 
    //           << " changed objects" << std::endl;
    
    // The rest of the processing is the same as processGameState
    // We're only receiving changed objects, so the processing logic remains the same
    
    // Current position in the data stream
    size_t pos = 2;
    
    // Track objects we've seen
    std::map<std::string, bool> objectsSeen;
    std::vector<std::shared_ptr<Object>> newObjects;
    
    // Process each object
    for (uint16_t i = 0; i < objectCount && pos < gameStateData.size(); i++) {
        std::shared_ptr<Object> newobj = deserializeObject(gameStateData, pos);
        if (!newobj) {
            continue; // Object deserialization success and there was no new object to be added
        }
        newObjects.push_back(newobj);
        objectsSeen[newobj->getObjID()] = true;  // Mark this object as seen
    }
    
    // Add any new objects to the game
    if (Game* game = Game::getInstance()) {
        for (auto& obj : newObjects) {
            game->addObject(obj);
        }
    }
}

void MultiplayerManager::processGameStatePart(const std::vector<uint8_t>& gameStateData) {
    if (gameStateData.size() < 7) {
        std::cerr << "[Client] Invalid partial game state data received (too small)" << std::endl;
        return;
    }
    
    // Parse metadata
    // First byte: flags (isFirst | isLast)
    uint8_t flags = gameStateData[0];
    bool isFirstPacket = (flags & 0x01) != 0;
    bool isLastPacket = (flags & 0x02) != 0;
    
    // Next 2 bytes: total object count
    uint16_t totalObjectCount = (static_cast<uint16_t>(gameStateData[1]) << 8) | 
                                static_cast<uint16_t>(gameStateData[2]);
    
    // Next 2 bytes: start index
    uint16_t startIndex = (static_cast<uint16_t>(gameStateData[3]) << 8) | 
                          static_cast<uint16_t>(gameStateData[4]);
    
    // Next 2 bytes: object count in this packet
    uint16_t packetObjectCount = (static_cast<uint16_t>(gameStateData[5]) << 8) | 
                                 static_cast<uint16_t>(gameStateData[6]);
    
    std::cout << "[Client] Received game state part: " 
              << (isFirstPacket ? "first " : "")
              << (isLastPacket ? "last " : "")
              << "packet with " << packetObjectCount 
              << " objects (total: " << totalObjectCount 
              << ", starting at index: " << startIndex << ")" << std::endl;
    
    // If this is the first packet, initialize our partial state storage
    if (isFirstPacket) {
        partialGameState_ = std::make_unique<PartialGameState>();
        partialGameState_->totalObjectCount = totalObjectCount;
        partialGameState_->complete = false;
        partialGameState_->packetIndices.clear();
        partialGameState_->parts.clear();
        partialGameState_->lastUpdateTime = std::chrono::steady_clock::now();
    }
    
    // If we don't have an active partial state or the object count doesn't match,
    // something is wrong - reset and wait for the next full update
    if (!partialGameState_ || partialGameState_->totalObjectCount != totalObjectCount) {
        std::cerr << "[Client] Received partial game state but no valid state accumulator exists" << std::endl;
        partialGameState_.reset();
        return;
    }
    
    // Reset timeout
    partialGameState_->lastUpdateTime = std::chrono::steady_clock::now();
    
    // Store this part (we store just the object data part, skipping the header)
    std::vector<uint8_t> objectData(gameStateData.begin() + 7, gameStateData.end());
    
    // Store the part with its corresponding start index
    partialGameState_->parts.push_back(objectData);
    partialGameState_->packetIndices.push_back(startIndex);
    
    // If this is the last packet, process the complete state
    if (isLastPacket) {
        partialGameState_->complete = true;
        
        bool hasAllParts = true;
        std::set<uint16_t> receivedIndices;
        for (auto idx : partialGameState_->packetIndices) {
            receivedIndices.insert(idx);
        }
        
        // Sort the indices to see what we have
        std::vector<uint16_t> sortedIndices(receivedIndices.begin(), receivedIndices.end());
        std::sort(sortedIndices.begin(), sortedIndices.end());
        
        // Check if we have all required parts by ensuring we have packets covering all objects
        if (sortedIndices.empty() || sortedIndices.back() + packetObjectCount < totalObjectCount) {
            hasAllParts = false;
            std::cerr << "[Client] Incomplete game state: highest index " 
                    << (sortedIndices.empty() ? 0 : sortedIndices.back())
                    << " doesn't cover all " << totalObjectCount << " objects" << std::endl;
        }
        
        // If we're missing parts, don't process
        if (!hasAllParts) {
            std::cerr << "[Client] Missing parts of game state update: received " 
                    << partialGameState_->parts.size() << " parts" << std::endl;
            
            // For debugging, show what parts we do have
            std::cout << "[Client] Received parts at indices: ";
            for (auto idx : sortedIndices) {
                std::cout << idx << " ";
            }
            std::cout << std::endl;
            
            return;
        }
        
        // Sort parts by start index to ensure correct order
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> indexedParts;
        for (size_t i = 0; i < partialGameState_->parts.size(); i++) {
            indexedParts.push_back({partialGameState_->packetIndices[i], partialGameState_->parts[i]});
        }
        std::sort(indexedParts.begin(), indexedParts.end(), 
                 [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Combine all parts into a single game state
        std::vector<uint8_t> completeState;
        
        // Add total object count
        completeState.push_back(static_cast<uint8_t>(totalObjectCount >> 8));
        completeState.push_back(static_cast<uint8_t>(totalObjectCount & 0xFF));
        
        // Add all object data in correct order
        for (const auto& [_, part] : indexedParts) {
            completeState.insert(completeState.end(), part.begin(), part.end());
        }
        
        // Process the complete state
        processGameState(completeState);
        
        // Clear the partial state
        partialGameState_.reset();
    }
}

void MultiplayerManager::handleChatMessage(const NetworkMessage& message) {
    // Extract chat message from data
    std::string chatText(message.data.begin(), message.data.end());
    
    // Call the chat handler if set
    if (chatHandler_) {
        chatHandler_(message.senderId, chatText);
    }
}

std::vector<uint8_t> MultiplayerManager::serializePlayerInput(const PlayerInput* input) {
    std::vector<uint8_t> data;
    
    // For now we just use a simple bit field to represent input state
    uint8_t inputBits = 0;
    
    if (input->get_left()) inputBits |= 0x01;
    if (input->get_right()) inputBits |= 0x02;
    if (input->get_up()) inputBits |= 0x04;
    if (input->get_down()) inputBits |= 0x08;
    if (input->get_attack()) inputBits |= 0x10;
    
    // Add the input bits to the data
    data.push_back(inputBits);
    
    // Add sequence number (useful for client-side prediction)
    data.push_back(static_cast<uint8_t>((inputSequenceNumber_ >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(inputSequenceNumber_ & 0xFF));
    
    return data;
}

std::vector<uint8_t> MultiplayerManager::serializePlayerState(const Player* player) {
    std::vector<uint8_t> data;
    
    // Get player position and velocity
    const Vec2& pos = player->getcollider().position;
    const Vec2& vel = player->getvelocity();
    
    // Allocate space for the data (2 Vec2s = 4 floats = 16 bytes)
    data.resize(16);
    
    // Copy position (8 bytes)
    std::memcpy(&data[0], &pos.x, sizeof(float));
    std::memcpy(&data[4], &pos.y, sizeof(float));
    
    // Copy velocity (8 bytes)
    std::memcpy(&data[8], &vel.x, sizeof(float));
    std::memcpy(&data[12], &vel.y, sizeof(float));

    // Add direction and animation state
    data.push_back(static_cast<uint8_t>(player->getDir()));
    data.push_back(static_cast<uint8_t>(player->getAnimationState()));
    
    return data;
}

std::shared_ptr<Object> MultiplayerManager::updateEntityPosition(const std::string& objectId, const Vec2& position, const Vec2& velocity) {
    // Update the position of a specific entity
    Game* game = Game::getInstance();
    if (!game) {
        std::cerr << "[Client] Game instance not found" << std::endl;
        return nullptr;
    }
    
    auto& objects = game->getObjects();
    for (auto& obj : objects) {
        if (obj->getObjID() == objectId) {
            // Update the object's position and velocity
            obj->setcollider(BoxCollider(position, obj->getcollider().size));
            obj->setvelocity(velocity);
            return obj; // Successfully updated
        }
    }
    
    return nullptr; // Object not found
}

void MultiplayerManager::deserializePlayerState(const std::vector<uint8_t>& data, RemotePlayer* player) {
    if (data.size() < 18) { // 16 bytes for position/velocity + 2 bytes for state/dir
        std::cerr << "[Client] Invalid player state data size: " << data.size() << std::endl;
        return;
    }
    
    // Extract position
    float posX, posY;
    std::memcpy(&posX, &data[0], sizeof(float));
    std::memcpy(&posY, &data[4], sizeof(float));
    
    // Extract velocity
    float velX, velY;
    std::memcpy(&velX, &data[8], sizeof(float));
    std::memcpy(&velY, &data[12], sizeof(float));
    
    // Extract direction and animation state
    FacingDirection dir = static_cast<FacingDirection>(data[16]);
    AnimationState animState = static_cast<AnimationState>(data[17]);
    
    // Update the remote player
    player->setDir(dir);
    player->setAnimationState(animState);
    player->setTargetPosition(Vec2(posX, posY));
    player->setTargetVelocity(Vec2(velX, velY));
    player->resetInterpolation();
}

std::shared_ptr<Object> MultiplayerManager::deserializeObject(const std::vector<uint8_t>& data, size_t& pos) {
    if (pos + 2 > data.size()) {
        std::cerr << "[Client] Not enough data to read object type and ID length" << std::endl;
        return nullptr;
    }
    
    // Read object type
    uint8_t objectType = data[pos++];
    
    // Read object ID length
    uint8_t idLength = data[pos++];
    
    if (pos + idLength > data.size()) {
        std::cerr << "[Client] Not enough data to read object ID" << std::endl;
        return nullptr;
    }
    
    // Read object ID
    std::string objectId(data.begin() + pos, data.begin() + pos + idLength);
    pos += idLength;
    
    // Read position and velocity (4 floats, 16 bytes total)
    if (pos + 16 > data.size()) {
        std::cerr << "[Client] Not enough data to read position and velocity" << std::endl;
        return nullptr;
    }
    
    float posX, posY, velX, velY;
    std::memcpy(&posX, &data[pos], sizeof(float));
    pos += sizeof(float);
    std::memcpy(&posY, &data[pos], sizeof(float));
    pos += sizeof(float);
    std::memcpy(&velX, &data[pos], sizeof(float));
    pos += sizeof(float);
    std::memcpy(&velY, &data[pos], sizeof(float));
    pos += sizeof(float);
    
    // Create the appropriate object based on type
    switch (static_cast<ObjectType>(objectType)) {
        case ObjectType::PLAYER: {
            // Find or create a remote player
            auto it = remotePlayers_.find(objectId);
            if (it == remotePlayers_.end()) {
                // Create new remote player
                auto newPlayer = std::make_unique<RemotePlayer>(objectId);
                it = remotePlayers_.emplace(objectId, std::move(newPlayer)).first;
            }
            RemotePlayer* player = it->second.get();
            AnimationState state = static_cast<AnimationState>(data[pos++]);
            FacingDirection dir = static_cast<FacingDirection>(data[pos++]);

            player->setDir(dir);
            player->setAnimationState(state);
            player->setTargetPosition(Vec2(posX, posY));
            player->setTargetVelocity(Vec2(velX, velY));
            player->resetInterpolation();
            return nullptr;
        }
        case ObjectType::TILE: {
            // Find or create a platform object
            Game* game = Game::getInstance();
            if (!game) {
                std::cerr << "[Client] Game instance not found" << std::endl;
                return nullptr;
            }
            auto& objects = game->getObjects();
            for (auto& obj : objects) {
                if (obj->getObjID() == objectId) {
                    // Update existing platform
                    obj->setcollider(BoxCollider(Vec2(posX, posY), obj->getcollider().size));
                    obj->setvelocity(Vec2(velX, velY));
                    return obj; // Successfully updated
                }
            }
            
            uint8_t tileIndex = 0; // Default tile index
            tileIndex = data[pos++];
            uint32_t flags;
            flags = (static_cast<uint32_t>(data[pos+3]) << 24) |
            (static_cast<uint32_t>(data[pos + 2]) << 16) |
            (static_cast<uint32_t>(data[pos + 1]) << 8) |
            static_cast<uint32_t>(data[pos]);
            pos += 4; // Move past the flags
            
            // Create new platform
            std::shared_ptr<Tile> platform = std::make_shared<Tile>(
                posX, posY,
                objectId,
                "Tilemap_Flat", tileIndex, 64, 64, 12 // Default tile index and size
            );

            platform->setFlag(flags);
            
            platform->setupAnimations(atlasBasePath_);
            platform->setcollider(BoxCollider(Vec2(posX, posY), Vec2(64, 64))); // Default size
            platform->setvelocity(Vec2(velX, velY));
            return platform;
        }
        case ObjectType::MINOTAUR: {
            AnimationState state = static_cast<AnimationState>(data[pos++]);
            FacingDirection dir = static_cast<FacingDirection>(data[pos++]);
            // Find or create a minotaur object
            Game* game = Game::getInstance();
            if (!game) {
                std::cerr << "[Client] Game instance not found" << std::endl;
                return nullptr;
            }
            auto& objects = game->getObjects();
            for (auto& obj : objects) {
                if (obj->getObjID() == objectId) {
                    obj->setAnimationState(state);
                    obj->setDir(dir);
                    // Update existing minotaur
                    obj->setcollider(BoxCollider(Vec2(posX, posY), obj->getcollider().size));
                    obj->setvelocity(Vec2(velX, velY));
                    return obj; // Successfully updated
                }
            }
            // Create new minotaur
            auto newMinotaur = std::make_shared<Minotaur>(posX, posY, objectId);
            newMinotaur->setupAnimations(atlasBasePath_);
            newMinotaur->setcollider(BoxCollider(Vec2(posX, posY), Vec2(64, 64))); // Default size
            newMinotaur->setvelocity(Vec2(velX, velY));
            return newMinotaur;
        }
        default:
            std::cerr << "[Client] Unknown object type: " << static_cast<int>(objectType) << std::endl;
            return nullptr;
    }
    
}

void MultiplayerManager::handlePlayerConnectMessage(const NetworkMessage& message) {
    // Extract player name from data
    std::string playerName(message.data.begin(), message.data.end());
    
    std::cout << "[Client] Player connected: " << message.senderId 
              << " (" << playerName << ")" << std::endl;
    
    // Don't create a remote player for ourselves
    if (message.senderId == playerId_) {
        return;
    }
    
    // Create a new remote player
    auto it = remotePlayers_.find(message.senderId);
    if (it == remotePlayers_.end()) {
        remotePlayers_[message.senderId] = std::make_unique<RemotePlayer>(message.senderId);
        std::cout << "[Client] Created new remote player: " << message.senderId << std::endl;
    }
}

void MultiplayerManager::handlePlayerDisconnectMessage(const NetworkMessage& message) {
    std::cout << "[Client] Player disconnected: " << message.senderId << std::endl;
    
    // Remove the remote player
    auto it = remotePlayers_.find(message.senderId);
    if (it != remotePlayers_.end()) {
        remotePlayers_.erase(it);
        std::cout << "[Client] Removed remote player: " << message.senderId << std::endl;
    }
}
