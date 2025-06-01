#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "object.h"
#include "collision/CollisionManager.h"
#include "objects/tile.h"
#include "objects/enemy.h"
#include "objects/minotaur.h"
#include "objects/player.h"
#include "factories/player_factory.h"
#include "nlohmann/json.hpp" // Include the JSON library
#include <mutex>
class Level {
    public:
        Level(const std::string& id, const std::string& name, CollisionManager* collisionManager);
        ~Level();

        // Initialize and load all level content
        bool load(nlohmann::json& levelData);  // Updated to include json parameter
        
        // Unload level resources
        void unload();
        
        // Update level-specific logic
        void update(float deltaTime);
        
        // Getters
        std::string getId() const { return id; }
        std::string getName() const { return name; }
        const std::vector<std::shared_ptr<Object>>& getObjects() const { return levelObjects; }
        std::string getBackgroundPath() const { return backgroundPath; }
        
        // Get player starting position in this level
        Vec2 getPlayerStartPosition() const { return playerStartPosition; }
        
        // Add an object to the level
        void addObject(std::shared_ptr<Object> object);
        void removeObject(std::shared_ptr<Object> object);
        
        // Check if level is completed
        bool isCompleted() const { return completed; }
        void setCompleted(bool value) { completed = value; }
        
        bool removeAllObjects();
        // Reset the level to its initial state
        void reset();
        
        // Add methods for tiles
        bool isCollidableTile(int tileIndex, const std::string& tileset);
        //std::shared_ptr<TileLayer> getTileLayer(const std::string& layerId);
        
        // Function to spawn a Minotaur enemy
        std::shared_ptr<Minotaur> spawnMinotaur(int x, int y);
        
        // Function to set all enemies to target a specific player
        void setAllEnemiesToTargetPlayer(std::shared_ptr<Player> player);
        
    public:
        /**
         * Create and add a player to the level at the player start position
         * 
         * @param playerId Unique identifier for the player
         * @return Shared pointer to the created player
         */
        std::shared_ptr<Player> createAndAddPlayer(const std::string& playerId);
        
        /**
         * Get a player object by ID if it exists in this level
         * 
         * @param playerId The player's unique identifier
         * @return Shared pointer to the player or nullptr if not found
         */
        std::shared_ptr<Player> getPlayer(const std::string& playerId);
        
    private:
        // Helper methods for loading various level components
        void loadPlatforms();
        void loadEnemies();
        void loadPlayers(nlohmann::json& levelData);
        void detectAndResolveCollisions();
        //void loadCollectibles();
        
        std::string id;                // Unique level identifier (e.g., "level1")
        std::string name;              // Display name (e.g., "Forest of Despair")
        std::string backgroundPath;    // Path to the background image
        Vec2 playerStartPosition;      // Starting position for the player
        
        bool loaded;                   // Has the level been loaded
        bool completed;                // Has the level been completed

        // Missing member variables
        std::vector<std::shared_ptr<Object>> levelObjects; // Collection of all level objects
        CollisionManager* collisionManager;               // Pointer to collision manager

        // Tile-related members
        int tileWidth;
        int tileHeight;
        //std::map<std::string, std::shared_ptr<TileLayer>> tileLayers;
    private:
        std::mutex gameStateMutex_;

};