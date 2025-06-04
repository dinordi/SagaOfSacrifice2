#include "network/EmbeddedServer.h"
#include "objects/player.h"
#include "objects/tile.h"
#include "objects/minotaur.h"
#include "player_manager.h"
#include <iostream>
#include <cmath>

NetworkMessage EmbeddedServer::deserializeMessage(const std::vector<uint8_t>& data, const std::string& clientId) {
    NetworkMessage message;
    
    // Make sure we have at least the type byte
    if (data.empty()) {
        std::cerr << "[EmbeddedServer] Empty message data received" << std::endl;
        message.type = MessageType::PING; // Default to harmless message type
        message.senderId = clientId; // Use the provided client ID
        return message;
    }
    
    // Extract message type
    message.type = static_cast<MessageType>(data[0]);
    
    // Extract sender ID if present
    if (data.size() >= 2) {
        uint8_t idLength = data[1];
        
        if (data.size() >= 2 + idLength) {
            message.senderId = std::string(data.begin() + 2, data.begin() + 2 + idLength);
        } else {
            // Invalid ID length, use provided client ID
            std::cerr << "[EmbeddedServer] Invalid sender ID length in message" << std::endl;
            message.senderId = clientId;
        }
        
        // Extract message data
        if (data.size() > 2 + idLength) {
            message.data = std::vector<uint8_t>(data.begin() + 2 + idLength, data.end());
        }
    } else {
        // No sender ID, use provided client ID
        message.senderId = clientId;
    }
    
    return message;
}

void EmbeddedServer::processPlayerInput(const std::string& playerId, const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    return;
    // Lookup the player
    auto& pm = PlayerManager::getInstance();
    auto player = pm.getPlayer(playerId);
    if (!player) {
        std::cerr << "[EmbeddedServer] Player not found for input: " << playerId << std::endl;
        return;
    }

    // Validate payload
    if (message.data.size() < 1) {
        std::cerr << "[EmbeddedServer] Invalid input data size: " << message.data.size() << " bytes" << std::endl;
        return;
    }

    // Unpack the bitflags
    uint8_t bits = message.data[0];
    bool left = (bits & 0x01) != 0;
    bool right = (bits & 0x02) != 0;
    bool up = (bits & 0x04) != 0;
    bool down = (bits & 0x08) != 0;
    bool jump = (bits & 0x10) != 0;
    bool attack = (bits & 0x20) != 0;

    // Apply input to player
    Vec2 moveDir(0, 0);
    if (left) moveDir.x -= 1.0f;
    if (right) moveDir.x += 1.0f;
    if (up) moveDir.y -= 1.0f;
    if (down) moveDir.y += 1.0f;

    // Normalize diagonal movement
    if (moveDir.x != 0 && moveDir.y != 0) {
        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        moveDir.x /= len;
        moveDir.y /= len;
    }

    // Update player velocity based on input
    const float PLAYER_SPEED = 200.0f;
    player->setvelocity(Vec2(moveDir.x * PLAYER_SPEED, moveDir.y * PLAYER_SPEED));
    
    // Set player direction based on movement
    if (moveDir.x < 0) {
        player->setDir(FacingDirection::WEST);
    } else if (moveDir.x > 0) {
        player->setDir(FacingDirection::EAST);
    }
    
    // Set animation state
    if (moveDir.x != 0 || moveDir.y != 0) {
        player->setAnimationState(AnimationState::WALKING);
    } else {
        player->setAnimationState(AnimationState::IDLE);
    }
    
    // Handle jump and attack states
    if (jump) {
        player->setAnimationState(AnimationState::JUMPING);
    } else if (attack) {
        player->setAnimationState(AnimationState::ATTACKING);
    }
}

void EmbeddedServer::processPlayerPosition(const std::string& playerId, const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // This is used as a fallback/reconciliation mechanism
    // The server is authoritative, but we allow clients to send occasional position updates
    // which can help correct errors or deal with special conditions
    
    // Lookup the player
    auto& pm = PlayerManager::getInstance();
    auto player = pm.getPlayer(playerId);
    if (!player) {
        std::cerr << "[EmbeddedServer] Player not found for position update: " << playerId << std::endl;
        return;
    }

    // Need at least 16 bytes for position/velocity
    if (message.data.size() < 16) {
        std::cerr << "[EmbeddedServer] Invalid player position data size: " << message.data.size() << std::endl;
        return;
    }
    
    // Extract position and velocity
    float posX, posY, velX, velY;
    std::memcpy(&posX, &message.data[4], sizeof(float));
    std::memcpy(&posY, &message.data[8], sizeof(float));
    std::memcpy(&velX, &message.data[12], sizeof(float));
    std::memcpy(&velY, &message.data[16], sizeof(float));
    
    // Update player state
    player->setcollider(BoxCollider(Vec2(posX, posY), player->getcollider().size));
    player->setvelocity(Vec2(velX, velY));
    
    // Extract animation state and direction if provided
    if (message.data.size() >= 18) {
        FacingDirection dir = static_cast<FacingDirection>(message.data[20]);
        AnimationState animState = static_cast<AnimationState>(message.data[21]);
        
        player->setDir(dir);
        player->setAnimationState(animState);
        if(animState == AnimationState::ATTACKING) {
            player->attack();
        }
    }
}

void EmbeddedServer::processEnemyState(
    const std::string& playerId,
    const NetworkMessage& message
) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);

    // Parse enemy state data from message
    if (message.data.size() < sizeof(uint32_t) + sizeof(bool) + sizeof(int16_t)) {
        std::cerr << "[EmbeddedServer] Invalid enemy state message size" << std::endl;
        return;
    }

    // Extract enemy ID, isDead flag, and health
    size_t offset = 4;
    
    // Read enemy ID length (1 byte)
    uint8_t enemyIdLength;
    enemyIdLength = message.data[offset++];
    
    // Read enemy ID string
    if (offset + enemyIdLength > message.data.size()) {
        std::cerr << "[EmbeddedServer] Invalid enemy ID length" << std::endl;
        return;
    }
    std::string enemyId(
        message.data.begin() + offset,
        message.data.begin() + offset + enemyIdLength
    );
    offset += enemyIdLength;
    
    // Read isDead flag (1 byte)
    if (offset >= message.data.size()) {
        std::cerr << "[EmbeddedServer] Missing isDead flag" << std::endl;
        return;
    }
    bool isDead = message.data[offset] != 0;
    offset += 1;
    
    // Read health (2 bytes)
    if (offset + sizeof(int16_t) > message.data.size()) {
        std::cerr << "[EmbeddedServer] Missing health data" << std::endl;
        return;
    }
    int16_t currentHealth;
    std::memcpy(&currentHealth, message.data.data() + offset, sizeof(int16_t));
    offset += sizeof(int16_t);


    // Update enemy state in the level
    if (levelManager_ && levelManager_->getCurrentLevel()) {
        Level* currentLevel = levelManager_->getCurrentLevel();
        
        // Find the enemy in the level
        auto& objects = currentLevel->getObjects();
        auto enemyIt = std::find_if(objects.begin(), objects.end(),
            [&enemyId](const std::shared_ptr<Object>& obj) {
                return obj && obj->getObjID() == enemyId;
            });
        
        if (enemyIt != objects.end()) {
            if (isDead) {
                // Remove the enemy from the level
                currentLevel->removeObject(*enemyIt);
                std::cout << "[EmbeddedServer] Removed dead enemy: " << enemyId << std::endl;
            } else {
                // Update enemy health
                Enemy* enemy = dynamic_cast<Enemy*>(enemyIt->get());
                if (enemy) {
                    enemy->setHealth(currentHealth);
                    std::cout << "[EmbeddedServer] Updated enemy " << enemyId << " health to " << currentHealth << std::endl;
                }
            }
            
            // Broadcast the enemy state change to ALL other clients
            sendEnemyStateToClients(enemyId, isDead, currentHealth);
        }
    }
}

void EmbeddedServer::sendEnemyStateToClients(const std::string& enemyId, bool isDead, int16_t health)
{
    NetworkMessage enemyMsg;
    enemyMsg.type = MessageType::ENEMY_STATE_UPDATE;


    enemyMsg.data.push_back(static_cast<uint8_t>(enemyId.size()));
    enemyMsg.data.insert(enemyMsg.data.end(), enemyId.begin(), enemyId.end());

    enemyMsg.data.push_back(static_cast<uint8_t>(isDead == true ? 1 : 0));

    // Add health (2 bytes) - Use memcpy for consistent byte order
    size_t currentSize = enemyMsg.data.size();
    enemyMsg.data.resize(currentSize + sizeof(int16_t));
    std::memcpy(enemyMsg.data.data() + currentSize, &health, sizeof(int16_t));
    
    
    // Broadcast to all clients
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        if (clientSockets_.empty()) {
            return;
        }
        
        for (auto& [id, sock] : clientSockets_) {
            if (sock && sock->is_open()) {
                sendToClient(sock, enemyMsg);
            }
        }
    }
}