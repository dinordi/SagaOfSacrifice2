// filepath: /Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/include/sprite_data.h
#pragma once



class SpriteRect {
public:
    int x; // X position of the sprite in the sprite sheet
    int y; // Y position of the sprite in the sprite sheet
    int w; // Width of the sprite
    int h; // Height of the sprite

    int spriteID; // ID of the sprite, i.e. 1.png, 2.png, etc.
    SpriteRect(int x, int y, int w, int h, int spriteID) : x(x), y(y), w(w), h(h), spriteID(spriteID) {}
};

class SpriteData {
public:
    // Constructor
    SpriteData(int ID, int width, int height, int columns);

    const SpriteRect& getSpriteRect(int index) const;

    int ID; //Name of sprite i.e. 1.png, 2.png, etc.
    int width;  // Width of the sprite, consistent in a single sprite sheet
    int height; // Height of the sprite, consistent in a single sprite sheet
    
    int columns; // Number of columns in the sprite sheet
    private:

};