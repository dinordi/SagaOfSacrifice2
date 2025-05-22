#include "level_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include "player_manager.h"
#include "game.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

LevelManager::~LevelManager() {
    // Clean up levels
    for (auto& levelPair : levels_) {
        levelPair.second->unload();
    }
    
}
bool LevelManager::initialize() {
    // Set the path to the levels directory
    std::string exec_path = fs::current_path().string();
    std::cout << "[LevelManager] Executable path: " << exec_path << "\n";
    //from this directory, go to SOS/assets/levels
    currentJsonFilePath_ = fs::path(exec_path).parent_path().parent_path() / "SOS" / "assets" / "levels";
    std::cout << "[LevelManager] Current path: " << exec_path << "\n";
    std::cout << "[LevelManager] Initializing from directory: "
              << currentJsonFilePath_ << "\n";

    if (!fs::exists(currentJsonFilePath_) || !fs::is_directory(currentJsonFilePath_)) {
        std::cerr << "[LevelManager] Levels directory not found: "
                  << currentJsonFilePath_ << "\n";
        return false;
    }

    for (auto const& entry : fs::directory_iterator(currentJsonFilePath_)) {
        if (entry.path().extension() != ".json") continue;

        std::ifstream in(entry.path());
        if (!in.is_open()) {
            std::cerr << "[LevelManager] Cannot open "
                      << entry.path() << "\n";
            continue;
        }

        try {
            json j;
            in >> j;
            auto id   = j.at("id").get<std::string>();
            auto name = j.at("name").get<std::string>();

            // register Level object and its file path
            levels_[id]         = std::make_shared<Level>(id, name, collisionManager);
            levelFilePaths_[id] = entry.path();

            std::cout << "[LevelManager] Registered level '"
                      << id << "' -> " << entry.path() << "\n";
        }
        catch (std::exception& e) {
            std::cerr << "[LevelManager] parse error in "
                      << entry.path() << ": " << e.what() << "\n";
        }
    }

    if (levels_.empty()) {
        std::cerr << "[LevelManager] No valid levels found in "
                  << currentJsonFilePath_ << "\n";
        return false;
    }
    else {
        std::cout << "[LevelManager] Found " << levels_.size()
                  << " levels in " << currentJsonFilePath_ << "\n";
    }

    return true;
}



bool LevelManager::loadLevel(const std::string& levelId) {
    auto it = levels_.find(levelId);
    bool loaded = false;
    std::cout << "[LevelManager] Loading level: " << levelId << std::endl;  
    if (it != levels_.end()) {
        currentLevel_ = it->second;
        
        // Load the JSON level data file
        std::string levelFilePath = levelFilePaths_[levelId];  // Use the path we already stored during initialization
        std::ifstream levelFile(levelFilePath);
        
        if (!levelFile.is_open()) {
            std::cerr << "Failed to open level file: " << levelFilePath << std::endl;
            loaded = false;
        }
        std::cout << "[LevelManager] Loading level from file: " << levelFilePath << std::endl;
        try {
            json levelData;
            levelFile >> levelData;
            
            // First load the level data
            bool loadSuccess = currentLevel_->load(levelData);
            if (!loadSuccess) {
                std::cerr << "[LevelManager] Failed to load level data from JSON" << std::endl;
                loaded = false;
            }
            
            // Example: Spawn a minotaur in the level near the player start position plus an offset
            Vec2 playerStart = currentLevel_->getPlayerStartPosition();
            
            // Spawn a minotaur slightly to the right of the player start position
            if (auto minotaur = currentLevel_->spawnMinotaur(playerStart.x + 300, playerStart.y)) {
                std::cout << "[LevelManager] Spawned a minotaur in level " << levelId << std::endl;
            }
            
            // Example: Spawn more minotaurs at different positions if desired
            currentLevel_->spawnMinotaur(playerStart.x + 400, playerStart.y + 100);
            currentLevel_->spawnMinotaur(playerStart.x - 200, playerStart.y + 200);
            
            // Set all enemies in the level to target any existing players
            for (auto& playerPair : PlayerManager::getInstance().getAllPlayers()) {
                currentLevel_->setAllEnemiesToTargetPlayer(playerPair.second);
            }
            
            // Get all existing players from PlayerManager and add them to this level
            auto& playerManager = PlayerManager::getInstance();
            const auto& players = playerManager.getAllPlayers();
            for (const auto& playerPair : players) {
                // Check if the player already exists in the level before adding
                bool playerExists = false;
                const auto& levelObjects = currentLevel_->getObjects();
                
                for (const auto& obj : levelObjects) {
                    if (obj->getObjID() == playerPair.first) {
                        playerExists = true;
                        std::cout << "[LevelManager] Player " << playerPair.first << " already exists in level " << levelId << std::endl;
                        break;
                    }
                }
                
                if (!playerExists) {

                    addPlayerToCurrentLevel(playerPair.first);
                }
            }
            std::cout << "[LevelManager] Loaded level: " << levelId << std::endl;
            return true;
        } catch (json::exception& e) {
            std::cerr << "[LevelManager] JSON error: " << e.what() << std::endl;
            loaded = false;
        } catch (std::exception& e) {
            std::cerr << "[LevelManager] Error loading level: " << e.what() << std::endl;
            loaded = false;
        }
    } else {
        std::cerr << "[LevelManager] Level ID not found: " << levelId << std::endl;
        loaded = false;
    }
    
    return loaded;
}

Level* LevelManager::getCurrentLevel() const {
    return currentLevel_.get();
}

Level* LevelManager::getLevel(const std::string& levelId) {
    auto it = levels_.find(levelId);
    if (it != levels_.end()) {
        return it->second.get();
    }
    return nullptr;
}
bool LevelManager::loadNextLevel() {
    if (currentLevel_) {
        auto it = std::find_if(levels_.begin(), levels_.end(),
            [this](const auto& pair) { return pair.second == currentLevel_; });
        
        if (it != levels_.end()) {
            ++it; // Move to the next level
            if (it != levels_.end()) {
                return loadLevel(it->first);
            }
        }
    }
    return false;
}
void LevelManager::resetCurrentLevel() {
    if (currentLevel_) {
        currentLevel_->reset();
    }
}

bool LevelManager::loadPreviousLevel() {
    if (currentLevel_) {
        auto it = std::find_if(levels_.begin(), levels_.end(),
            [this](const auto& pair) { return pair.second == currentLevel_; });

        if (it != levels_.end() && it != levels_.begin()) {
            // Find the previous element manually (unordered_map does not support --)
            auto prev = levels_.begin();
            for (auto iter = levels_.begin(); iter != it; ++iter) {
                prev = iter;
            }
            if (prev != it) {
                return loadLevel(prev->first);
            }
        }
    }
    return false;
}

// Add a player to the current level
bool LevelManager::addPlayerToCurrentLevel(const std::string& playerId) {
    if (!currentLevel_) {
        std::cerr << "[LevelManager] No current level to add player to" << std::endl;
        return false;
    }
    std::cout << "[LevelManager] Adding player " << playerId << " to current level" << std::endl;
    
    // Get or create the player using PlayerManager
    auto& playerManager = PlayerManager::getInstance();
    auto player = playerManager.getPlayer(playerId);
    
    // Get the level's player start position
    Vec2 startPos = currentLevel_->getPlayerStartPosition();
    
    // If the player doesn't exist yet, create a new one
    if (!player) {
        player = playerManager.createPlayer(playerId, startPos);
    } else {
        // For existing players, update their position to the level's start position
        player->setposition(startPos);
        std::cout << "[LevelManager] Repositioned player " << playerId 
                  << " to level start position: " << startPos.x << "," << startPos.y << std::endl;
    }
    
    // Add the player to the current level
    currentLevel_->addObject(player);
    std::cout << "[LevelManager] Added player " << playerId << " to current level" << std::endl;
    
    return true;
}

// Update the active level
void LevelManager::update(float deltaTime) {
    if (currentLevel_) {
        currentLevel_->update(deltaTime);
    }
}

bool LevelManager::removePlayerFromCurrentLevel(const std::string& playerId) {
    if (currentLevel_) {
        auto& playerManager = PlayerManager::getInstance();
        auto player = playerManager.getPlayer(playerId);
        
        if (player) {
            currentLevel_->removeObject(player);
            std::cout << "[LevelManager] Removed player " << playerId << " from current level" << std::endl;
            return true;
        }
    }
    return false;
}
bool LevelManager::removeAllPlayersFromCurrentLevel() {
    if (currentLevel_) {
        auto& playerManager = PlayerManager::getInstance();
        const auto& players = playerManager.getAllPlayers();
        
        for (const auto& playerPair : players) {
            currentLevel_->removeObject(playerPair.second);
            std::cout << "[LevelManager] Removed player " << playerPair.first << " from current level" << std::endl;
        }
        return true;
    }
    return false;
}

bool LevelManager::removeAllObjectsFromCurrentLevel() {
    if (currentLevel_) {
        currentLevel_->removeAllObjects();
        std::cout << "[LevelManager] Removed all objects from current level" << std::endl;
        return true;
    }
    return false;
}