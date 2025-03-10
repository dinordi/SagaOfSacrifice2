#ifndef SPRITES_SDL_H
#define SPRITES_SDL_H

#include <map>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <unordered_map>

extern std::map<char, int> sdl_characters;
extern std::map<int, SDL_FRect> characterRects;
struct Sprite {
    SDL_Texture* texture;
    SDL_FRect srcRect;
};

extern std::unordered_map<int, Sprite> spriteMap;

void initializeCharacters(SDL_Renderer* renderer, const std::filesystem::path& path);
SDL_Texture* LoadSprite(SDL_Renderer* renderer, const std::filesystem::path& path);

#endif // SPRITES_SDL_H