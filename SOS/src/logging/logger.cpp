#include "logger.h"
#include <iostream>

Logger* Logger::instance = nullptr;
std::mutex Logger::mutex;

class DefaultLogger : public Logger {
public:
    void log(const std::string &message) override {
        std::cout << "Default Logger: " << message << std::endl;
    }
};

// Initialize the default logger
// Logger* Logger::instance = new DefaultLogger();

Logger* Logger::getInstance() {
    std::lock_guard<std::mutex> lock(mutex);
    if (instance == nullptr) {
        instance = new DefaultLogger(); // Default logger, can be overridden
    }
    return instance;
}

void Logger::setInstance(Logger* newInstance) {
    std::lock_guard<std::mutex> lock(mutex);
    if (instance != nullptr) {
        delete instance;
    }
    instance = newInstance;
}