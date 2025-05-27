#include "objects/tile.h"
#include <filesystem>
#include <iostream>

Tile::Tile(int x, int y, std::string objID, std::string tileMap, int tileIndex, int tileWidth, int tileHeight, int columns) : Object(BoxCollider(x, y, tileWidth, tileHeight), ObjectType::TILE, (objID)), tileIndex(tileIndex) {
    // Initialize Tile-specific attributes here
}

void Tile::setupAnimations(std::filesystem::path atlasPath)
{
    addSpriteSheet(AnimationState::IDLE, atlasPath / "Tilemap_Flat.tpsheet");
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