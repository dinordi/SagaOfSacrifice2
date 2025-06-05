#include "sprite_data.h"
#include <iostream>
using json = nlohmann::json;
namespace fs = std::filesystem;

std::unordered_map<std::string, SpriteData*> SpriteData::spriteCache;

SpriteData::SpriteData(std::string atlasPath)
{
    addSpriteSheet(atlasPath);
}

SpriteData::~SpriteData() {
    // Clean up spriteRects map
    spriteRects.clear();

    //Do not cleanup spriteCache, it needs to be shutdown manually
}

SpriteData* SpriteData::getSharedInstance(const std::string& atlasPath)
{
    // Check if we already have this sprite sheet loaded
    auto it = spriteCache.find(atlasPath);
    if (it != spriteCache.end()) {
        return it->second;
    }
    
    // If not, create a new instance and cache it
    SpriteData* newInstance = new SpriteData(atlasPath);
    spriteCache[atlasPath] = newInstance;
    return newInstance;
}

SpriteRect SpriteData::getSpriteRect(int index) const {
    auto it = spriteRects.find(index);
    if (it == spriteRects.end()) {
        for(const auto& pair : spriteRects) {
            std::cout << "[SpriteData] SpriteRect ID: " << pair.first << ", Rect: (" 
                      << pair.second.x << ", " << pair.second.y << ", "
                      << pair.second.w << ", " << pair.second.h << ")\n";
        }
        std::cout << "Could not find sprite rect for index: " << index << std::endl;
        return SpriteRect(); // Return an empty SpriteRect if not found
    }
    
    const SpriteRect& spriteRect = it->second;

    return spriteRect;
}

void SpriteData::makeSpriteRect(json& data, int index) {
    // Load the sprite sheet image from the given atlas path
    // load level JSON file

    const json& sprites = data["textures"][0]["sprites"];
    const json& sprite = sprites[index];
    const json& region = sprite["region"];

    int x = region["x"].get<int>();
    int y = region["y"].get<int>();
    int w = region["w"].get<int>();
    int h = region["h"].get<int>();

    SpriteRect(x, y, w, h, "wolfman_idle.png");//Temp png
}

void SpriteData::addSpriteSheet(std::string atlasPath) 
{
    std::fstream file(atlasPath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open sprite sheet file: " + atlasPath);
    }
    json data;
    try {
        file >> data;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Error parsing sprite sheet JSON: " + std::string(e.what()));
    }

    // Manual conversion from JSON to std::vector<SpriteRect>
    int index = 0;
    std::string image = data["textures"][0]["image"].get<std::string>();
    setid_(image); // Set the id_ to the image name
    for (const auto& sprite : data["textures"][0]["sprites"]) {
        const auto& region = sprite["region"];
        int x = region["x"].get<int>();
        int y = region["y"].get<int>();
        int w = region["w"].get<int>();
        int h = region["h"].get<int>();
        spriteRects[index] = SpriteRect(x, y, w, h, image);
        index++;
    }
    // Store the sprite data in the map
}