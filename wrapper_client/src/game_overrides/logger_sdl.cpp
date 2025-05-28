#include "logger_sdl.h"
#include <SDL3/SDL.h>

void LoggerSDL::log(const std::string &message) {
    SDL_Log("SDL Logger: %s", message.c_str());
}