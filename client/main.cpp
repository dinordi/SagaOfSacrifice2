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
    Camera* camera;
    
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
    std::string basePathStr = path;
    std::size_t pos = basePathStr.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        basePathStr = basePathStr.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePathSOS = std::filesystem::path(basePathStr);

    path = basePathSOS.string() + "/SOS";

    std::string path_sprites = path + "/assets/spriteatlas/";
    imageName = imageName + ".png";

    AudioManager& audio = SDL2AudioManager::Instance();

    if(audio.initialize(basePathSOS) == false) {
        std::cout << "AudioManager initialized successfully." << std::endl;
        audio.loadMusic("music/menu/menu.wav");
        audio.loadSound("sfx/001.wav");
        audio.loadSound("sfx/jump.wav");
        audio.playMusic();
        audio.playSound("001");
        audio.playSound("jump");
    }

    
    
    camera = new Camera(1920, 1080); // Assuming 1920x1080 resolution
    renderer = new Renderer(std::filesystem::path(path_sprites), camera, devMode);
    if(debugMode) {
        std::cout << "Debug mode: Loaded image." << std::endl;
        return 0;
    }
    
    //wait 5 seconds
    std::cout << "Waiting for 2 seconds before starting the game..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    PlayerInput* controller = new SDL2Input();
    Game& game = Game::getInstance();
    game.setPlayerInput(controller);
    std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;

    // Initialize server configuration
    game.initializeServerConfig(basePathSOS);
    
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
    
    // Move initUIO() to after game initialization
    while (running && game.isRunning()) {
    uint64_t current_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t frame_time_us = current_time_us - last_time_us;
    last_time_us = current_time_us;

    // Cap frame time to avoid spiral of death
    if (frame_time_us > max_allowed_frame_time_us) {
        frame_time_us = max_allowed_frame_time_us;
    }

    //Print once a second
    static uint64_t last_print_time_us = 0;
    if (current_time_us - last_print_time_us >= 1000000) { // 1 second
        last_print_time_us = current_time_us;
        std::cout << "Game running... Frame time: " << frame_time_us << " us" << std::endl;
    }

    // Use actual frame time as deltaTime
    float deltaTime = static_cast<float>(frame_time_us) / 1000000.0f;
    game.update(deltaTime); // This varies based on actual frame time
    camera->update(game.getPlayer());

    // Initialize UIO only after game is fully set up and running
    static bool uio_initialized = false;
    if (!uio_initialized && game.isRunning()) {
        renderer->initUIO();
        uio_initialized = true;
    }
}
    // Clean up
    if (game.isServerConnection()) {
        game.shutdownServerConnection();
    }
    delete renderer;
    return 0;
}