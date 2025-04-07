// filepath: /Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/include/sprite_data.h
#pragma once

class SpriteData {
public:
    int ID;
    int width;
    int height;

    // Constructor
    SpriteData(int ID, int width, int height)
        : ID(ID), width(width), height(height) {}
};