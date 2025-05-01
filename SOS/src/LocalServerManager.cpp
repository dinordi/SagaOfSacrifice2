#include "LocalServerManager.h"
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
    stopLocalServer();
}

bool LocalServerManager::startLocalServer(int port, std::filesystem::path serverPath) {
    if (serverRunning_) {
        std::cerr << "Local server is already running" << std::endl;
        return false;
    }

    serverPort_ = port;
    
    // Start a thread to run the local server
    serverThread_ = std::make_unique<std::thread>([this, port, serverPath]() {
        // Command to run local server
        std::string command;
        
        #ifdef _WIN32
        // Windows-specific command
        command = "start /B .\\server\\build\\SagaServer.exe " + std::to_string(port);
        #else
        // macOS/Linux command
        
        command = serverPath.string() + std::to_string(port) + " &";
        #endif
        
        std::cout << "Starting local server with command: " << command << std::endl;
        int result = system(command.c_str());
        
        if (result != 0) {
            std::cerr << "Failed to start local server, error code: " << result << std::endl;
            serverRunning_ = false;
        } else {
            std::cout << "Local server process started successfully" << std::endl;
            serverRunning_ = true;
            
            // Wait for a bit to let the server initialize
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    // Detach the thread so it runs independently
    serverThread_->detach();
    
    // Wait a moment for the server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return true;
}

void LocalServerManager::stopLocalServer() {
    if (!serverRunning_) {
        return;
    }

    std::cout << "Stopping local server..." << std::endl;

    // Stop the server by sending a request or killing the process
    #ifdef _WIN32
    // Windows-specific termination (would need process ID tracking for a cleaner approach)
    system("taskkill /F /IM SagaServer.exe");
    #else
    // On Unix systems, we could use a more targeted approach if we stored the PID
    system("pkill -f \"SagaServer\"");
    #endif

    serverRunning_ = false;
    std::cout << "Local server stopped" << std::endl;
}

bool LocalServerManager::isLocalServerRunning() const {
    return serverRunning_;
}