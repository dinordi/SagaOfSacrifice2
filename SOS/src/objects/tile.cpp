#include "objects/tile.h"

Tile::Tile(int x, int y, std::string objID, std::string tileMap, int tileIndex, int tileWidth, int tileHeight, int columns) : Object(BoxCollider(x, y, tileWidth, tileHeight), ObjectType::TILE, objID) {
    // Initialize Tile-specific attributes here
    SpriteData* spriteData = new SpriteData(tileMap, tileWidth, tileHeight, columns);
    addSpriteSheet(AnimationState::IDLE, spriteData, 250, true, tileIndex);
    addAnimation(AnimationState::IDLE, tileIndex, 1, columns, 250, true);

}

void Tile::update(float deltaTime) {

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