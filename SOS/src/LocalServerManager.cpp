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
#include "level_manager.h"
#include "collision/CollisionManager.h"
#endif

LocalServerManager::LocalServerManager() : serverRunning_(false), serverPort_(0) {
}

LocalServerManager::~LocalServerManager() {

}

bool LocalServerManager::startEmbeddedServer(int port, LevelManager* levelManager, CollisionManager* collisionManager) {
    if (serverRunning_) {
        std::cerr << "[LocalServerManager] Server is already running" << std::endl;
        return false;
    }
    
    serverPort_ = port;
    
    std::cout << "[LocalServerManager] Starting embedded server on port " << port << std::endl;
    
    try {
        embeddedServer_ = std::make_unique<EmbeddedServer>(port, levelManager);
        embeddedServer_->start();
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
    std::cout << "[LocalServerManager] Attempting to stop embedded server..." << std::endl;
    
    if (!serverRunning_) {
        std::cout << "[LocalServerManager] No server was running, nothing to stop" << std::endl;
        return true;
    }
    
    if (!embeddedServer_) {
        std::cerr << "[LocalServerManager] ERROR: Server marked as running but no server instance exists!" << std::endl;
        serverRunning_ = false;
        return false;
    }
    
    try {
        std::cout << "[LocalServerManager] Calling stop() on embedded server..." << std::endl;
        embeddedServer_->stop();
        std::cout << "[LocalServerManager] Embedded server stop() completed" << std::endl;
        
        embeddedServer_.reset();
        serverRunning_ = false;
        
        std::cout << "[LocalServerManager] Embedded server successfully stopped and resources freed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[LocalServerManager] ERROR while stopping embedded server: " << e.what() << std::endl;
        
        try {
            embeddedServer_.reset();
        } catch (...) {
        }
        
        serverRunning_ = false;
        return false;
    }
}