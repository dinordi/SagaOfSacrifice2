// filepath: /Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/include/sprite_data.h
#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <map>
#include <filesystem>
using json = nlohmann::json;

#define DEFINE_GETTER_SETTER(type, member) \
private:                                   \
    type member;                           \
public:                                    \
    type& get##member() { return member; } \
    const type& get##member() const { return member; } \
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
    uint8_t count;

    SpriteRect() : x(0), y(0), w(0), h(0), id_(""), count(0) {}
    SpriteRect(int x, int y, int w, int h, std::string id, uint8_t index)
        : x(x), y(y), w(w), h(h), id_(std::move(id)), count(index) {}
};


class SpriteData {
// SpriteData is a single spritesheet of a single animation
public:
    // Constructor
    SpriteData(std::string atlasPath);
    ~SpriteData();

    static SpriteData* getSharedInstance(const std::string& atlasPath);

    SpriteRect getSpriteRect(int index) const;
    const std::map<int, SpriteRect>& getSpriteRects() const { return spriteRects; }

    void addSpriteSheet(std::string atlasPath);

    int width;  // Width of the sprite, consistent in a single sprite sheet
    int height; // Height of the sprite, consistent in a single sprite sheet
    int columns; // Number of columns in the sprite sheet
private:
    DEFINE_GETTER_SETTER(std::string, id_);
    std::map<int, SpriteRect> spriteRects; // Maps sprite sheet ID to its rects

    static std::unordered_map<std::string, SpriteData*> spriteCache;
};