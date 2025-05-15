// SOS/main.cpp

#include "game.h"
#include "utils/TimeUtils.h" // Include our TimeUtils header
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <filesystem>

#include "input_pl.h"
#include "SDL2AudioManager.h"
#include "Renderer.h"
#include "SDL2AudioManager.h"

float FPS = 60.0f;

// Note: get_ticks() is now defined in TimeUtils.cpp

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help                    Show this help message" << std::endl;
    std::cout << "  -i, --image <imageName>       Specify image name (default: Solid_blue)" << std::endl;
    std::cout << "  -m, --multiplayer             Enable multiplayer mode with remote server" << std::endl;
    std::cout << "  -s, --server <serverAddress>  Specify server address (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>             Specify server port (default: 8080)" << std::endl;
    std::cout << "  -id, --playerid <id>          Specify player ID (default: random)" << std::endl;
    std::cout << "  -l, --local                   Run in local-only mode without server (for development)" << std::endl;
    std::cout << "  -e, --embedded               Use embedded server (default) instead of external server" << std::endl;
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
    bool enableRemoteMultiplayer = false;
    bool localOnlyMode = false;
    bool useEmbeddedServer = true; // Default to embedded server
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
            enableRemoteMultiplayer = true;
        } else if (arg == "-l" || arg == "--local") {
            localOnlyMode = true;
        } else if (arg == "-e" || arg == "--embedded") {
            useEmbeddedServer = true;
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
    if (enableRemoteMultiplayer) {
        std::cout << "Multiplayer enabled. Connecting to server: " << serverAddress 
                  << ":" << serverPort << " with player ID: " << playerId << std::endl;
    } else if (!localOnlyMode) {
        if (useEmbeddedServer) {
            std::cout << "Single player mode with embedded server enabled." << std::endl;
        } else {
            std::cout << "Single player mode with external server enabled." << std::endl;
        }
    } else {
        std::cout << "Local-only mode (no server) enabled for development." << std::endl;
    }
    
    std::string path = "/home/root/SagaOfSacrifice2/SOS/assets/sprites/";
    imageName = imageName + ".png";

    AudioManager* audio = new SDL2AudioManager();
    std::string basePathSOS = "/home/root/SagaOfSacrifice2";
    if(audio->initialize(basePathSOS) == false) {
    std::string basePathSOS = "/home/root/SagaOfSacrifice2/SOS/assets/";
    if(!audio->initialize(basePathSOS)) {
        std::cerr << "Failed to initialize AudioManager." << std::endl;
        return -1;
    }
    std::cout << "AudioManager initialized successfully." << std::endl;
    audio->loadMusic("music/menu/menu.wav");
    audio->loadSound("sfx/001.wav");
    audio->loadSound("sfx/jump.wav");
    audio->playMusic();
    audio->playSound("001");
    audio->playSound("jump");

    Renderer renderer(path + imageName);
    PlayerInput* controller = new EvdevController();
    Game game(controller, playerId);
    std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
    renderer.init();
    
    // Initialize network features based on mode
    if (enableRemoteMultiplayer) {
        // Connect to remote server for multiplayer
        if (!game.initializeServerConnection(serverAddress, serverPort, playerId)) {
            std::cerr << "Failed to initialize multiplayer. Continuing in single player mode." << std::endl;
        } else {
            std::cout << "Multiplayer initialized successfully!" << std::endl;
        }
    } else if (!localOnlyMode) {
        if (useEmbeddedServer) {
            // Initialize single player with embedded server (new recommended approach)
            if (!game.initializeSinglePlayerEmbeddedServer()) {
                std::cerr << "Failed to initialize embedded server. Falling back to local-only mode." << std::endl;
            } else {
                std::cout << "Single player with embedded server initialized successfully!" << std::endl;
            }
        }
    } else {
        // Local-only mode (no server) for development/debugging
        std::cout << "Running in local-only mode without server." << std::endl;
    }

    auto lastTime = get_ticks();
    auto lastRenderTime = lastTime;
    
    // In your game initialization code
    // std::unique_ptr<AudioManager> audioManager = std::make_unique<SDL2AudioManager>();
    // audioManager->initialize("/home/root/SagaOfSacrifice2/SOS/assets/");
    
    // // Load sounds and music
    // audioManager->loadMusic("music/menu/title.wav");
    // audioManager->loadSound("sfx/jump.wav");
    // audioManager->loadSound("sfx/001.wav");
    // // Play music
    // audioManager->setMusicVolume(0.9f);
    
    std::cout << "Entering gameloop..." << std::endl;
    while (game.isRunning()) {
        auto currentTime = get_ticks();
        uint32_t deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Update game logic
        game.update(deltaTime);
        
        // Render game state at appropriate intervals
        // uint32_t renderElapsedTime = currentTime - lastRenderTime;
        // if (renderElapsedTime > 1000.0f / FPS) {
        //     renderer.render(game.getObjects());
        //     lastRenderTime = currentTime;
        // }
        
        // Add small sleep to prevent CPU hogging
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Clean up
    if (game.isServerConnection()) {
        game.shutdownServerConnection();
    }
    
    return 0;
}