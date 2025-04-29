// filepath: /Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/include/sprite_data.h
#pragma once
#include <string>

#define DEFINE_GETTER_SETTER(type, member) \
private:                                   \
    type member;                           \
public:                                    \
    type& get##member() { return member; } \
    void set##member(const type& value) { member = value; }

#define DEFINE_CONST_GETTER_SETTER(type, member) \
private:                                   \
    const type member;                           \
public:                                    \
    const type& get##member() const { return member; } \
    
struct SpriteRect {
    int x; // X position of the sprite in the sprite sheet
    int y; // Y position of the sprite in the sprite sheet
    int w; // Width of the sprite
    int h; // Height of the sprite
    std::string id_; // ID of the sprite, i.e. player.png, enemy.png, etc.

    SpriteRect(int x, int y, int w, int h, std::string id)
        : x(x), y(y), w(w), h(h), id_(std::move(id)) {}
};


class SpriteData {
public:
    // Constructor
    SpriteData(std::string id, int width, int height, int columns);

    SpriteRect getSpriteRect(int index) const;

    int width;  // Width of the sprite, consistent in a single sprite sheet
    int height; // Height of the sprite, consistent in a single sprite sheet
    int columns; // Number of columns in the sprite sheet
private:
    DEFINE_CONST_GETTER_SETTER(std::string, id_);
};