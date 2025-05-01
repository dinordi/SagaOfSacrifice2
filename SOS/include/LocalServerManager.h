#pragma once

#include <memory>
#include <thread>

// Class to manage launching and stopping a local game server
class LocalServerManager {
public:
    LocalServerManager();
    ~LocalServerManager();

    // Start the local server process
    bool startLocalServer(int port);

    // Stop the local server
    void stopLocalServer();

    // Check if local server is running
    bool isLocalServerRunning() const;

private:
    std::unique_ptr<std::thread> serverThread_;
    bool serverRunning_;
    int serverPort_;
};