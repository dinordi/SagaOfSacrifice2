#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

#include "network/EmbeddedServer.h"

// Default server port
const int DEFAULT_PORT = 8282;

// Global pointer to server for signal handler
std::unique_ptr<EmbeddedServer> g_server;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [port]" << std::endl;
    std::cout << "  port: Optional port number (default: " << DEFAULT_PORT << ")" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting Saga of Sacrifice 2 dedicated server..." << std::endl;
        
        // Get port from command line arguments or use default
        int port = DEFAULT_PORT;
        if (argc > 1) {
            if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
                printUsage(argv[0]);
                return 0;
            }
            
            try {
                port = std::stoi(argv[1]);
                if (port < 1 || port > 65535) {
                    std::cerr << "Invalid port number. Using default port " << DEFAULT_PORT << std::endl;
                    port = DEFAULT_PORT;
                }
            } catch (const std::exception& e) {
                std::cerr << "Invalid port argument. Using default port " << DEFAULT_PORT << std::endl;
            }
        }
        
        // Register signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Create and start server
        std::cout << "Initializing server on port " << port << std::endl;
        g_server = std::make_unique<EmbeddedServer>(port);
        g_server->start();
        
        std::cout << "Server running on port " << port << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        // Keep the main thread alive while the server is running
        while (g_server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "Server shutdown complete" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}