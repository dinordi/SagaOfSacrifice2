#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const std::string &message) = 0;

    static Logger* getInstance();
    static void setInstance(Logger* instance);

protected:
    Logger() = default;

private:
    static Logger* instance;
    static std::mutex mutex;
};

#endif // LOGGER_H