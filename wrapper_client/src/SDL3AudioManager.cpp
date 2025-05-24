#include "SDL3AudioManager.h"
#include <SDL3_mixer/SDL_mixer.h> 
#include <iostream> // For std::cerr
#include <filesystem> // For path manipulation if needed for getSoundName

// Helper function to extract sound name from file path
static std::string getSoundName(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string name = p.stem().string(); // Gets filename without extension
    return name;
}

SDL3AudioManager::SDL3AudioManager() : mMusic(nullptr), mInitialized(false) {
    // Constructor
}

SDL3AudioManager::~SDL3AudioManager() {
    if (mInitialized) {
        // Free all sound effects
        for (auto& pair : mSoundEffects) {
            if (pair.second) {
                Mix_FreeChunk(pair.second);
            }
        }
        mSoundEffects.clear();

        // Free music
        if (mMusic) {
            Mix_FreeMusic(mMusic);
            mMusic = nullptr;
        }

        Mix_CloseAudio();
        Mix_Quit(); // Quit SDL_mixer subsystems
    }
}

bool SDL3AudioManager::initialize(const std::string& basePath) {
    mBasePath = basePath;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL3 could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        mInitialized = false;
        return mInitialized;
    }

    // Initialize SDL_mixer
    if (Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG) == 0) {
        std::cerr << "Failed to initialize SDL_mixer (MP3 and OGG support for SDL3)! SDL_Error: " << SDL_GetError() << std::endl;
        if (Mix_Init(0) == 0) {
             std::cerr << "Failed to initialize SDL_mixer (basic support for SDL3)! SDL_Error: " << SDL_GetError() << std::endl;
             SDL_Quit();
             mInitialized = false;
             return mInitialized;
        }
    }
    
    // Open audio device for SDL3
    SDL_AudioSpec spec;
    SDL_zero(spec); // Initialize all fields to zero
    spec.freq = 44100;
    spec.format = SDL_AUDIO_S16; // Standard SDL3 16-bit signed audio format, native endian
    spec.channels = 2;

    if (Mix_OpenAudio(0, &spec) < 0) { // Pass device ID 0 for default, and pointer to spec
        std::cerr << "SDL_mixer (SDL3) could not open audio! SDL_Error: " << SDL_GetError() << std::endl;
        Mix_Quit();
        SDL_Quit();
        mInitialized = false;
        return mInitialized;
    }

    // Set the volume off
    Mix_Volume(-1, 0);

    mInitialized = true;
    std::cout << "SDL3AudioManager (wrapper_client) initialized successfully." << std::endl;
    return mInitialized;
}

bool SDL3AudioManager::loadSound(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot load sound." << std::endl;
        return false;
    }
    std::string fullPath = mBasePath + "/" + filePath;
    Mix_Chunk* sound = Mix_LoadWAV(fullPath.c_str());
    if (!sound) {
        std::cerr << "Failed to load sound effect (SDL3 - wrapper_client)! Path: " << fullPath << " SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::string soundName = getSoundName(filePath); // Derive soundName from filePath
    
    // Free existing sound if any, before replacing
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        Mix_FreeChunk(it->second);
    }
    mSoundEffects[soundName] = sound;
    std::cout << "Loaded sound (SDL3 - wrapper_client): " << soundName << " from " << fullPath << std::endl;
    return true;
}

bool SDL3AudioManager::playSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot play sound." << std::endl;
        return false;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        if (Mix_PlayChannel(-1, it->second, 0) == -1) {
            std::cerr << "Failed to play sound (SDL3 - wrapper_client): " << soundName << " SDL_Error: " << SDL_GetError() << std::endl;
            return false; // Failed to play
        }
        return true; // Sound played successfully
    } else {
        std::cerr << "Sound not found (SDL3 - wrapper_client): " << soundName << std::endl;
        return false; // Sound not found
    }
}

bool SDL3AudioManager::stopSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot stop sound." << std::endl;
        return false;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        bool soundStopped = false;
        // Iterate through all active channels
        for (int i = 0; i < Mix_AllocateChannels(-1); ++i) {
            if (Mix_Playing(i) && Mix_GetChunk(i) == it->second) {
                Mix_HaltChannel(i);
                soundStopped = true; // Mark that at least one instance was stopped
            }
        }
        return soundStopped; // Return true if at least one instance was stopped
    } else {
        std::cerr << "Sound not found (SDL3 - wrapper_client), cannot stop: " << soundName << std::endl;
        return false;
    }
}

bool SDL3AudioManager::setVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot set volume." << std::endl;
        return false;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (sdlVolume < 0) sdlVolume = 0;
    if (sdlVolume > MIX_MAX_VOLUME) sdlVolume = MIX_MAX_VOLUME;
    Mix_Volume(-1, sdlVolume); 
    return true; // SDL_mixer's Mix_Volume doesn't return a status. Assume success.
}

bool SDL3AudioManager::loadMusic(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot load music." << std::endl;
        return false;
    }
    std::string fullPath = mBasePath + "/" + filePath;
    if (mMusic) {
        Mix_HaltMusic();
        Mix_FreeMusic(mMusic);
        mMusic = nullptr;
    }
    mMusic = Mix_LoadMUS(fullPath.c_str());
    if (!mMusic) {
        std::cerr << "Failed to load music (SDL3 - wrapper_client)! Path: " << fullPath << " SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    } else {
        std::cout << "Loaded music (SDL3 - wrapper_client): " << filePath << std::endl;
        return true;
    }
}

bool SDL3AudioManager::playMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot play music." << std::endl;
        return false;
    }
    if (mMusic) {
        if (Mix_PlayingMusic() == 0) { 
            if (Mix_PlayMusic(mMusic, -1) == -1) { 
                std::cerr << "Failed to play music (SDL3 - wrapper_client)! SDL_Error: " << SDL_GetError() << std::endl;
                return false;
            }
        } else if (Mix_PausedMusic() == 1) { 
            Mix_ResumeMusic();
        }
        return true; // Music is playing or resumed
    } else {
        std::cerr << "No music loaded to play (SDL3 - wrapper_client)." << std::endl;
        return false;
    }
}

bool SDL3AudioManager::pauseMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot pause music." << std::endl;
        return false;
    }
    if (Mix_PlayingMusic() == 1) { 
        Mix_PauseMusic();
        return true;
    }
    return true; // Consider it a success if the state is achieved (music is paused or not playing)
}

bool SDL3AudioManager::stopMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot stop music." << std::endl;
        return false;
    }
    Mix_HaltMusic();
    return true; // Mix_HaltMusic doesn't return status. Assume success.
}

bool SDL3AudioManager::setMusicVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL3 - wrapper_client) not initialized. Cannot set music volume." << std::endl;
        return false;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (sdlVolume < 0) sdlVolume = 0;
    if (sdlVolume > MIX_MAX_VOLUME) sdlVolume = MIX_MAX_VOLUME;
    Mix_VolumeMusic(sdlVolume);
    return true; // Mix_VolumeMusic doesn't return status. Assume success.
}
