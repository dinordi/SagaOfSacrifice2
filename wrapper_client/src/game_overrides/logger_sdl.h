#ifndef LOGGER_SDL_H
#define LOGGER_SDL_H

#include "logger.h"

class LoggerSDL : public Logger {
public:
    void log(const std::string &message) override;
};

#endif // LOGGER_SDL_H