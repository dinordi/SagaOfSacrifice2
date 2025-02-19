#pragma once

#include <map>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>

extern std::map<char, int> characters;
extern std::map<char, SDL_Rect> characterRects;
extern std::unordered_map<int, SDL_Texture*> spriteMap;

void initializeCharacters();
SDL_Texture* LoadSprite(SDL_Renderer* renderer, const std::filesystem::path& path);