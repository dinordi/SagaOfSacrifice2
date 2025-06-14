#include "sprites_sdl.h"

std::map<char, int> sdl_characters;
std::map<int, SDL_FRect> characterRects;
std::unordered_map<int, Sprite> spriteMap;
std::unordered_map<std::string, SDL_Texture*> spriteMap2;

SDL_Texture* LoadSprite(SDL_Renderer* renderer, const std::filesystem::path& path) {
    SDL_Surface* surface = IMG_Load(path.string().c_str());
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load sprite: %s", path.string().c_str());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

void initializeCharacters(SDL_Renderer* renderer, const std::filesystem::path& path) {
    float spriteWidth = 127; // Example width
    float spriteHeight = 127; // Example height
    int columns = 3; // Number of columns in the sprite sheet

    auto lettersPath = (path / "SOS/assets/sprites/letters.png").make_preferred();
    SDL_Texture* lettersTexture = LoadSprite(renderer, lettersPath);
    if (!lettersTexture) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load letters sprite: %s", lettersPath.string().c_str());
        return;
    }

    for (char c = 'a'; c <= 'z'; ++c) {
        sdl_characters[c] = c - 'a' + 11;
        int index = c - 'a';
        float x = (index % columns) * spriteWidth;
        float y = (index / columns) * spriteHeight;
        characterRects[sdl_characters[c]] = { x, y, spriteWidth, spriteHeight };
        spriteMap[sdl_characters[c]] = { lettersTexture, { x, y, spriteWidth, spriteHeight } };
    }

    for (char c = '0'; c <= '9'; ++c) {
        sdl_characters[c] = c - '0' + 37;
        int index = 26 + (c - '0');
        float x = (index % columns) * spriteWidth;
        float y = (index / columns) * spriteHeight;
        characterRects[sdl_characters[c]] = { x, y, spriteWidth, spriteHeight };
        spriteMap[sdl_characters[c]] = { lettersTexture, { x, y, spriteWidth, spriteHeight } };
    }
    sdl_characters[' '] = 100;
    characterRects[sdl_characters[' ']] = { 0, 11 * spriteHeight, spriteWidth, spriteHeight };
    spriteMap[sdl_characters[' ']] = { lettersTexture, { 0, 11 * spriteHeight, spriteWidth, spriteHeight } };


    // For all png files with numbers as filename
    for (int i = 1; i <= 11; ++i) {
        std::string filename = "SOS/assets/sprites/" + std::to_string(i) + ".png";
        SDL_Texture* texture = LoadSprite(renderer, (path / filename).make_preferred());
        if (!texture) {
            SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load sprite: %s", filename.c_str());
            continue;
        }
        spriteMap[i] = { texture, { 0, 0, spriteWidth, spriteHeight } };
    }
}

// ...existing code...
Sprite initSprite(const SpriteRect sprData, SDL_Renderer* renderer, const std::filesystem::path& baseAssetsPath) {
    std::string filename = "SOS/assets/sprites/" + sprData.id_ + ".png";
    SDL_Texture* texture = LoadSprite(renderer, (baseAssetsPath / filename).make_preferred());
    SDL_FRect srcRect = { 0, 0, static_cast<float>(sprData.w), static_cast<float>(sprData.h) };

    return { texture, srcRect };
}
// ...existing code...

void loadAllSprites(SDL_Renderer* renderer, const std::filesystem::path& path)
{
    //Get amount of images in sprites/ folder
    std::vector<std::string> imageFiles;

    for (const auto& entry : std::filesystem::directory_iterator(path / "SOS/assets/sprites/")) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            imageFiles.push_back(entry.path().filename().string());
        }
    }
    // Load each image into the spriteMap2
    for (const auto& imageFile : imageFiles) {
        std::string filename = "SOS/assets/sprites/" + imageFile;
        SDL_Texture* texture = LoadSprite(renderer, (path / filename).make_preferred());
        if (!texture) {
            SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load sprite: %s", filename.c_str());
            continue;
        }
        std::string imageName = imageFile.substr(0, imageFile.find_last_of('.'));
        spriteMap2[imageName] = texture;
    }

}
