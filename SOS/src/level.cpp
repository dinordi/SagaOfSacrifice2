#include "level.h"
#include "AudioManager.h" // Include the AudioManager header
#include <iostream>
#include <nlohmann/json.hpp> // Add proper json include
#include <fstream>
using json = nlohmann::json;
 
Level::Level(const std::string& id, const std::string& name, CollisionManager* collisionManager)
    : id(id), name(name), loaded(false), completed(false), collisionManager(collisionManager) {
    // Initialize player start position to a default value
    playerStartPosition = Vec2(0, 0);
}
Level::~Level() {
    // Clean up level objects
    unload();
    
}
bool Level::load(json& levelData ) {
    // Load level data from JSON
    if (levelData.contains("background")) {
        backgroundPath = levelData["background"];

    }
    
    if (levelData.contains("playerStart")) {
        playerStartPosition.x = levelData["playerStart"]["x"];
        playerStartPosition.y = levelData["playerStart"]["y"];
        std::cout << "Player start position: (" << playerStartPosition.x << ", " << playerStartPosition.y << ")" << std::endl;

    }
    else {
        std::cerr << "Player start position not found in level data" << std::endl;
    }
// Load layers from the layers array
if(levelData.contains("layers")) {
    std::cout << "Loading " << levelData["layers"].size() << " layers..." << std::endl;
    
    // Get tile dimensions from level data
    int tileWidth = levelData.contains("tileWidth") ? levelData["tileWidth"].get<int>() : 32;
    int tileHeight = levelData.contains("tileHeight") ? levelData["tileHeight"].get<int>() : 32;
    
    
    for (const auto& layer : levelData["layers"]) {
        std::string layerId = layer["id"];
        std::string layerType = layer["type"];
        
        
        std::cout << "Loading layer: " << layerId << " (type: " << layerType << ")" << std::endl;
        
        if (layerType == "tilemap" && layer.contains("data")) {
            const auto& tileData = layer["data"];
            std::string tileset = layer["tileset"];
            // Process each row
            for (int row = 0; row < tileData.size(); row++) {
                const auto& rowData = tileData[row];
                
                // Process each column in the row
                for (int col = 0; col < rowData.size(); col++) {
                    int tileId = rowData[col];
                    
                    // Skip empty tiles (tile ID 0)
                    if (tileId == 0) continue;
                    
                    // Calculate world position
                    int worldX = col * tileWidth;
                    int worldY = row * tileHeight;
                    
                    // Create tile object
                    std::string tileObjectId = layerId + "_" + std::to_string(row) + "_" + std::to_string(col);
                    auto tile = std::make_shared<Tile>(worldX, worldY, tileObjectId, 
                                                      tileset, tileId, tileWidth, tileHeight, 0);
                    
                    // // Set collision flags based on layer and tile type
                    // if (layerId == "floor") {
                    //     // Floor tiles - walkable but visible
                    //     tile->setFlag(Tile::WALKABLE);
                    // }
                    // else if (layerId == "pools") {
                    //     // Pool tiles - might slow player down or cause damage
                    //     tile->setFlag(Tile::WALKABLE | Tile::HAZARD);
                    // }
                    // else if (layerId == "path") {
                    //     // Path tiles - easy movement
                    //     tile->setFlag(Tile::WALKABLE);
                    // }
               
                        // Ruin tiles - might block movement
                        tile->setFlag(Tile::BLOCKS_HORIZONTAL | Tile::BLOCKS_VERTICAL);
                        
                    
                    // Add to level objects
                    levelObjects.push_back(tile);
                    
                    std::cout << "Created tile: " << tileObjectId << " at (" << worldX << ", " << worldY 
                              << ") with tileId " << tileId << std::endl;
                }
            }
        }
    }
    
    std::cout << "Loaded " << levelObjects.size() << " total tile objects." << std::endl;
} else {
    std::cerr << "No layers found in level data" << std::endl;
}
    // Load objects from the objects array
    // if(levelData.contains("objects")) {
    //     for (const auto& obj : levelData["objects"]) {
    //         // Extract values from JSON
    //         int x = obj["x"];
    //         int y = obj["y"];
    //         std::string objID = obj["id"];
            
    //         // Create platform object
    //         auto object = std::make_shared<Tile>(x, y, objID, 
    //             "Tilemap_Flat", 0, 64, 64, 5);
    //         object->setFlag(Tile::BLOCKS_HORIZONTAL | Tile::BLOCKS_VERTICAL);
    //         levelObjects.push_back(object);
    //     }
    // }
    
    // Load enemies separately
    if(levelData.contains("enemies")) {
        for (const auto& obj : levelData["enemies"]) {
            // Extract common enemy values
            int x = obj["x"];
            int y = obj["y"];
            std::string objID = obj["id"];
            std::string type = obj["type"];
            int hp = obj["hp"];
            int speed = obj["speed"];
            
            // Create different enemy types based on the type field
            if (type == "android_scout") {
                // Example: Create a basic enemy
                // auto enemy = std::make_shared<AndroidScout>(x, y, 
                //     new SpriteData("enemies", spriteId, 32, 32), objID, hp, speed);
                // levelObjects.push_back(enemy);
                std::cout << "Found android_scout enemy at position (" << x << ", " << y << "), HP: " << hp << std::endl;
            } else if (type == "drone_swarm") {
                // Example for another enemy type
                std::cout << "Found drone_swarm enemy at position (" << x << ", " << y << "), HP: " << hp << std::endl;
            } else if (type == "cyber_hound") {
                std::cout << "Found cyber_hound enemy at position (" << x << ", " << y << "), HP: " << hp << std::endl;
            } else {
                // Generic enemy handling
                std::cout << "Found enemy of type " << type << " at position (" << x << ", " << y << "), HP: " << hp << std::endl;
            }
        }
    }
    
    // Load items
    if(levelData.contains("items")) {
        for (const auto& item : levelData["items"]) {
            int x = item["x"];
            int y = item["y"];
            int spriteId = item["spriteId"];
            std::string itemID = item["id"];
            std::string type = item["type"];
            
            std::cout << "Found item " << itemID << " of type " << type << " at position (" << x << ", " << y << ")" << std::endl;
            
            // Create different item types based on the type field
            // Example:
            // if (type == "currency") {
            //     int value = item["value"];
            //     auto collectible = std::make_shared<CurrencyItem>(x, y, 
            //         new SpriteData("items", spriteId, 32, 32), itemID, value);
            //     levelObjects.push_back(collectible);
            // }
        }
    }
    
    if(levelData.contains("music")) {
        std::string musicPath = levelData["music"];
        //check if the music path is valid
        if (musicPath.empty()) {
            std::cerr << "Music path is empty" << std::endl;
            return false;
        }
        // Check if the music file exists
        std::ifstream musicFile(musicPath);
        if (!musicFile) {
            std::cerr << "Music file not found: " << musicPath << std::endl;
            return false;
        }
        musicFile.close();
        // Load music file
        std::cout << "Loading music: " << musicPath << std::endl;
    }
    
    if(levelData.contains("soundEffects")) {
        for (const auto& sound : levelData["soundEffects"]) {
            std::string soundPath = sound;
            // Load all sound effects in the class with audio instance
            // Check if the sound file exists
            std::ifstream soundFile(soundPath);
            if (!soundFile) {
                std::cerr << "Sound effect file not found: " << soundPath << std::endl;
                return false;
            }
            soundFile.close();
            
            std::cout << "Loading sound effect: " << soundPath << std::endl;
        }
    }
    
    // Load tilemap layers
    if (levelData.contains("layers") && levelData.contains("tileWidth") && levelData.contains("tileHeight")) {
        int tileWidth = levelData["tileWidth"];
        int tileHeight = levelData["tileHeight"];
        
        // Store tile dimensions
        this->tileWidth = tileWidth;
        this->tileHeight = tileHeight;
        
        // Process each layer
        for (const auto& layer : levelData["layers"]) {
            std::string layerId = layer["id"];
            std::string layerType = layer["type"];
            std::string tileset = layer["tileset"];
            
            std::cout << "Loading tilemap layer: " << layerId << " using tileset: " << tileset << std::endl;
            
            if (layerType == "tilemap") {
                // Get the 2D array of tile indices
                auto& tileData = layer["data"];
                
                // Create a new TileLayer object (you'll need to implement this class)
                // auto tileLayer = std::make_shared<TileLayer>(layerId, tileset, tileWidth, tileHeight);
                
                // Iterate through the 2D array
                int rowIndex = 0;
                for (const auto& row : tileData) {
                    int colIndex = 0;
                    for (const auto& tileIndex : row) {
                        // if (tileIndex != 0) {  // 0 usually means empty tile
                        //     // Calculate world position based on tile indices
                        //     int x = colIndex * tileWidth;
                        //     int y = rowIndex * tileHeight;
                            
                        //     // Create a tile object
                        //     // Here we're assuming you have a Tile class that extends Object
                        //     auto tile = std::make_shared<Tile>(
                        //         x, y,
                        //         new SpriteData(tileset, tileIndex, tileWidth, tileHeight),
                        //         layerId + "_" + std::to_string(rowIndex) + "_" + std::to_string(colIndex)
                        //     );
                            
                        //     // Add tile to the layer
                        //     tileLayer->addTile(tile);
                            
                        //     If the tile is collidable, add it to level objects for collision detection
                        //     if (isCollidableTile(tileIndex, tileset)) {
                        //         //levelObjects.push_back(tile);
                        //     }
                        // }
                        colIndex++;
                    }
                    rowIndex++;
                }
                
                // Store the tile layer
                //tileLayers[layerId] = tileLayer;
            }
        }
    }
    
    loaded = true;
    return true;
}

bool Level::isCollidableTile(int tileIndex, const std::string& tileset) {
    // Define which tiles should have collision
    // This could be enhanced to load from a configuration file
    
    // For example: in "NeonFloor" tileset, indices 1 and 2 are solid walls
    if (tileset == "NeonFloor" && (tileIndex == 1 || tileIndex == 2)) {
        return true;
    }
    
    // In "AcidPools" tileset, all tiles might cause damage but aren't solid
    if (tileset == "AcidPools") {
        return false;  // Not solid, but might have other effects
    }
    
    // Default: no collision
    return false;
}

void Level::update(float deltaTime) {
    std::vector<std::shared_ptr<Object>> objectsToRemove;

    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
    
        // Update all game objects
        for (auto& object : levelObjects) {
            object->update(deltaTime);
            // Check if object is an entity that has health
            if (object->type == ObjectType::PLAYER || object->type == ObjectType::MINOTAUR) {
                std::shared_ptr<Entity> entity = std::static_pointer_cast<Entity>(object);
                if(entity->isDead())
                {
                    objectsToRemove.push_back(object);  // Mark for removal if dead, removing directly here will cause iteration issues
                }
            }
        }

        // Remove dead entities from the level
        for (const auto& obj : objectsToRemove) {
            removeObject(obj);
        }
        
        // Detect and resolve collisions
        detectAndResolveCollisions();
    }
    
    // Send game state to clients periodically
    static uint64_t updateTimer = 0;
    updateTimer += deltaTime;
    
    // Update audio if needed
    //audioManager->update();
}

void Level::detectAndResolveCollisions() {
    // Detect and resolve collisions using the collision manager
    collisionManager->detectCollisions(levelObjects);
}   

void Level::addObject(std::shared_ptr<Object> object) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    if (object) {
        // Check if the object is already in the level
        for (const auto& existingObj : levelObjects) {
            if (existingObj->getObjID() == object->getObjID()) {
                std::cout << "[Level] Object with ID " << object->getObjID() << " already exists in level" << std::endl;
                return; // Skip duplicate objects
            }
        }
        
        levelObjects.push_back(object);
        std::cout << "[Level] Added object with ID: " << object->getObjID() << std::endl;
    } else {
        std::cerr << "[Level] Attempted to add null object to level" << std::endl;
    }
}

void Level::reset() {
    // Reset level state
    for (auto& object : levelObjects) {
        object->setposition(playerStartPosition); // Reset position to start
        object->setAnimationState(AnimationState::IDLE); // Reset animation state
    }
    //reset audio
    completed = false;
}

void Level::unload() {
    // Unload level resources
    levelObjects.clear();
    loaded = false;
    //unload all audio
}

void Level::removeObject(std::shared_ptr<Object> object) {
    auto it = std::remove(levelObjects.begin(), levelObjects.end(), object);
    if (it != levelObjects.end()) {
        levelObjects.erase(it, levelObjects.end());
        // std::cout << "[Level] Removed object with ID: " << object->getObjID() << std::endl;
    } else {
        std::cerr << "[Level] Object with ID: " << object->getObjID() << " not found in level" << std::endl;
    }
}

bool Level::removeAllObjects() {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    levelObjects.clear();
    std::cout << "[Level] Cleared all objects from level" << std::endl;
    return true;
}

std::shared_ptr<Minotaur> Level::spawnMinotaur(int x, int y) {
    // Generate a unique ID for the minotaur
    static int minotaurCounter = 0;
    std::string minotaurId = "minotaur_" + std::to_string(minotaurCounter++);
    
    // Create a new minotaur at the specified position
    std::shared_ptr<Minotaur> minotaur = std::make_shared<Minotaur>(x, y, minotaurId);
    
    // Add the minotaur to the level objects
    levelObjects.push_back(minotaur);
    
    std::cout << "Spawned Minotaur at position (" << x << ", " << y << ") with ID: " << minotaurId << std::endl;
    
    return minotaur;
}

void Level::setAllEnemiesToTargetPlayer(std::shared_ptr<Player> player) {
    if (!player) {
        std::cerr << "[Level] Cannot set null player as target for enemies" << std::endl;
        return;
    }
    
    // Find all enemies in the level and set the player as their target
    for (auto& object : levelObjects) {
        if(object->type == ObjectType::MINOTAUR)
        {
            Minotaur* enemy = static_cast<Minotaur*>(object.get());
            
            if (enemy) {
                enemy->setTargetPlayer(player);
                std::cout << "[Level] Set player as target for enemy: " << object->getObjID() << std::endl;
            }
        }
        // Use dynamic_cast to check if this object is an Enemy
    }
}