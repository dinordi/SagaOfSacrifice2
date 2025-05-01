#include "LocalServerManager.h"
#include "network/EmbeddedServer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#endif

LocalServerManager::LocalServerManager() : serverRunning_(false), serverPort_(0) {
}

LocalServerManager::~LocalServerManager() {

}

bool LocalServerManager::startEmbeddedServer(int port) {
    if (serverRunning_) {
        std::cerr << "[LocalServerManager] Server is already running" << std::endl;
        return false;
    }
    
    serverPort_ = port;
    
    std::cout << "[LocalServerManager] Starting embedded server on port " << port << std::endl;
    
    try {
        // Create and start the embedded server
        embeddedServer_ = std::make_unique<EmbeddedServer>(port);
        embeddedServer_->start();
        // Sleep for half a second...
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        serverRunning_ = true;
        std::cout << "[LocalServerManager] Embedded server started successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[LocalServerManager] Failed to start embedded server: " << e.what() << std::endl;
        embeddedServer_.reset();
        return false;
    }
}

bool LocalServerManager::stopEmbeddedServer()
{
    embeddedServer_->stop();
    return true;
}