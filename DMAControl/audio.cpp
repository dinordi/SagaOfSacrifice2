#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>
#include <filesystem> // Requires C++17
#include <chrono>
#include <thread>
#include <cstdlib> // For setenv

// --- AudioManager Interface ---
class AudioManager {
public:
    virtual ~AudioManager() = default;

    virtual void initialize(const std::string& basePath) = 0;
    virtual void loadSound(const std::string& filePath) = 0;
    virtual void playSound(const std::string& soundName) = 0;
    virtual void stopSound(const std::string& soundName) = 0;
    virtual void setVolume(float volume) = 0;

    // Methods for managing background music
    virtual void loadMusic(const std::string& filePath) = 0;
    virtual void playMusic() = 0;
    virtual void pauseMusic() = 0;
    virtual void stopMusic() = 0;
    virtual void setMusicVolume(float volume) = 0;
};

// --- SDL2AudioManager Class Definition ---
class SDL2AudioManager : public AudioManager {
private:
    std::unordered_map<std::string, Mix_Chunk*> soundEffects;
    Mix_Music* backgroundMusic;
    bool isInitialized;
    float masterVolume;
    float musicVolume;
    std::string basePath;

    std::string getFilenameFromPath(const std::string& filePath);
    void cleanup();

public:
    SDL2AudioManager();
    ~SDL2AudioManager() override;

    void initialize(const std::string& basePath) override;
    void loadSound(const std::string& filePath) override;
    void playSound(const std::string& soundName) override;
    void stopSound(const std::string& soundName) override;
    void setVolume(float volume) override;

    void loadMusic(const std::string& filePath) override;
    void playMusic() override;
    void pauseMusic() override;
    void stopMusic() override;
    void setMusicVolume(float volume) override;
};

// --- SDL2AudioManager Implementation ---

SDL2AudioManager::SDL2AudioManager()
    : backgroundMusic(nullptr), isInitialized(false), masterVolume(1.0f), musicVolume(1.0f) {
}

SDL2AudioManager::~SDL2AudioManager() {
    cleanup();
}

void SDL2AudioManager::initialize(const std::string& basePath)
{
    // Force SDL to use ALSA directly if on Linux (might not be needed/work on macOS)
    #ifdef __linux__
    setenv("SDL_AUDIODRIVER", "alsa", 1); // Consider removing/commenting out for macOS
    setenv("ALSA_CARD", "Generic", 1);   // Consider removing/commenting out for macOS
    setenv("AUDIODEV", "hw:0,0", 1);     // Consider removing/commenting out for macOS
    #endif

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL audio could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        return;
    }

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        SDL_QuitSubSystem(SDL_INIT_AUDIO); // Clean up SDL audio if mixer fails
        return;
    }

    this->basePath = basePath;
    isInitialized = true;
    std::cout << "SDL2 Audio Manager Initialized. Base path: " << this->basePath << std::endl;
}

std::string SDL2AudioManager::getFilenameFromPath(const std::string& filePath) {
    try {
         std::filesystem::path path(filePath);
         return path.stem().string();
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << " Path: " << filePath << std::endl;
        // Fallback or alternative naming strategy if needed
        size_t last_slash_idx = filePath.find_last_of("\\/");
        std::string filename = (std::string::npos == last_slash_idx) ? filePath : filePath.substr(last_slash_idx + 1);
        size_t dot_idx = filename.rfind('.');
        return (std::string::npos == dot_idx) ? filename : filename.substr(0, dot_idx);
    }
}


void SDL2AudioManager::loadSound(const std::string& filePath) {
    if (!isInitialized) {
        std::cerr << "Audio system not initialized!" << std::endl;
        return;
    }
    std::string fullPath = this->basePath + filePath;
    std::string soundName = getFilenameFromPath(filePath); // Use relative path for name

    // Check if sound is already loaded
    if (soundEffects.count(soundName)) {
        std::cout << "Sound already loaded: " << soundName << std::endl;
        return;
    }

    Mix_Chunk* sound = Mix_LoadWAV(fullPath.c_str());

    if (!sound) {
        std::cerr << "Failed to load sound effect: " << fullPath << std::endl;
        std::cerr << "SDL_mixer Error: " << Mix_GetError() << std::endl;
        return;
    }

    // Store the sound with its name as the key
    soundEffects[soundName] = sound;
    std::cout << "Loaded sound: " << soundName << " from " << fullPath << std::endl;
}

void SDL2AudioManager::playSound(const std::string& soundName) {
    if (!isInitialized) return;

    auto it = soundEffects.find(soundName);
    if (it != soundEffects.end() && it->second != nullptr) {
        int channel = Mix_PlayChannel(-1, it->second, 0);
        if (channel == -1) {
            std::cerr << "Could not play sound effect '" << soundName << "': " << Mix_GetError() << std::endl;
        } else {
            int volumeValue = static_cast<int>(MIX_MAX_VOLUME * masterVolume);
            Mix_Volume(channel, volumeValue);
            std::cout << "Playing sound: " << soundName << " on channel " << channel << std::endl;
        }
    } else {
        std::cerr << "Sound not found or null: " << soundName << std::endl;
    }
}

void SDL2AudioManager::stopSound(const std::string& soundName) {
    if (!isInitialized) return;

    auto it = soundEffects.find(soundName);
    if (it == soundEffects.end() || it->second == nullptr) {
         std::cerr << "Cannot stop sound, not found: " << soundName << std::endl;
         return;
    }

    // Stop all channels playing this sound
    for (int i = 0; i < Mix_AllocateChannels(-1); i++) {
        if (Mix_Playing(i) && Mix_GetChunk(i) == it->second) {
            Mix_HaltChannel(i);
            std::cout << "Stopped sound: " << soundName << " on channel " << i << std::endl;
        }
    }
}

void SDL2AudioManager::setVolume(float volume) {
    masterVolume = volume;
    // Clamp volume between 0 and 1
    if (masterVolume < 0.0f) masterVolume = 0.0f;
    if (masterVolume > 1.0f) masterVolume = 1.0f;

    if (!isInitialized) return;

    // Update all currently playing sound effects
    int volumeValue = static_cast<int>(MIX_MAX_VOLUME * masterVolume);
    for (int i = 0; i < Mix_AllocateChannels(-1); i++) {
        if (Mix_Playing(i)) {
            Mix_Volume(i, volumeValue);
        }
    }
     std::cout << "Master volume set to: " << masterVolume << std::endl;
}

void SDL2AudioManager::loadMusic(const std::string& filePath) {
    if (!isInitialized) return;

    std::string fullPath = this->basePath + filePath;

    // Free previous music if it exists
    if (backgroundMusic != nullptr) {
        Mix_FreeMusic(backgroundMusic);
        backgroundMusic = nullptr;
        std::cout << "Freed previous background music." << std::endl;
    }

    backgroundMusic = Mix_LoadMUS(fullPath.c_str());
    if (!backgroundMusic) {
        std::cerr << "Failed to load music: " << fullPath << std::endl;
        std::cerr << "SDL_mixer Error: " << Mix_GetError() << std::endl;
    } else {
        std::cout << "Music loaded: " << fullPath << std::endl;
    }
}

void SDL2AudioManager::playMusic() {
    if (!isInitialized || !backgroundMusic) {
         std::cerr << "Cannot play music: Not initialized or no music loaded." << std::endl;
         return;
    }

    if (Mix_PlayingMusic() == 0) {
        // -1 means loop indefinitely
        if (Mix_PlayMusic(backgroundMusic, -1) == -1) {
            std::cerr << "Failed to play music: " << Mix_GetError() << std::endl;
        } else {
             std::cout << "Playing music." << std::endl;
        }
    } else if (Mix_PausedMusic() == 1) {
        // Resume paused music
        Mix_ResumeMusic();
         std::cout << "Resumed music." << std::endl;
    } else {
         std::cout << "Music is already playing." << std::endl;
    }

    setMusicVolume(musicVolume); // Apply current volume setting
}

void SDL2AudioManager::pauseMusic() {
    if (isInitialized && Mix_PlayingMusic() == 1 && Mix_PausedMusic() == 0) {
        Mix_PauseMusic();
        std::cout << "Paused music." << std::endl;
    }
}

void SDL2AudioManager::stopMusic() {
    if (isInitialized && Mix_PlayingMusic() == 1) {
        Mix_HaltMusic();
        std::cout << "Stopped music." << std::endl;
    }
}

void SDL2AudioManager::setMusicVolume(float volume) {
    musicVolume = volume;
    // Clamp volume between 0 and 1
    if (musicVolume < 0.0f) musicVolume = 0.0f;
    if (musicVolume > 1.0f) musicVolume = 1.0f;

    if (isInitialized) {
        int volumeValue = static_cast<int>(MIX_MAX_VOLUME * musicVolume);
        Mix_VolumeMusic(volumeValue);
         std::cout << "Music volume set to: " << musicVolume << std::endl;
    }
}

void SDL2AudioManager::cleanup() {
    if (!isInitialized) return;

    std::cout << "Cleaning up SDL2 Audio Manager..." << std::endl;

    // Stop all sounds and music
    Mix_HaltChannel(-1);
    Mix_HaltMusic();

    // Free all loaded sounds
    for (auto& pair : soundEffects) {
        if (pair.second != nullptr) {
            Mix_FreeChunk(pair.second);
            // std::cout << "Freed sound chunk: " << pair.first << std::endl;
        }
    }
    soundEffects.clear();

    // Free music
    if (backgroundMusic != nullptr) {
        Mix_FreeMusic(backgroundMusic);
        backgroundMusic = nullptr;
        // std::cout << "Freed background music." << std::endl;
    }

    // Close SDL_mixer and SDL audio
    Mix_CloseAudio();
    while(Mix_Init(0)) { // Keep quitting subsystems until Mix_Init returns 0
        Mix_Quit();
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    isInitialized = false;
    std::cout << "SDL2 Audio Manager cleanup complete." << std::endl;
}


// --- Main Function ---
int main(int argc, char* argv[]) {
    std::cout << "Starting Audio Test..." << std::endl;

    // --- IMPORTANT: Set the correct path to your assets folder ---
    // If your executable is in the SOS folder, "./assets/" should work.
    // Otherwise, provide the full path, e.g., "/Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/assets/"
    std::string assetsPath = "/home/root/SagaOfSacrifice2/SOS/assets/";
    // std::string assetsPath = "/Users/dinordi/Documents/GitHub/SagaOfSacrifice2/SOS/assets/"; // Example absolute path

    system("amixer -c 0 set Master 100%"); // Set system volume to 100% using ALSA directly
    SDL2AudioManager audioManager;
    audioManager.initialize(assetsPath);

    // Load a sound effect (relative path from assetsPath)
    audioManager.loadSound("sfx/jump.wav");
    audioManager.loadSound("music/menu/menu.wav");

    // Load music (optional)
    // audioManager.loadMusic("music/menu/menu.wav");

    // Play the sound effect
    std::cout << "Playing sound 'jump'..." << std::endl;
    audioManager.playSound("menu");
    // audioManager.playSound("jump");

    // Play music (optional)
    // audioManager.setMusicVolume(0.5f); // Set volume before playing
    // audioManager.playMusic();

    // Keep the program running for a bit so the sound can play
    std::cout << "Waiting for 5 seconds..." << std::endl;
    SDL_Delay(5000); // Wait 5 seconds

    // Stop music before cleanup (optional, cleanup handles it too)
    // audioManager.stopMusic();

    std::cout << "Audio Test Finished." << std::endl;

    // audioManager's destructor will automatically call cleanup() when main exits
    return 0;
}