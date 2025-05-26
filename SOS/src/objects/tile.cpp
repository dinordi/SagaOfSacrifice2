#include "objects/tile.h"
#include <string>
#include <iostream>

Tile::Tile(int x, int y, std::string objID, std::string tileMap, int tileIndex, int tileWidth, int tileHeight, int columns) : Object(BoxCollider(x, y, tileWidth, tileHeight), ObjectType::TILE, (objID)), tileIndex(tileIndex) {
    // Initialize Tile-specific attributes here
    std::cout << "Creating Tile at (" << x << ", " << y << ") with ID: " << objID << std::endl;
    SpriteData* spriteData = new SpriteData(tileMap, tileWidth, tileHeight, columns);
     addSpriteSheet(AnimationState::IDLE, spriteData, 250, false, tileIndex);
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