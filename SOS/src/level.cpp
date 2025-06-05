// ────────────────────────────── Level.cpp ───────────────────────────────
#include "level.h"
#include "AudioManager.h"

#include <iostream>
#include <fstream>
#include <climits>
#include <algorithm>
#include <filesystem>
#include <chrono>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Tiled flip/rotation flags (bits 31–29) */
namespace {
    constexpr uint32_t FLIP_MASK = 0xE0000000u;
}

/* ── ctor / dtor ──────────────────────────────────────────────────────── */
Level::Level(const std::string& id,
             const std::string& name,
             CollisionManager*   collisionManager)
    : id(id),
      name(name),
      loaded(false),
      completed(false),
      collisionManager(collisionManager),
      playerStartPosition(0, 0)
{}

Level::~Level() { unload(); }

/* ───────────────────────────────  load  ─────────────────────────────── */
bool Level::load(json& levelData)
{
    /* --- basic map props ------------------------------------------------ */
    backgroundPath = levelData.value("background", "");
    const int tileWidth  = levelData.value("tilewidth",  32);
    const int tileHeight = levelData.value("tileheight", 32);

    if (levelData.contains("playerStart"))
        playerStartPosition = Vec2(levelData["playerStart"].value("x", 0),
                                   levelData["playerStart"].value("y", 0));

    /* --- build GID → tileset table ------------------------------------- */
    struct Range { int first, last; std::string name; };
    std::vector<Range> gidMap;

    if (levelData.contains("tilesets"))
    {
        for (const auto& ts : levelData["tilesets"])
        {
            Range r;
            r.first = ts.at("firstgid").get<int>();
            r.name  = ts.contains("name")
                      ? ts["name"].get<std::string>()
                      : std::filesystem::path(ts["source"].get<std::string>())
                            .stem().string();
            gidMap.push_back(r);
        }
        std::sort(gidMap.begin(), gidMap.end(),
                  [](auto& a, auto& b){ return a.first < b.first; });
        for (std::size_t i = 0; i < gidMap.size(); ++i)
            gidMap[i].last = (i + 1 < gidMap.size())
                           ? gidMap[i+1].first - 1 : INT_MAX;
    }

    auto gidToTileset =
        [&](int gid, std::string& tsName, int& localId) -> bool
    {
        for (const auto& r : gidMap)
            if (gid >= r.first && gid <= r.last)
            {
                tsName  = r.name;
                localId = gid - r.first;   // 0-based frame
                return true;
            }
        return false;
    };

    /* --- tile layers ---------------------------------------------------- */

    if (levelData.contains("layers"))
    {
        for (const auto& layer : levelData["layers"])
        {
            if (layer.value("type", "") != "tilelayer")
                continue;

            const int width  = layer.at("width");
            const int height = layer.at("height");
            const auto& data = layer.at("data");
            const int layerid = layer.at("id");

            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    const std::size_t index = static_cast<std::size_t>(row) * width + col;
                    const uint32_t rawGid   = data[index];

                    /* skip empty cells and any tile with flip/rotation bits */
                    if (rawGid == 0 || (rawGid & FLIP_MASK))
                        continue;

                    const int gid = static_cast<int>(rawGid);

                    std::string tileset;
                    int spriteIndex = 0;
                    if (!gidToTileset(gid, tileset, spriteIndex))
                        continue;               // orphan GID – skip

                    const int worldX = col * tileWidth;
                    const int worldY = row * tileHeight;

                    uint16_t objId = Object::getNextObjectID();
                    std::cout << "LayerID: " << layerid << std::endl;
                    auto tile = std::make_shared<Tile>(
                        worldX, worldY, objId,
                        tileset, spriteIndex,
                        tileWidth, tileHeight, layerid);
                    std::cout << "Layer after: " << tile->getLayer() << std::endl;
                    levelObjects.push_back(tile);
                    
                }
            }
        }
    }

    /* --- enemies -------------------------------------------------------- */
    if (levelData.contains("enemies"))
    {
        for (const auto& e : levelData["enemies"])
            if (e.value("type","") == "minotaur")
                spawnMinotaur(e.value("x", 0), e.value("y", 0));
    }

    /* --- items (placeholder) ------------------------------------------- */
    if (levelData.contains("items")){
        for (const auto& item : levelData["items"])
            std::cout << "Found item "
                      << item.value("id","") << " of type "
                      << item.value("type","")
                      << " at (" << item.value("x",0)
                      << ", " << item.value("y",0) << ")\n";

    loaded = true;
    
}

/* ---------------- rest of Level methods unchanged --------------------- */


    /* ---- music + SFX ---------------------------------------------------- */
    if (levelData.contains("music"))
    {
        const std::string& musicPath = levelData["music"];
        if (musicPath.empty() || !std::ifstream(musicPath))
        {
            std::cerr << "[Level] Music file not found: "
                      << musicPath << '\n';
            return false;
        }
        std::cout << "Loading music: " << musicPath << '\n';
    }

    if (levelData.contains("soundEffects"))
    {
        for (const std::string& sfx : levelData["soundEffects"])
        {
            if (!std::ifstream(sfx))
            {
                std::cerr << "[Level] SFX file not found: "
                          << sfx << '\n';
                return false;
            }
            std::cout << "Loading SFX: " << sfx << '\n';
        }
    }

    /* ---- success -------------------------------------------------------- */
    loaded = true;
    return true;                                     // explicit!
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
                    std::cout << "[Level] Object with ID: " << object->getObjID() << " is dead, removing from level." << std::endl;
                    objectsToRemove.push_back(object);  // Mark for removal if dead, removing directly here will cause iteration issues
                }
            }
        }

        // Remove dead entities from the level
        for (const auto& obj : objectsToRemove) {
            removeObject(obj);
        }

        auto timeBeforeCollision = std::chrono::steady_clock::now();
        // Detect and resolve collisions
        detectAndResolveCollisions();
        auto timeAfterCollision = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timeAfterCollision - timeBeforeCollision);
        // std::cout << "[Level] Collision detection and resolution took "
        //           << duration.count() << " microseconds\n";
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
    
    uint16_t nextObjId = Object::getNextObjectID();
    // Create a new minotaur at the specified position
    std::shared_ptr<Minotaur> minotaur = std::make_shared<Minotaur>(x, y, nextObjId);
    
    // Add the minotaur to the level objects
    levelObjects.push_back(minotaur);
    
    std::cout << "Spawned Minotaur at position (" << x << ", " << y << ") with ID: " << nextObjId << std::endl;
    
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