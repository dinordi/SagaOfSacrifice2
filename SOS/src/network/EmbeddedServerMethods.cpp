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
        FacingDirection dir = static_cast<FacingDirection>(message.data[16]);
        AnimationState animState = static_cast<AnimationState>(message.data[17]);
        
        player->setDir(dir);
        player->setAnimationState(animState);
    }
}

void EmbeddedServer::processEnemyState(
    const std::string& playerId,
    const NetworkMessage& message
) {
    // Validate message data
    if (message.data.empty()) {
        std::cerr << "[EmbeddedServer] Invalid player action data: empty" << std::endl;
        return;
    }

    size_t pos = 4; // Start after action type

    // Read enemy ID length
    int idLength = message.data[pos++];

    // Extract enemy ID
    if (pos >= message.data.size()) {
        std::cerr << "[EmbeddedServer] Invalid enemy ID in state update" << std::endl;
        return;
    }
    
    std::string enemyId(message.data.begin() + pos, message.data.begin() + pos + idLength);
    pos += idLength; // Move position past the ID

    // Read isDead flag (1 byte after the null terminator)
    if (pos + 1 >= message.data.size()) {
        std::cerr << "[EmbeddedServer] Missing isDead flag in enemy state update" << std::endl;
        return;
    }
    bool isDead = message.data[pos++] != 0;
    
    int16_t health;
    memcpy(&health, message.data.data() + pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    std::cout << "[EmbeddedServer] Received enemy state update for " << enemyId 
              << ": isDead=" << isDead << ", health=" << health << std::endl;
    // Get the level and find the enemy
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    Level* level = levelManager_->getCurrentLevel();
    if (!level) {
        std::cerr << "[EmbeddedServer] No active level for enemy state update" << std::endl;
        return;
    }

    std::cout << "[EmbeddedServer] Processing enemy state update for " << enemyId 
              << ": isDead=" << isDead << ", health=" << health << std::endl;
    
    // Find the enemy by ID
    const auto& objects = level->getObjects();
    for (auto& obj : objects) {
        if (obj && obj->type == ObjectType::MINOTAUR && obj->getObjID() == enemyId) {
            Enemy* enemy = static_cast<Enemy*>(obj.get());

            // Update the enemy state
            if (isDead) {
                enemy->setHealth(0);  // Ensure health is set to 0 when dead
                // broadcast enemy death to clients
                sendEnemyStateToClients(enemy->getObjID(), true, 0);
            } else {
                
                // Update health if not dead
                enemy->setHealth(health);
                sendEnemyStateToClients(enemy->getObjID(), true, 0);
            }
            break;
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
    enemyMsg.data.push_back(health >> 8);
    enemyMsg.data.push_back(health & 0xFF);
    
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