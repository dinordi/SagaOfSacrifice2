#include "sprites_sdl.h"
#include "sprites.h"

std::map<char, int> characters;
std::map<int, SDL_Rect> characterRects;
std::unordered_map<int, SDL_Texture*> spriteMap;

// Function to load a sprite
SDL_Texture* LoadSprite(SDL_Renderer* renderer, const std::filesystem::path& path) {
    auto surface = IMG_Load(path.string().c_str());
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load sprite: %s", path.string().c_str());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

void initializeCharacters(SDL_Renderer* renderer, const std::filesystem::path& basePath) {
    int spriteWidth = 16; // Example width
    int spriteHeight = 16; // Example height
    int columns = 16; // Number of columns in the sprite sheet

    auto lettersPath = basePath / "SOS/sprites/letters.png";
    SDL_Texture* lettersTexture = LoadSprite(renderer, lettersPath);
    if (!lettersTexture) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load letters sprite: %s", lettersPath.string().c_str());
        return;
    }

    for (char c = 'a'; c <= 'z'; ++c) {
        characters[c] = c - 'a' + 11;
        int index = c - 'a';
        int x = (index % columns) * spriteWidth;
        int y = (index / columns) * spriteHeight;
        characterRects[characters[c]] = { x, y, spriteWidth, spriteHeight };
    }
    for (char c = '0'; c <= '9'; ++c) {
        characters[c] = c - '0' + 37;
        int index = 26 + (c - '0');
        int x = (index % columns) * spriteWidth;
        int y = (index / columns) * spriteHeight;
        characterRects[characters[c]] = { x, y, spriteWidth, spriteHeight };
    }
    characters[' '] = 100;
    characterRects[characters[' ']] = { 0, 11 * spriteHeight, spriteWidth, spriteHeight };

    // Store the letters texture in spriteMap
    spriteMap[0] = lettersTexture;
}