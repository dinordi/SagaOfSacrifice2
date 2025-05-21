#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <fstream>
#include "level.h"
#include "collision/CollisionManager.h"

class Game;

class LevelManager {
public:
    LevelManager(Game* game, CollisionManager* collisionManager) : game(game), collisionManager(collisionManager){};
    ~LevelManager();

    // Initialize the level manager and load all level metadata
    bool initialize();
    
    // Load and activate a specific level
    bool loadLevel(const std::string& levelId);
    
    // Get the currently active level
    Level* getCurrentLevel() const;
    
    // Get a specific level by ID
    Level* getLevel(const std::string& levelId);
    
    // Move to the next level in sequence
    bool loadNextLevel();

    // Move to the previous level in sequence
    bool loadPreviousLevel();
    
    // Reset the current level
    void resetCurrentLevel();
    
    // Update the currently active level
    void update(uint64_t deltaTime);
    
    // Check if all levels have been completed
    bool areAllLevelsCompleted() const;
    
    // Get list of all level IDs
    std::vector<std::string> getAllLevelIds() const;
    
    // Add a player to the current level
    bool addPlayerToCurrentLevel(const std::string& playerId);
    bool removePlayerFromCurrentLevel(const std::string& playerId);
    bool removeAllPlayersFromCurrentLevel();
    bool removeAllObjectsFromCurrentLevel();

private:
    Game* game;
    CollisionManager* collisionManager;
private:
    std::unordered_map<std::string, std::shared_ptr<Level>> levels_;
    std::shared_ptr<Level> currentLevel_;
    std::string currentJsonFilePath_;
    std::unordered_map<std::string, std::filesystem::path> levelFilePaths_;


};