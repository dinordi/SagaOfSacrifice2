#include "sprite_data.h"
#include <iostream>
using json = nlohmann::json;
namespace fs = std::filesystem;

SpriteData::SpriteData(std::string id, int width, int height, int columns)
    : id_(std::move(id)), width(width), height(height), columns(columns)
{
    std::string basePathStr = fs::current_path().string();
    std::size_t pos = basePathStr.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        basePathStr = basePathStr.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = fs::path(basePathStr);
    atlasPath = (basePath / "SOS" / "assets" / "spriteatlas" / (id_ + ".tpsheet")).string();
    
    // std::cout << "[SpriteData] SpriteData created with ID: " << id_
    //           << ", Width: " << width << ", Height: " << height 
    //           << ", Columns: " << columns << std::endl;

    if (!fs::exists(atlasPath)) {
        // std::cerr << "[SpriteData] Atlas path not found: " << atlasPath<< "\n";
    } else {
        // std::cout << "[SpriteData] Sprite path found: " << atlasPath << "\n";
    }
}


SpriteRect SpriteData::getSpriteRect(int index) const {
    std::ifstream file(atlasPath);
    if (!file.is_open()) {
        // std::cerr << "[SpriteData] Failed to open sprite JSON: " << atlasPath << std::endl;
        return SpriteRect(0, 0, 0, 0, id_);
    }

    json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        std::cerr << "[SpriteData] JSON parse error: " << e.what() << std::endl;
        return SpriteRect(0, 0, 0, 0, id_);
    }

    // Navigate to the sprites array
    if (!data.contains("textures") || !data["textures"].is_array() || data["textures"].empty()) {
        std::cerr << "[SpriteData] Invalid or missing 'textures' array in JSON." << std::endl;
        return SpriteRect(0, 0, 0, 0, id_);
    }

    if (!data["textures"][0].contains("sprites") || !data["textures"][0]["sprites"].is_array()) {
        std::cerr << "[SpriteData] Missing or malformed 'sprites' array in JSON." << std::endl;
        return SpriteRect(0, 0, 0, 0, id_);
    }
    const json& sprites = data["textures"][0]["sprites"];
    if (index < 0 || index >= sprites.size()) {
        std::cerr << "[SpriteData] Invalid index for 'sprites' array." << std::endl;
        return SpriteRect(0, 0, 0, 0, id_);
    }

    const json& sprite = sprites[index];
    const json& region = sprite["region"];

    int x = region["x"].get<int>();
    int y = region["y"].get<int>();
    int w = region["w"].get<int>();
    int h = region["h"].get<int>();

    return SpriteRect(x, y, w, h, id_);
}