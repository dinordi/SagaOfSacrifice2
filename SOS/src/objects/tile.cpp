#include "objects/tile.h"
#include <filesystem>
#include <iostream>

Tile::Tile(int x, int y, uint16_t objID, std::string tileMap, int tileIndex, int tileWidth, int tileHeight, int columns) :   
        Object(BoxCollider(x, y, tileWidth, tileHeight), 
        ObjectType::TILE, objID), 
        tileIndex(tileIndex), 
        tileMapName(tileMap) 
{
    // Initialize Tile-specific attributes here
}

void Tile::setupAnimations(std::filesystem::path atlasPath)
{
    std::string fileName = tileMapName + ".tpsheet";
    atlasPath /= fileName;
    addSpriteSheet(AnimationState::IDLE, atlasPath);
    addAnimation(AnimationState::IDLE, 1, 0, true); // Idle animation (1 frame)
}


void Tile::update(float deltaTime) {

}

bool Tile::isCollidable() const {
    if(hasFlag(Tile::BLOCKS_HORIZONTAL_LEFT) || hasFlag(Tile::BLOCKS_VERTICAL_TOP) ||
       hasFlag(Tile::BLOCKS_HORIZONTAL_RIGHT) || hasFlag(Tile::BLOCKS_VERTICAL_BOTTOM)) {
        return true; // Tile is collidable if it blocks horizontal or vertical movement
    }
    return false; // Default to not collidable
}

void Tile::accept(CollisionVisitor& visitor) {
    visitor.visit(this);
}