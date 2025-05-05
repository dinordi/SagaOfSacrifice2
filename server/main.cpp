#include "include/GameServer.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <csignal>

// Default server port
const int DEFAULT_PORT = 8282;

// Pointer to server for signal handler
GameServer* g_server = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting Saga of Sacrifice 2 multiplayer server..." << std::endl;
        
        // Get port from command line arguments or use default
        int port = DEFAULT_PORT;
        if (argc > 1) {
            try {
                port = std::stoi(argv[1]);
                if (port < 1 || port > 65535) {
                    std::cerr << "Invalid port number. Using default port " << DEFAULT_PORT << std::endl;
                    port = DEFAULT_PORT;
		    std::cout << "Changed port to: " << port << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Invalid port argument. Using default port " << DEFAULT_PORT << std::endl;
            }
        }
        
        // Register signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Create and start server
        boost::asio::io_context io_context;
        GameServer server(io_context, port);
        g_server = &server;
        
        std::cout << "Server listening on port " << port << std::endl;
        server.start();
        
        // Run the io_context in this thread
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
