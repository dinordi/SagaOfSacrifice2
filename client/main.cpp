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
    
    AudioManager& audio = SDL2AudioManager::Instance();

    std::string path = std::filesystem::current_path().string();
    std::string basePathStr = path;
    auto basePathSOS = std::filesystem::path(basePathStr);
    path = basePathSOS.string() + "/SOS";
    int timer = 0;
    if(audio.initialize(basePathSOS) == false) {
        std::cout << "AudioManager initialized successfully." << std::endl;
        audio.loadMusic("music/menu/menu.wav");
        audio.loadSound("sfx/001.wav");
        audio.loadSound("sfx/jump.wav");
        audio.loadSound("sfx/slash.wav");
        audio.loadSound("sfx/menu_nav.wav");
        audio.playMusic();

    }   
    while(1){
        timer = get_ticks();

        // play a sound every 5 seconds
        if (get_ticks() - timer > 5000) {
            // Play a random sound every 5 seconds
            int soundIndex = rand() % 3; // Assuming you have 3 sounds loaded
            switch (soundIndex) {
                case 0:
                    audio.playSound("slash");
                    break;
                case 1:
                    audio.playSound("jump");
                    break;
                case 2:
                    audio.playSound("menu_nav");
                    break;
                default:
                    break;
            }
            timer = get_ticks();
        }

                // play a sound every 5 seconds
        if (get_ticks() - timer > 15000) {
            // Play a random sound every 5 seconds
            int soundIndex = rand() % 3; // Assuming you have 3 sounds loaded
            switch (soundIndex) {
                case 0:
                    audio.playSound("slash");
                    break;
                case 1:
                    audio.playSound("jump");
                    break;
                case 2:
                    audio.playSound("menu_nav");
                    break;
                default:
                    break;
            }
            timer = get_ticks();
        }

        // play a sound every 5 seconds
        if (get_ticks() - timer > 1500) {
            // Play a random sound every 5 seconds
            int soundIndex = rand() % 3; // Assuming you have 3 sounds loaded
            switch (soundIndex) {
                case 0:
                    audio.playSound("slash");
                    break;
                case 1:
                    audio.playSound("jump");
                    break;
                case 2:
                    audio.playSound("menu_nav");
                    break;
                default:
                    break;
            }
            timer = get_ticks();
        }

    }

    return 0;
}