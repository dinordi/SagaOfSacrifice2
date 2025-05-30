// SOS/main.cpp

#include "game.h"
#include "utils/TimeUtils.h" // Include our TimeUtils header
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <csignal>
#include <filesystem>

#include "input_pl.h"
#include "SDL2Input.h"
#include "Renderer.h"
#include "SDL2AudioManager.h"

float FPS = 60.0f;
volatile bool running = true;


void handle_sigint(int) {
    std::cout << "SIGINT received. Exiting..." << std::endl;
    running = false;
}
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
    std::cout << "  -d, --debug                   Just load in an image and quit. For debugging purposes" << std::endl;
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
    std::signal(SIGINT, handle_sigint); // Register signal handler for Ctrl+C
    // Parse command line arguments
    std::string imageName = "Solid_blue";
    bool enableRemoteMultiplayer = false;
    bool localOnlyMode = false;
    bool debugMode = false;
    bool devMode = false;
    bool useEmbeddedServer = true; // Default to embedded server
    std::string serverAddress = "localhost";
    int serverPort = 8080;
    std::string playerId = generateRandomPlayerId();
    Renderer *renderer;
    
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
        } else if (arg == "-d" || arg == "--debug") {
            debugMode = true;
            std::cout << "Debug mode enabled. Loading image and quitting." << std::endl;
        } else if (arg == "-dev" || arg == "--dev") {
            devMode = true;
            std::cout << "Development mode enabled. Running headless" << std::endl;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
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
    
    // Get path from where the executable is running
    std::string path = std::filesystem::current_path().string();
    // Assuming executable is in /SagaOfSacrifice2/SOS/client/build
    // Want to go to /SagaOfSacrifice2/SOS
    path = path.substr(0, path.find("/client/build"));
    path = path + "/SOS";

    std::string path_sprites = path + "/assets/sprites/";
    imageName = imageName + ".png";

    AudioManager* audio = new SDL2AudioManager();
    std::string path_assets = path + "/assets";
    if(!audio->initialize(path_assets)) {
        std::cerr << "Failed to initialize AudioManager." << std::endl;
    }
    if(audio)
    {
        audio->loadMusic("music/menu/menu.wav");
        audio->loadSound("sfx/001.wav");
        audio->loadSound("sfx/jump.wav");
        audio->playMusic();
        audio->playSound("001");
        audio->playSound("jump");
    }
    renderer = new Renderer(path_sprites + imageName);
    if(!devMode)
    {
        renderer = new Renderer(path_sprites + imageName);
        if(debugMode) {
            std::cout << "Debug mode: Loaded image." << std::endl;
            return 0;
        }
    }
    PlayerInput* controller = new SDL2Input();
    Game *game = new Game(controller, playerId);
    std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
    
    // Initialize network features based on mode
    if (enableRemoteMultiplayer) {
        // Connect to remote server for multiplayer
        if (!game->initializeServerConnection(serverAddress, serverPort, playerId)) {
            std::cerr << "Failed to initialize multiplayer. Continuing in single player mode." << std::endl;
        } else {
            std::cout << "Multiplayer initialized successfully!" << std::endl;
        }
    } else if (!localOnlyMode) {
        if (useEmbeddedServer) {
            // Initialize single player with embedded server (new recommended approach)
            if (!game->initializeSinglePlayerEmbeddedServer()) {
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
    
    
    // Timing setup
    const uint64_t fixed_timestep_us = 16666; // ~60 updates/sec (1,000,000 / 60)
    const uint64_t max_allowed_frame_time_us = 250000; // Cap to avoid spiral of death
    
    uint64_t last_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t accumulator_us = 0;
    
    std::cout << "Entering gameloop..." << std::endl;
    while (running && game->isRunning()) {
        uint64_t current_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        uint64_t frame_time_us = current_time_us - last_time_us;
        last_time_us = current_time_us;
    
        // Cap frame time to avoid spiral of death
        if (frame_time_us > max_allowed_frame_time_us) {
            frame_time_us = max_allowed_frame_time_us;
        }
    
        accumulator_us += frame_time_us;
    
        // Fixed timestep update
        while (accumulator_us >= fixed_timestep_us) {
            float fixed_delta_seconds = static_cast<float>(fixed_timestep_us) / 1000000.0f;
            game->update(fixed_delta_seconds);
            accumulator_us -= fixed_timestep_us;
        }
    
        // Optionally: render here if you want to decouple rendering from update
    
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while(running){}
    // Clean up
    if (game->isServerConnection()) {
        game->shutdownServerConnection();
    }
    delete renderer;
    return 0;
}