#ifndef LOGGER_SDL_H
#define LOGGER_SDL_H

#include "logger.h"
#include <string>

class LoggerSDL : public Logger {
public:
    LoggerSDL();
    ~LoggerSDL();
    void log(const std::string& message) override;
};

#endif // LOGGER_SDL_H