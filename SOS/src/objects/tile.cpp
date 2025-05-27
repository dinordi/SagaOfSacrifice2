#include "objects/tile.h"
#include <filesystem>

Tile::Tile(int x, int y, std::string objID, std::string tileMap, int tileIndex, int tileWidth, int tileHeight, int columns) : Object(BoxCollider(x, y, tileWidth, tileHeight), ObjectType::TILE, std::to_string(tileIndex)) {
    // Initialize Tile-specific attributes here
    std::filesystem::path base = std::filesystem::current_path();
    std::string temp = base.string();
    std::size_t pos = temp.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        temp = temp.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = std::filesystem::path(temp);
    basePath /= "SOS/assets/spriteatlas";

    addSpriteSheet(AnimationState::IDLE, basePath / "tilemap_flat.tpsheet");
    // addAnimation(AnimationState::IDLE, tileIndex, 1, columns, 250, true);

}


void Tile::update(float deltaTime) {

}

bool Tile::isCollidable() const {
    if(hasFlag(Tile::BLOCKS_HORIZONTAL) || hasFlag(Tile::BLOCKS_VERTICAL)) {
        return true; // Tile is collidable if it blocks horizontal or vertical movement
    }
    return false; // Default to not collidable
}

void Tile::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}

void Tile::setTileType(TileType type) {
    tileType = type;
}

TileType Tile::getTileType() const {
    return tileType;
}