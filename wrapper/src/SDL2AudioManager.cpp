#include "SDL2AudioManager.h"
#include <SDL2/SDL_mixer.h> // Correct include for SDL2_mixer
#include <iostream> // For std::cerr

// Helper function to extract sound name from file path
static std::string getSoundName(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string name = (lastSlash == std::string::npos) ? filePath : filePath.substr(lastSlash + 1);
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        name = name.substr(0, lastDot);
    }
    return name;
}

SDL2AudioManager::SDL2AudioManager() : mMusic(nullptr), mInitialized(false) {
    // Constructor
}

SDL2AudioManager::~SDL2AudioManager() {
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

void SDL2AudioManager::initialize(const std::string& basePath) {
    mBasePath = basePath;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL (version 2) could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        mInitialized = false;
        return;
    }

    // Initialize SDL_mixer
    // Using common formats like MP3 and OGG. Add more if needed (e.g., MIX_INIT_FLAC).
    if (Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG) == 0) {
        std::cerr << "Failed to initialize SDL2_mixer (MP3 and OGG support)! Mix_Error: " << Mix_GetError() << std::endl;
        // Try initializing with no specific flags if the above fails
        if (Mix_Init(0) == 0) {
             std::cerr << "Failed to initialize SDL2_mixer (basic support)! Mix_Error: " << Mix_GetError() << std::endl;
             SDL_Quit();
             mInitialized = false;
             return;
        }
    }
    
    // Open audio device
    // Parameters: frequency, format, channels, chunksize
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL2_mixer could not open audio! Mix_Error: " << Mix_GetError() << std::endl;
        Mix_Quit();
        SDL_Quit();
        mInitialized = false;
        return;
    }

    mInitialized = true;
    std::cout << "SDL2AudioManager initialized successfully." << std::endl;
}

void SDL2AudioManager::loadSound(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot load sound." << std::endl;
        return;
    }
    std::string fullPath = mBasePath + "/" + filePath;
    Mix_Chunk* sound = Mix_LoadWAV(fullPath.c_str());
    if (!sound) {
        std::cerr << "Failed to load sound effect (SDL2)! Path: " << fullPath << " Mix_Error: " << Mix_GetError() << std::endl;
        return;
    }
    std::string soundName = getSoundName(filePath);
    mSoundEffects[soundName] = sound;
    std::cout << "Loaded sound (SDL2): " << soundName << " from " << fullPath << std::endl;
}

void SDL2AudioManager::playSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot play sound." << std::endl;
        return;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        if (Mix_PlayChannel(-1, it->second, 0) == -1) {
            std::cerr << "Failed to play sound (SDL2): " << soundName << " Mix_Error: " << Mix_GetError() << std::endl;
        }
    } else {
        std::cerr << "Sound not found (SDL2): " << soundName << std::endl;
    }
}

void SDL2AudioManager::stopSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot stop sound." << std::endl;
        return;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        // Iterate through all active channels
        for (int i = 0; i < Mix_AllocateChannels(-1); ++i) {
            if (Mix_Playing(i) && Mix_GetChunk(i) == it->second) {
                Mix_HaltChannel(i);
            }
        }
    } else {
        std::cerr << "Sound not found (SDL2), cannot stop: " << soundName << std::endl;
    }
}

void SDL2AudioManager::setVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot set volume." << std::endl;
        return;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (sdlVolume < 0) sdlVolume = 0;
    if (sdlVolume > MIX_MAX_VOLUME) sdlVolume = MIX_MAX_VOLUME;
    Mix_Volume(-1, sdlVolume); 
}

void SDL2AudioManager::loadMusic(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot load music." << std::endl;
        return;
    }
    std::string fullPath = mBasePath + "/" + filePath;
    if (mMusic) {
        Mix_HaltMusic();
        Mix_FreeMusic(mMusic);
        mMusic = nullptr;
    }
    mMusic = Mix_LoadMUS(fullPath.c_str());
    if (!mMusic) {
        std::cerr << "Failed to load music (SDL2)! Path: " << fullPath << " Mix_Error: " << Mix_GetError() << std::endl;
    } else {
        std::cout << "Loaded music (SDL2): " << filePath << std::endl;
    }
}

void SDL2AudioManager::playMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot play music." << std::endl;
        return;
    }
    if (mMusic) {
        if (Mix_PlayingMusic() == 0) {
            if (Mix_PlayMusic(mMusic, -1) == -1) {
                std::cerr << "Failed to play music (SDL2)! Mix_Error: " << Mix_GetError() << std::endl;
            }
        } else if (Mix_PausedMusic() == 1) {
            Mix_ResumeMusic();
        }
    } else {
        std::cerr << "No music loaded to play (SDL2)." << std::endl;
    }
}

void SDL2AudioManager::pauseMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot pause music." << std::endl;
        return;
    }
    if (Mix_PlayingMusic() == 1) {
        Mix_PauseMusic();
    }
}

void SDL2AudioManager::stopMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot stop music." << std::endl;
        return;
    }
    Mix_HaltMusic();
}

void SDL2AudioManager::setMusicVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot set music volume." << std::endl;
        return;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (sdlVolume < 0) sdlVolume = 0;
    if (sdlVolume > MIX_MAX_VOLUME) sdlVolume = MIX_MAX_VOLUME;
    Mix_VolumeMusic(sdlVolume);
}
