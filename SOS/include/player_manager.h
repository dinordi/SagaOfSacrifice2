#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "objects/player.h"
#include "factories/player_factory.h"
#include "level.h"
#include <mutex>
#include <iostream>
/**
 * PlayerManager - Singleton class for managing players across the game
 * 
 * This class centralizes player management so they can be accessed
 * from both Level objects and the EmbeddedServer. It ensures that 
 * players are created consistently and can be tracked across the game.
 */
class PlayerManager {
public:
    /**
     * Get the singleton instance
     */
    static PlayerManager& getInstance();

    std::shared_ptr<Player> createPlayer(const std::string& playerId, const Vec2& position = Vec2(500, 100));

    bool addPlayerToLevel(const std::string& playerId, Level* level);
    
    std::shared_ptr<Player> getPlayer(const std::string& playerId);

    const std::unordered_map<std::string, std::shared_ptr<Player>>& getAllPlayers() const;
    
    bool removePlayer(const std::string& playerId);


    void clear();


private:
    // Private constructor for singleton
    PlayerManager() {}
    
    // Prevent copying
    PlayerManager(const PlayerManager&) = delete;
    PlayerManager& operator=(const PlayerManager&) = delete;
    
    // Player storage
    std::unordered_map<std::string, std::shared_ptr<Player>> players_;
    std::mutex playerMutex_;
};
