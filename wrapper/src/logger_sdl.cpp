#include "logger_sdl.h"
#include <SDL3/SDL.h>

LoggerSDL::LoggerSDL() {
    // Constructor implementation
}

LoggerSDL::~LoggerSDL() {
    // Destructor implementation
}

void LoggerSDL::log(const std::string& message) {
    SDL_Log("%s", message.c_str());
}