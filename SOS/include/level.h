#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "object.h"
#include "collision/CollisionManager.h"
#include "objects/tile.h"
#include "objects/enemy.h"
#include "objects/minotaur.h"
#include "objects/player.h"
#include "factories/player_factory.h"

#include <nlohmann/json.hpp>

class Level
{
public:
    Level(const std::string& id,
          const std::string& name,
          CollisionManager* collisionManager);
    ~Level();

    /* -------- life-cycle -------- */
    bool load();   // read JSON, build level
    void unload();                          // free everything
    void reset();                           // start over
    void update(float deltaTime);           // per-frame

    /* -------- getters ---------- */
    std::string                          getId()       const { return id;   }
    std::string                          getName()     const { return name; }
    const std::vector<std::shared_ptr<Object>>& getObjects()  const { return levelObjects; }
    const std::shared_ptr<Object> getObject(const uint16_t objId) const; 
    std::string                          getBackgroundPath() const { return backgroundPath; }
    Vec2                                 getPlayerStartPosition() const { return playerStartPosition; }

    /* -------- object management -------- */
    void addObject   (std::shared_ptr<Object> object);
    void removeObject(std::shared_ptr<Object> object);
    bool removeAllObjects();

    /* -------- level status -------- */
    bool isCompleted() const { return completed; }
    void setCompleted(bool v){ completed = v;    }

    /* -------- tile helpers -------- */
    bool isCollidableTile(int localTileId,
                          const std::string& tilesetName);

    /* -------- enemies / AI -------- */
    std::shared_ptr<Minotaur> spawnMinotaur(int x, int y);
    void setAllEnemiesToTargetPlayer(std::shared_ptr<Player> player);

    /* -------- players ------------- */
    std::shared_ptr<Player> createAndAddPlayer(const std::string& playerId);
    std::shared_ptr<Player> getPlayer(const std::string& playerId);

private:
    /* ---------- tileset table ---------- */
    struct TilesetInfo {
        std::string name;
        uint32_t    firstgid;
        uint32_t    tilecount;
    };

    /* ---------- JSON helpers ----------- */
    void processLayer(const nlohmann::json& layer,
                      int tileW, int tileH,
                      const std::vector<TilesetInfo>& tilesets,
                      const std::string& parentId = "");

    /* ---------- sub-loaders ------------ */
    void loadPlayers  (const nlohmann::json& levelData);
    void loadEnemies  (const nlohmann::json& levelData);
    void loadPlatforms(const nlohmann::json& levelData);

    /* ---------- collision -------------- */
    void detectAndResolveCollisions();

private:
    /* ---------- core data -------------- */
    std::string id;                 // “level1”
    std::string name;               // display name
    std::string backgroundPath;

    Vec2 playerStartPosition{0,0};

    bool loaded   = false;
    bool completed= false;

    std::vector<std::shared_ptr<Object>> levelObjects;
    std::vector<TilesetInfo>             tilesets_;

    /* map-wide tile metrics */
    int tileWidth  = 32;
    int tileHeight = 32;

    CollisionManager* collisionManager = nullptr;
    mutable std::mutex gameStateMutex_;
};
