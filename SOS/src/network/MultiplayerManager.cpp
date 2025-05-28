#include <iostream>
#include <cstring>
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "network/NetworkConfig.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include "interfaces/playerInput.h"  // Add player input header
#include "game.h"
#include <algorithm>

// External function declaration
extern uint32_t get_ticks();

// RemotePlayer implementation
RemotePlayer::RemotePlayer(const std::string& id) 
    : Object(BoxCollider(0,0,128,128), ObjectType::PLAYER, id), 
      id_(id),
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
      inputSequenceNumber_(0) {
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

void MultiplayerManager::sendEnemyStateUpdate(const std::string& enemyId, bool isDead, int currentHealth) {
    if (!network_ || !network_->isConnected()) {
        return;
    }
    
    std::cout << "[MultiplayerManager] Sending enemy state update for " << enemyId 
              << ", isDead: " << (isDead ? "true" : "false") 
              << ", health: " << currentHealth << std::endl;
    
    // Create a message to inform the server about the enemy state
    NetworkMessage enemyStateMsg;
    enemyStateMsg.type = MessageType::ENEMY_STATE_UPDATE; // Reuse existing message type
    enemyStateMsg.senderId = playerId_; // Send as the local player
    
    // Encode the enemy state in the data payload
    // Format: [enemy ID length][enemy ID string][1 byte isDead][4 bytes health]
    std::vector<uint8_t> data; 
    // Add enemy ID length (1 byte)
    data.push_back(static_cast<uint8_t>(enemyId.size()));
    // Add enemy ID string
    data.insert(data.end(), enemyId.begin(), enemyId.end());
    
    // Add isDead flag
    data.push_back(isDead ? 1 : 0);
    
    // Add current health (4 bytes)
    data.push_back((currentHealth >> 24) & 0xFF);
    data.push_back((currentHealth >> 16) & 0xFF);
    data.push_back((currentHealth >> 8) & 0xFF);
    data.push_back(currentHealth & 0xFF);
    
    enemyStateMsg.data = data;
    
    // Send the message to the server
    network_->sendMessage(enemyStateMsg);
}

void MultiplayerManager::handleEnemyStateMessage(const NetworkMessage& message) {
    // This method is called by the server to handle enemy state updates
    // It updates enemy health, dead status, etc.
    if (message.data.size() < 2) {
        std::cerr << "[Client] Invalid enemy state update message received" << std::endl;
        return;
    }
    // Format same as server: [1 byte action type][enemy ID string][1 byte isDead][4 bytes health]
    size_t pos = 0;

    // Read enemy ID length
    size_t idLength = message.data[pos++];
    std::string enemyId(message.data.begin() + pos, message.data.begin() + pos + idLength);
    pos += idLength;

    // Read isDead flag
    if (pos >= message.data.size()) {
        std::cerr << "[Client] Missing isDead flag in enemy state update" << std::endl;
        return;
    }
    bool isDead = message.data[pos++] != 0;
    // Read health (4 bytes)
    if (pos + 4 > message.data.size()) {
        std::cerr << "[Client] Missing health value in enemy state update" << std::endl;
        return;
    }
    int health = (message.data[pos] << 24) |
                 (message.data[pos + 1] << 16) |
                 (message.data[pos + 2] << 8) |
                  message.data[pos + 3];
    pos += 4;
    std::cout << "[Client] Received enemy state update for " << enemyId 
              << ", isDead: " << (isDead ? "true" : "false") 
              << ", health: " << health << std::endl;
    // Find the remote player or enemy object
    Game * game = Game::getInstance();
    if (!game) {
        std::cerr << "[Client] Game instance not found" << std::endl;
        return;
    }
    // Find the enemy object by ID in gameObjects
    std::shared_ptr<Enemy> enemyObject = nullptr;
    auto& objects = game->getObjects();
    for (auto& obj : objects) {
        if (obj->getObjID() == enemyId) {
            switch(obj->type) {
                case ObjectType::MINOTAUR:
                    // Cast to Enemy type
                    enemyObject = std::static_pointer_cast<Minotaur>(obj);
                    break;
                default:
                    std::cerr << "[Client] Object with ID " << enemyId << " is not recognized as an enemy type" << std::endl;
                    return;
            }
            break;
        }
    }
    if (!enemyObject) {
        std::cerr << "[Client] Enemy object not found: " << enemyId << std::endl;
        return;
    }
    // Update the enemy state
    if (isDead) {
        std::cout << "[Client] Enemy " << enemyId << " has died" << std::endl;
        // Handle enemy death logic here, e.g., remove from game, play death animation, etc.
        enemyObject->die();  // Assuming die() method handles cleanup
    } else {
        std::cout << "[Client] Enemy " << enemyId << " is alive with health: " << health << std::endl;
        // Update health or other properties as needed
        enemyObject->setHealth(health);  // Assuming setHealth() method exists
    }

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
        case MessageType::CHAT_MESSAGE:
            handleChatMessage(message);
            break;
        case MessageType::CONNECT:
            handlePlayerConnectMessage(message);
            break;
        case MessageType::DISCONNECT:
            handlePlayerDisconnectMessage(message);
            break;
        case MessageType::ENEMY_STATE_UPDATE:
            // Handle enemy state updates (e.g., when an enemy dies)
            // This is a new message type for server-controlled physics
            // We can process this in the same way as player actions
            handleEnemyStateMessage(message);
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
    
    // Parse the game state data
    processGameState(message.data);
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
        // Read object type
        if (pos >= gameStateData.size()) break;
        uint8_t objectType = gameStateData[pos++];
        
        // Read object ID length
        if (pos >= gameStateData.size()) break;
        uint8_t idLength = gameStateData[pos++];
        
        // Read object ID
        if (pos + idLength > gameStateData.size()) break;
        std::string objectId(gameStateData.begin() + pos, gameStateData.begin() + pos + idLength);
        // std::cout << "[Client] Processing object ID: " << objectId 
        //           << " of type: " << (objectType) << std::endl;
        pos += idLength;
        
        // Read position and velocity (4 floats, 16 bytes total)
        if (pos + 16 > gameStateData.size()) break;
        
        float posX, posY, velX, velY;
        std::memcpy(&posX, &gameStateData[pos], sizeof(float));
        pos += sizeof(float);
        std::memcpy(&posY, &gameStateData[pos], sizeof(float));
        pos += sizeof(float);
        std::memcpy(&velX, &gameStateData[pos], sizeof(float));
        pos += sizeof(float);
        std::memcpy(&velY, &gameStateData[pos], sizeof(float));
        pos += sizeof(float);
        
        // Mark this object as seen
        objectsSeen[objectId] = true;
        
        // Process based on object type
        switch (static_cast<ObjectType>(objectType)) {
            case ObjectType::PLAYER: { // Player
                // Skip if this is our local player
                // if (objectId == playerId_) {
                //     // Position and velocity are already handled by the local player and reconciliation
                // }
                
                // Find or create remote player
                auto it = remotePlayers_.find(objectId);
                if (it == remotePlayers_.end()) {
                    // Create new remote player
                    auto newPlayer = std::make_unique<RemotePlayer>(objectId);
                    it = remotePlayers_.emplace(objectId, std::move(newPlayer)).first;
                    std::cout << "[Client] Created new remote player: " << objectId << std::endl;
                }

                AnimationState state = static_cast<AnimationState>(gameStateData[pos++]);
                FacingDirection dir = static_cast<FacingDirection>(gameStateData[pos++]);

                
                // Update remote player state
                RemotePlayer* player = it->second.get();
                
                player->setDir(dir);
                player->setAnimationState(state);
                player->setTargetPosition(Vec2(posX, posY));
                player->setTargetVelocity(Vec2(velX, velY));
                player->resetInterpolation();
                break;
            }
            case ObjectType::TILE: { // Platform
                // Read platform width and height (2 floats, 8 bytes total)
                if (pos + 8 > gameStateData.size()) break;
                
                // float width, height;
                // std::memcpy(&width, &gameStateData[pos], sizeof(float));
                // pos += sizeof(float);
                // std::memcpy(&height, &gameStateData[pos], sizeof(float));
                // pos += sizeof(float);

                // Read tile index (1 byte)
                if (pos >= gameStateData.size()) break;
                uint8_t tileIndex = gameStateData[pos++];

                if (pos >= gameStateData.size()) break;
                uint32_t flags;
                std::memcpy(&flags, &gameStateData[pos], sizeof(uint32_t));
                pos += sizeof(uint32_t);
                
                // Check if we need to create a new platform object
                bool found = false;
                
                // Let the Game class handle object management
                if (Game* game = Game::getInstance()) {
                    // Check if the platform already exists
                    auto& objects = game->getObjects();
                    for (auto& obj : objects) {
                        if (obj->getObjID() == objectId) {
                            // Update existing platform
                            obj->setcollider(BoxCollider(Vec2(posX, posY), obj->getcollider().size));
                            obj->setvelocity(Vec2(velX, velY));
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        // Create new platform
                        std::shared_ptr<Tile> platform = std::make_shared<Tile>(
                            posX, posY,
                            objectId,
                            "Tilemap_Flat", tileIndex, 64, 64, 12
                        );
                        platform->setupAnimations(atlasBasePath_);
                        platform->setFlag(flags);
                        newObjects.push_back(platform);
                        // std::cout << "[Client] Created new platform: " << objectId << " at " 
                        //           << posX << "," << posY << " size: " << width << "x" << height << std::endl;
                    }
                }
                break;
            }
            case ObjectType::MINOTAUR: {
                    // std::cout << "[Client] Minotaur object received: " << objectId 
                    //     << " at " << posX << "," << posY
                    //     << " with velocity: " << velX << "," << velY << std::endl; 
                    AnimationState state = static_cast<AnimationState>(gameStateData[pos++]);
                    FacingDirection dir = static_cast<FacingDirection>(gameStateData[pos++]);

                    std::shared_ptr<Object> existingObject = updateEntityPosition(objectId, Vec2(posX, posY), Vec2(velX, velY));
                    if (existingObject == nullptr) {
                        // Create new platform
                        auto newEntity = std::make_shared<Minotaur>(posX, posY, objectId);
                        newEntity->setupAnimations(atlasBasePath_);
                        newEntity->setDir(dir);
                        newEntity->setAnimationState(state);
                        newEntity->setvelocity(Vec2(velX, velY));
                        newObjects.push_back(newEntity);
                    }
                    else
                    {
                        Minotaur* existingMinotaur = static_cast<Minotaur*>(existingObject.get());
                        existingMinotaur->setDir(dir);
                        existingMinotaur->setAnimationState(state);
                    }
                    break;
                }
            default:
                std::cerr << "[Client] Unknown object type: " << static_cast<int>(objectType) << std::endl;
                break;
            }
        }
        // Add any new objects to the game
        if (Game* game = Game::getInstance()) {
            for (auto& obj : newObjects) {
                game->addObject(obj);
            }
        }
}

// Returns true if the position was updated successfully, false if the object was not found
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
            // std::cout << "[Client] Updating position of object: " << objectId 
            //           << " to " << position.x << "," << position.y 
            //           << " with velocity: " << velocity.x << "," << velocity.y << std::endl;
            // Update the object's position and velocity
            obj->setcollider(BoxCollider(position, obj->getcollider().size));
            obj->setvelocity(velocity);
            return obj; // Successfully updated
        }
    }
    return nullptr;
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
    player->setcollider(BoxCollider(pos, player->getcollider().size));
    player->setvelocity(vel);
}

std::vector<uint8_t> MultiplayerManager::serializePlayerInput(const PlayerInput* input) {
    std::vector<uint8_t> data;
    
    // Need to cast away const because PlayerInput getters aren't const methods
    PlayerInput* nonConstInput = const_cast<PlayerInput*>(input);
    
    uint8_t inputState = 0;
    if (nonConstInput->get_left()) inputState |= 1;
    if (nonConstInput->get_right()) inputState |= 2;
    if (nonConstInput->get_up()) inputState |= 4;
    if (nonConstInput->get_down()) inputState |= 8;
    if (nonConstInput->get_attack()) inputState |= 16;
    
    // Add input state
    data.push_back(inputState);
    
    // Add sequence number (32-bit)
    data.push_back((inputSequenceNumber_ >> 24) & 0xFF);
    data.push_back((inputSequenceNumber_ >> 16) & 0xFF);
    data.push_back((inputSequenceNumber_ >> 8) & 0xFF);
    data.push_back(inputSequenceNumber_ & 0xFF);
    
    return data;
}

void MultiplayerManager::handleChatMessage(const NetworkMessage& message) {
    // Extract the chat message from the data
    std::string chatMessage(message.data.begin(), message.data.end());
    std::cout << "[Client] Received chat message from " << message.senderId << ": " << chatMessage << std::endl;
    
    // If we have a chat handler registered, call it
    if (chatHandler_) {
        chatHandler_(message.senderId, chatMessage);
    }
}

void MultiplayerManager::handlePlayerConnectMessage(const NetworkMessage& message) {
    std::cout << "[Client] Received player connect message from " << message.senderId << std::endl;
    // A new player has connected, extract the player info from the message
    std::string playerId = message.senderId;
    std::string playerInfo;
    
    if (!message.data.empty()) {
        playerInfo = std::string(message.data.begin(), message.data.end());
    }
    
    std::cout << "[Client] New player connected: " << playerId << " - " << playerInfo << std::endl;
    
    // Check if this player is already known
    if (remotePlayers_.find(playerId) != remotePlayers_.end()) {
        std::cout << "[Client] Player " << playerId << " already exists, not creating a new remote player" << std::endl;
        return;
    }
    
    // Create a new remote player
    auto newPlayer = std::make_unique<RemotePlayer>(playerId);
    remotePlayers_.emplace(playerId, std::move(newPlayer));
    
    // Notify any listeners about the new player (e.g., UI updates)
    if (chatHandler_) {
        chatHandler_("SYSTEM", "Player " + playerId + " joined the game");
    }
}

void MultiplayerManager::handlePlayerDisconnectMessage(const NetworkMessage& message) {
    // A player has disconnected
    std::string playerId = message.senderId;
    std::cout << "[Client] Player disconnected: " << playerId << std::endl;
    
    // Remove the player from our collection of remote players
    remotePlayers_.erase(playerId);
    
    // Notify any listeners about the player leaving
    if (chatHandler_) {
        chatHandler_("SYSTEM", "Player " + playerId + " left the game");
    }
}