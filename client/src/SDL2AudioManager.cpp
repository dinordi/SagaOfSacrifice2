#include "SDL2AudioManager.h"
#include <iostream>
#include <filesystem>

SDL2AudioManager::SDL2AudioManager() 
    : backgroundMusic(nullptr), isInitialized(false), masterVolume(1.0f), musicVolume(1.0f) {
}

SDL2AudioManager::~SDL2AudioManager() {
    cleanup();
}

void SDL2AudioManager::initialize(const std::string& basePath)
{
    // Force SDL to use ALSA directly if on Linux
    #ifdef __linux__
    setenv("SDL_AUDIODRIVER", "alsa", 1);
    setenv("ALSA_CARD", "Generic", 1);
    setenv("AUDIODEV", "hw:0,0", 1);
    #endif
    
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL audio could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return;
    }
    
    this->basePath = basePath;

    std::cout << "Audio initialized successfully. Driver in use: " << SDL_GetCurrentAudioDriver() << std::endl;
    isInitialized = true;
}

void SDL2AudioManager::loadSound(const std::string& filePath) {
    if (!isInitialized) {
        std::cerr << "Audio system not initialized!" << std::endl;
        return;
    }
    std::string fullPath = this->basePath + filePath;

    std::string soundName = getFilenameFromPath(fullPath);
    Mix_Chunk* sound = Mix_LoadWAV(fullPath.c_str());
    
    if (!sound) {
        std::cerr << "Failed to load sound effect: " << fullPath << std::endl;
        std::cerr << "SDL_mixer Error: " << Mix_GetError() << std::endl;
        return;
    }
    
    // Store the sound with its name as the key
    soundEffects[soundName] = sound;
    std::cout << "Loaded sound: " << soundName << std::endl;
}

void SDL2AudioManager::playSound(const std::string& soundName) {
    if (!isInitialized) return;
    
    auto it = soundEffects.find(soundName);
    if (it != soundEffects.end()) {
        int channel = Mix_PlayChannel(-1, it->second, 0);
        if (channel == -1) {
            std::cerr << "Could not play sound effect: " << Mix_GetError() << std::endl;
        } else {
            int volumeValue = static_cast<int>(MIX_MAX_VOLUME * masterVolume);
            Mix_Volume(channel, volumeValue);
        }
    } else {
        std::cerr << "Sound not found: " << soundName << std::endl;
    }
}

void SDL2AudioManager::stopSound(const std::string& soundName) {
    if (!isInitialized) return;
    
    // Stop all channels playing this sound
    for (int i = 0; i < Mix_AllocateChannels(-1); i++) {
        if (Mix_GetChunk(i) == soundEffects[soundName]) {
            Mix_HaltChannel(i);
        }
    }
}

void SDL2AudioManager::setVolume(float volume) {
    masterVolume = volume;
    // Clamp volume between 0 and 1
    if (masterVolume < 0.0f) masterVolume = 0.0f;
    if (masterVolume > 1.0f) masterVolume = 1.0f;

    // Update all currently playing sound effects
    for (int i = 0; i < Mix_AllocateChannels(-1); i++) {
        if (Mix_Playing(i) == 1) {
            Mix_Volume(i, static_cast<int>(MIX_MAX_VOLUME * masterVolume));
        }
    }
}

void SDL2AudioManager::loadMusic(const std::string& filePath) {
    if (!isInitialized) return;
    
    std::string fullPath = this->basePath + filePath;

    // Free previous music if it exists
    if (backgroundMusic != nullptr) {
        Mix_FreeMusic(backgroundMusic);
        backgroundMusic = nullptr;
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
    if (!isInitialized || !backgroundMusic) return;
    
    if (Mix_PlayingMusic() == 0) {
        // -1 means loop indefinitely
        if (Mix_PlayMusic(backgroundMusic, -1) == -1) {
            std::cerr << "Failed to play music: " << Mix_GetError() << std::endl;
        }
    } else if (Mix_PausedMusic() == 1) {
        // Resume paused music
        Mix_ResumeMusic();
    }
    
    setMusicVolume(musicVolume);
}

void SDL2AudioManager::pauseMusic() {
    if (isInitialized && Mix_PlayingMusic() == 1 && Mix_PausedMusic() == 0) {
        Mix_PauseMusic();
    }
}

void SDL2AudioManager::stopMusic() {
    if (isInitialized && Mix_PlayingMusic() == 1) {
        Mix_HaltMusic();
    }
}

void SDL2AudioManager::setMusicVolume(float volume) {
    musicVolume = volume;
    // Clamp volume between 0 and 1
    if (musicVolume < 0.0f) musicVolume = 0.0f;
    if (musicVolume > 1.0f) musicVolume = 1.0f;
    
    if (isInitialized) {
        Mix_VolumeMusic(static_cast<int>(MIX_MAX_VOLUME * musicVolume));
    }
}

std::string SDL2AudioManager::getFilenameFromPath(const std::string& filePath) {
    std::filesystem::path path(filePath);
    return path.stem().string();
}

void SDL2AudioManager::cleanup() {
    if (!isInitialized) return;
    
    // Free all loaded sounds
    for (auto& pair : soundEffects) {
        if (pair.second != nullptr) {
            Mix_FreeChunk(pair.second);
        }
    }
    soundEffects.clear();
    
    // Free music
    if (backgroundMusic != nullptr) {
        Mix_FreeMusic(backgroundMusic);
        backgroundMusic = nullptr;
    }
    
    // Close SDL_mixer and SDL audio
    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    isInitialized = false;
}