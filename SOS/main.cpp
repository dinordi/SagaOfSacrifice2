// SOS/main.cpp

#include "game.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>

#include "petalinux/input_pl.h"

// Example usage in your game initialization
#include "petalinux/SDL2AudioManager.h"

#include "Renderer.h"

float FPS = 60.0f;

uint32_t get_ticks() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now().time_since_epoch();
    return duration_cast<milliseconds>(now).count();
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help                    Show this help message" << std::endl;
    std::cout << "  -i, --image <imageName>       Specify image name (default: Solid_blue)" << std::endl;
    std::cout << "  -m, --multiplayer             Enable multiplayer mode" << std::endl;
    std::cout << "  -s, --server <serverAddress>  Specify server address (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>             Specify server port (default: 8080)" << std::endl;
    std::cout << "  -id, --playerid <id>          Specify player ID (default: random)" << std::endl;
}

std::string generateRandomPlayerId() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int idLength = 8;
    std::string result;
    result.reserve(idLength);
    
    for (int i = 0; i < idLength; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    std::string imageName = "Solid_blue";
    bool enableMultiplayer = false;
    std::string serverAddress = "localhost";
    int serverPort = 8080;
    std::string playerId = generateRandomPlayerId();
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-i" || arg == "--image") {
            if (i + 1 < argc) {
                imageName = argv[++i];
            }
        } else if (arg == "-m" || arg == "--multiplayer") {
            enableMultiplayer = true;
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                serverAddress = argv[++i];
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                serverPort = std::atoi(argv[++i]);
                if (serverPort <= 0 || serverPort > 65535) {
                    std::cerr << "Invalid port number. Using default port 8080." << std::endl;
                    serverPort = 8080;
                }
            }
        } else if (arg == "-id" || arg == "--playerid") {
            if (i + 1 < argc) {
                playerId = argv[++i];
            }
        }
    }
    
    std::cout << "Image name: " << imageName << std::endl;
    if (enableMultiplayer) {
        std::cout << "Multiplayer enabled. Connecting to server: " << serverAddress 
                  << ":" << serverPort << " with player ID: " << playerId << std::endl;
    }
    
    std::string path = "/home/root/SagaOfSacrifice2/SOS/assets/sprites/";
    imageName = imageName + ".png";

    Renderer renderer(path + imageName);
    PlayerInput* controller = new EvdevController();
    Game game(controller);
    std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
    renderer.init();
    
    // Initialize multiplayer if enabled
    if (enableMultiplayer) {
        if (!game.initializeMultiplayer(serverAddress, serverPort, playerId)) {
            std::cerr << "Failed to initialize multiplayer. Continuing in single player mode." << std::endl;
        } else {
            std::cout << "Multiplayer initialized successfully!" << std::endl;
        }
    }
    
    auto lastTime = get_ticks();
    auto lastRenderTime = lastTime;
    
    // In your game initialization code
    std::unique_ptr<AudioManager> audioManager = std::make_unique<SDL2AudioManager>();
    audioManager->initialize("/home/root/SagaOfSacrifice2/SOS/assets/");
    
    // Load sounds and music
    audioManager->loadMusic("music/menu/menu.wav");
    audioManager->loadSound("sfx/jump.wav");

    // Play music
    audioManager->setMusicVolume(0.9f);
    
    std::cout << "Entering gameloop..." << std::endl;
    while (game.isRunning()) {
        auto currentTime = get_ticks();
        uint32_t deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Update game logic
        game.update(deltaTime);
        
        // Render game state at appropriate intervals
        uint32_t renderElapsedTime = currentTime - lastRenderTime;
        if (renderElapsedTime > 1000.0f / FPS) {
            renderer.render(game.getObjects());
            lastRenderTime = currentTime;
        }
        
        // Add small sleep to prevent CPU hogging
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Clean up
    if (game.isMultiplayerActive()) {
        game.shutdownMultiplayer();
    }
    
    return 0;
}