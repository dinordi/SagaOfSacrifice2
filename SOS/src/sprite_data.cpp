#include "sprite_data.h"


SpriteData::SpriteData(std::string atlasPath)
{
    addSpriteSheet(atlasPath);
}

SpriteRect SpriteData::getSpriteRect(int index) const {
    return spriteRects.at(index);
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