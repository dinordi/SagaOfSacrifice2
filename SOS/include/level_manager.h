#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "level.h"
#include "collision/CollisionManager.h"
#include "game.h"

class LevelManager {
public:
    LevelManager(Game* game, CollisionManager* collisionManager);
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
    
    // Reset the current level
    void resetCurrentLevel();
    
    // Update the currently active level
    void update(uint64_t deltaTime);
    
    // Check if all levels have been completed
    bool areAllLevelsCompleted() const;
    
    // Get list of all level IDs
    std::vector<std::string> getAllLevelIds() const;


};