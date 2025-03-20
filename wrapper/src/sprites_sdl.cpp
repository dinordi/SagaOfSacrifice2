#include "sprites_sdl.h"

std::map<char, int> sdl_characters;
std::map<int, SDL_FRect> characterRects;
std::unordered_map<int, Sprite> spriteMap;

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

    // Load additional sprites
    spriteMap[1] = { LoadSprite(renderer, (path / "SOS/assets/sprites/playerBig.png").make_preferred()), { 0, 0, spriteWidth, spriteHeight } };
    spriteMap[2] = { LoadSprite(renderer, (path / "SOS/assets/sprites/player.png").make_preferred()), { 0, 0, spriteWidth, spriteHeight } };
    spriteMap[3] = { LoadSprite(renderer, (path / "SOS/assets/sprites/fatbat.png").make_preferred()), { 0, 0, spriteWidth, spriteHeight } };
    spriteMap[4] = { LoadSprite(renderer, (path / "SOS/assets/sprites/tiles.png").make_preferred()), { 0, 0, spriteWidth, spriteHeight } };
    // Add more sprites as needed
}