#pragma once

#include <memory>
#include <thread>
#include <filesystem>
#include <functional>
#include <atomic>

// Forward declarations
class EmbeddedServer;

// Class to manage launching and stopping a local game server
class LocalServerManager {
public:
    LocalServerManager();
    ~LocalServerManager();

    // Start the local server as a thread in the same process
    bool startEmbeddedServer(int port); // Added parameters

    bool stopEmbeddedServer();

private:
    std::atomic<bool> serverRunning_;
    int serverPort_;
    std::unique_ptr<EmbeddedServer> embeddedServer_;
};