#include "player_manager.h"


PlayerManager& PlayerManager::getInstance() {
    static PlayerManager instance;
    return instance;
}

std::shared_ptr<Player> PlayerManager::createPlayer(const std::string& playerId, const Vec2& position) {
    std::lock_guard<std::mutex> lock(playerMutex_);
    
    // Check if player already exists
    if (players_.find(playerId) != players_.end()) {
        std::cerr << "[PlayerManager] Player with ID " << playerId << " already exists" << std::endl;
        return nullptr;
    }
    
    // Create a new player
    auto player = PlayerFactory::createPlayer(playerId, position);
    
    // Add to the map
    players_[playerId] = player;
    std::cout << "[PlayerManager] Created player with ID: " << playerId << std::endl;
    return player;
}

std::shared_ptr<Player> PlayerManager::getPlayer(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(playerMutex_);
    
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        return it->second;
    }
    
    std::cerr << "[PlayerManager] Player with ID " << playerId << " not found" << std::endl;
    return nullptr;
}


const std::unordered_map<std::string, std::shared_ptr<Player>>& PlayerManager::getAllPlayers() const {
    return players_;
}

bool PlayerManager::removePlayer(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(playerMutex_);
    
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        players_.erase(it);
        std::cout << "[PlayerManager] Removed player with ID: " << playerId << std::endl;
        return true;
    }
    
    std::cerr << "[PlayerManager] Player with ID " << playerId << " not found" << std::endl;
    return false;
}

void PlayerManager::clear() {
    std::lock_guard<std::mutex> lock(playerMutex_);
    
    players_.clear();
    std::cout << "[PlayerManager] Cleared all players" << std::endl;
    
}
