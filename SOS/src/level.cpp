// ────────────────────────────── Level.cpp (fixed for multiple layers) ───────────────────────────────
#include "level.h"
#include "LevelData_level1.h"    // must now contain:
//   – constexpr uint16_t level1_TILE_WIDTH, level1_TILE_HEIGHT, level1_WIDTH, level1_HEIGHT, level1_SIZE
//   – constexpr size_t   level1_LAYER_COUNT
//   – static constexpr const uint32_t* level1_LAYERS[level1_LAYER_COUNT];
//   – static constexpr const char* level1_LAYER_NAMES[level1_LAYER_COUNT];
//   – constexpr uint32_t level1_PLAYER_START_X, level1_PLAYER_START_Y
//   – struct EnemyData { … }; static const EnemyData level1_ENEMIES[]; constexpr size_t level1_ENEMY_COUNT;
//   – … etc.

#include "AudioManager.h"
#include "objects/minotaur.h"
#include "objects/player.h"

#include <iostream>
#include <fstream>
#include <climits>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string>

using namespace std;

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
{ }

Level::~Level() {
    unload();
}

/* ─────────────────────────────── loadLevel1 ────────────────────────────────
   Now loops over *all* tile‐layers defined in the header.  Requires that
   LevelData_level1.h defines:
     – constexpr size_t level1_LAYER_COUNT;
     – static constexpr const uint32_t* level1_LAYERS[level1_LAYER_COUNT];
     – static constexpr const char* level1_LAYER_NAMES[level1_LAYER_COUNT];
     – constexpr uint16_t level1_TILE_WIDTH, level1_TILE_HEIGHT;
     – constexpr uint16_t level1_WIDTH, level1_HEIGHT;
     – constexpr size_t   level1_SIZE;
     – constexpr uint32_t level1_PLAYER_START_X, level1_PLAYER_START_Y;
     – static const EnemyData level1_ENEMIES[]; constexpr size_t level1_ENEMY_COUNT;
*/
bool Level::load()
{
    // --- basic map props --------------------------------------------------
    const int tileWidth  = static_cast<int>(level1_TILE_WIDTH);
    const int tileHeight = static_cast<int>(level1_TILE_HEIGHT);

    // Player start:
    playerStartPosition = Vec2(
        static_cast<float>(level1_PLAYER_START_X),
        static_cast<float>(level1_PLAYER_START_Y)
    );

    // --- build GID → tileset table ---------------------------------------
    struct Range { uint32_t first, last; std::string name; };
    std::vector<Range> gidMap;
    gidMap.reserve(level1_RANGE_COUNT);

    // level1_RANGES[i].firstGid .. .lastGid  maps to tileset index i
    for (size_t i = 0; i < level1_RANGE_COUNT; ++i)
    {
        Range r;
        r.first = level1_RANGES[i].firstGid;
        r.last  = level1_RANGES[i].lastGid;
        r.name  = level1_TILESET_NAMES[level1_RANGES[i].tilesetIndex];
        gidMap.push_back(r);
    }

    // Sort by starting GID (generator already sorted, but just in case):
    std::sort(gidMap.begin(), gidMap.end(),
              [](auto& a, auto& b){ return a.first < b.first; });

    auto gidToTileset = [&](uint32_t gid, std::string& tsName, int& localId) -> bool
    {
        for (const auto& r : gidMap) {
            if (gid >= r.first && gid <= r.last) {
                tsName   = r.name;
                localId  = static_cast<int>(gid - r.first);  // zero‐based index
                return true;
            }
        }
        return false;
    };

    // --- tile layers (multiple) ------------------------------------------
    const int width  = static_cast<int>(level1_WIDTH);
    const int height = static_cast<int>(level1_HEIGHT);

    // Sanity check: level1_SIZE == width*height ?
    if (level1_SIZE != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        std::cerr << "[Level::load] Unexpected size mismatch: "
                  << "level1_SIZE=" << level1_SIZE
                  << " vs " << (width * height) << "\n";
        return false;
    }

    // For each layer 0 .. level1_LAYER_COUNT-1:
    for (size_t layerIdx = 0; layerIdx < level1_LAYER_COUNT; ++layerIdx)
    {
        const uint32_t* rawLayer = level1_LAYERS[layerIdx];
        // (Optional) if you want a name/ID:
        const char* layerName = level1_LAYER_NAMES[layerIdx];

        for (int row = 0; row < height; ++row)
        {
            for (int col = 0; col < width; ++col)
            {
                size_t index = static_cast<size_t>(row) * static_cast<size_t>(width) + col;
                uint32_t rawGid = rawLayer[index];

                // Skip empty cells or flipped tiles
                if (rawGid == 0 || (rawGid & FLIP_MASK)) {
                    continue;
                }

                std::string tilesetName;
                int spriteIndex = 0;
                if (!gidToTileset(rawGid, tilesetName, spriteIndex)) {
                    // GID not found in any range → ignore
                    continue;
                }

                int worldX = col * tileWidth;
                int worldY = row * tileHeight;

                uint16_t objId = Object::getNextObjectID();
                auto tile = std::make_shared<Tile>(
                    worldX, worldY, objId,
                    tilesetName, spriteIndex,
                    tileWidth, tileHeight,  /*layer=*/ static_cast<int>(layerIdx)
                );

                std::cout << "[Level::load] Layer " << layerIdx
                          << " → Adding tile at (" << worldX << ", " << worldY
                          << ") with GID: " << rawGid
                          << " (layer: " << layerIdx
                          << " (tileset: " << tilesetName
                          << ", local ID: " << spriteIndex
                          << ", object ID: " << objId << ")\n";

                levelObjects.push_back(tile);
            }
        }
    }

    // --- enemies ---------------------------------------------------------
    for (size_t i = 0; i < level1_ENEMY_COUNT; ++i)
    {
        const auto& e = level1_ENEMIES[i];
        if (std::string(e.type) == "minotaur") {
            spawnMinotaur(static_cast<int>(e.x), static_cast<int>(e.y));
        }
        else {
            std::cout << "[Level::load] Skipping unsupported enemy type: "
                      << e.type << "\n";
        }
    }

    // (Items, triggers, objectives, music/sfx are not in the header yet—
    //  hard-code or extend your header + this code if needed.)

    loaded = true;
    return true;
}

/* ---------------- rest of Level methods unchanged --------------------- */

bool Level::isCollidableTile(int tileIndex, const std::string& tileset) {
    // Example collision rules:
    if (tileset == "NeonFloor" && (tileIndex == 1 || tileIndex == 2)) {
        return true;
    }
    if (tileset == "AcidPools") {
        return false;
    }
    return false;
}

void Level::update(float deltaTime) {
    std::vector<std::shared_ptr<Object>> objectsToRemove;

    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);

        for (auto& object : levelObjects) {
            object->update(deltaTime);
            if (object->type == ObjectType::PLAYER ||
                object->type == ObjectType::MINOTAUR) {
                auto entity = std::static_pointer_cast<Entity>(object);
                if (entity->isDead()) {
                    std::cout << "[Level] Object with ID: "
                              << object->getObjID()
                              << " is dead, removing from level.\n";
                    objectsToRemove.push_back(object);
                }
            }
        }

        for (const auto& obj : objectsToRemove) {
            removeObject(obj);
        }

        auto t0 = std::chrono::steady_clock::now();
        detectAndResolveCollisions();
        auto t1 = std::chrono::steady_clock::now();
        // auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
        // std::cout << "[Level] Collision pass took " << dur.count() << " µs\n";
    }

    static uint64_t updateTimer = 0;
    updateTimer += static_cast<uint64_t>(deltaTime);

    // audioManager->update();  // if you’re using AudioManager
}

void Level::detectAndResolveCollisions() {
    collisionManager->detectCollisions(levelObjects);
}

void Level::addObject(std::shared_ptr<Object> object) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    if (object) {
        for (const auto& existing : levelObjects) {
            if (existing->getObjID() == object->getObjID()) {
                std::cout << "[Level] Object with ID "
                          << object->getObjID()
                          << " already exists in level\n";
                return;
            }
        }
        levelObjects.push_back(object);
        std::cout << "[Level] Added object with ID: "
                  << object->getObjID() << "\n";
    } else {
        std::cerr << "[Level] Attempted to add null object\n";
    }
}

void Level::reset() {
    for (auto& object : levelObjects) {
        object->setposition(playerStartPosition);
        object->setAnimationState(AnimationState::IDLE);
    }
    completed = false;
}

void Level::unload() {
    levelObjects.clear();
    loaded = false;
}

void Level::removeObject(std::shared_ptr<Object> object) {
    auto it = std::remove(levelObjects.begin(), levelObjects.end(), object);
    if (it != levelObjects.end()) {
        levelObjects.erase(it, levelObjects.end());
    } else {
        std::cerr << "[Level] Object with ID: "
                  << object->getObjID() << " not found\n";
    }
}

bool Level::removeAllObjects() {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    levelObjects.clear();
    std::cout << "[Level] Cleared all objects from level\n";
    return true;
}

std::shared_ptr<Minotaur> Level::spawnMinotaur(int x, int y) {
    uint16_t nextObjId = Object::getNextObjectID();
    auto minotaur = std::make_shared<Minotaur>(x, y, nextObjId);
    levelObjects.push_back(std::static_pointer_cast<Object>(minotaur));
    std::cout << "Spawned Minotaur at (" << x << ", " << y
              << ") with ID: " << nextObjId << "\n";
    return minotaur;
}

void Level::setAllEnemiesToTargetPlayer(std::shared_ptr<Player> player) {
    if (!player) {
        std::cerr << "[Level] Cannot set null player as target\n";
        return;
    }
    for (auto& object : levelObjects) {
        if (object->type == ObjectType::MINOTAUR) {
            auto enemy = dynamic_cast<Minotaur*>(object.get());
            if (enemy) {
                enemy->setTargetPlayer(player);
                std::cout << "[Level] Set player as target for enemy: "
                          << object->getObjID() << "\n";
            }
        }
    }
}
