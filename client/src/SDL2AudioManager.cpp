#include "SDL2AudioManager.h" 
#include <SDL2/SDL.h> // Added for SDL_Init, SDL_QuitSubSystem, etc.
#include <SDL2/SDL_mixer.h>
#include <iostream> 

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
        SDL_QuitSubSystem(SDL_INIT_AUDIO); // Quit SDL audio subsystem
    }
}

bool SDL2AudioManager::initialize(const std::string& basePath) {
    mBasePath = basePath;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL (version 2) audio subsystem could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        mInitialized = false;
        return mInitialized;
    }

    // Initialize SDL_mixer for MP3 and OGG support
    int mixFlags = MIX_INIT_MP3 | MIX_INIT_OGG;
    int initializedMixFlags = Mix_Init(mixFlags);
    if ((initializedMixFlags & mixFlags) != mixFlags) {
        std::cerr << "Failed to initialize SDL2_mixer with all requested flags (MP3, OGG)! Mix_Error: " << Mix_GetError() << std::endl;
        if (initializedMixFlags == 0) {
            std::cerr << "SDL2_mixer basic support also failed! Mix_Error: " << Mix_GetError() << std::endl;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            mInitialized = false;
            return mInitialized;
        }
        std::cerr << "SDL2_mixer initialized with partial format support." << std::endl;
    }
    
    // Open audio device
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL2_mixer could not open audio! Mix_Error: " << Mix_GetError() << std::endl;
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        mInitialized = false;
        return mInitialized;
    }

    mInitialized = true;
    std::cout << "SDL2AudioManager initialized successfully." << std::endl;
    return mInitialized;
}

bool SDL2AudioManager::loadSound(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot load sound." << std::endl;
        return false;
    }
    std::string fullPath = mBasePath + "/" + filePath;
    Mix_Chunk* sound = Mix_LoadWAV(fullPath.c_str());
    if (!sound) {
        std::cerr << "Failed to load sound effect (SDL2)! Path: " << fullPath << " Mix_Error: " << Mix_GetError() << std::endl;
        return false;
    }
    std::string soundName = getSoundName(filePath);
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        Mix_FreeChunk(it->second);
    }
    mSoundEffects[soundName] = sound;
    std::cout << "Loaded sound (SDL2): " << soundName << " from " << fullPath << std::endl;
    return true;
}

bool SDL2AudioManager::playSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot play sound." << std::endl;
        return false;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        if (Mix_PlayChannel(-1, it->second, 0) == -1) {
            std::cerr << "Failed to play sound (SDL2): " << soundName << " Mix_Error: " << Mix_GetError() << std::endl;
            return false;
        }
        return true;
    } else {
        std::cerr << "Sound not found (SDL2): " << soundName << std::endl;
        return false;
    }
}

bool SDL2AudioManager::stopSound(const std::string& soundName) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot stop sound." << std::endl;
        return false;
    }
    auto it = mSoundEffects.find(soundName);
    if (it != mSoundEffects.end() && it->second) {
        bool soundActiveOnChannel = false;
        for (int i = 0; i < Mix_AllocateChannels(-1); ++i) {
            if (Mix_Playing(i) && Mix_GetChunk(i) == it->second) {
                Mix_HaltChannel(i);
                soundActiveOnChannel = true; 
            }
        }
        return true; 
    } else {
        std::cerr << "Sound not found (SDL2), cannot stop: " << soundName << std::endl;
        return false;
    }
}

bool SDL2AudioManager::setVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot set volume." << std::endl;
        return false;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    sdlVolume = std::max(0, std::min(sdlVolume, MIX_MAX_VOLUME));
    Mix_Volume(-1, sdlVolume);
    return true;
}

bool SDL2AudioManager::loadMusic(const std::string& filePath) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot load music." << std::endl;
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
        std::cerr << "Failed to load music (SDL2)! Path: " << fullPath << " Mix_Error: " << Mix_GetError() << std::endl;
        return false;
    } else {
        std::cout << "Loaded music (SDL2): " << filePath << std::endl;
        return true;
    }
}

bool SDL2AudioManager::playMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot play music." << std::endl;
        return false;
    }
    if (mMusic) {
        if (Mix_PlayingMusic() == 0) {
            if (Mix_PlayMusic(mMusic, -1) == -1) {
                std::cerr << "Failed to play music (SDL2)! Mix_Error: " << Mix_GetError() << std::endl;
                return false;
            }
        } else if (Mix_PausedMusic() == 1) {
            Mix_ResumeMusic();
        }
        return true;
    } else {
        std::cerr << "No music loaded to play (SDL2)." << std::endl;
        return false;
    }
}

bool SDL2AudioManager::pauseMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot pause music." << std::endl;
        return false;
    }
    if (Mix_PlayingMusic() == 1 && Mix_PausedMusic() == 0) {
        Mix_PauseMusic();
    }
    return true;
}

bool SDL2AudioManager::stopMusic() {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot stop music." << std::endl;
        return false;
    }
    Mix_HaltMusic();
    return true;
}

bool SDL2AudioManager::setMusicVolume(float volume) {
    if (!mInitialized) {
        std::cerr << "AudioManager (SDL2) not initialized. Cannot set music volume." << std::endl;
        return false;
    }
    int sdlVolume = static_cast<int>(volume * MIX_MAX_VOLUME);
    sdlVolume = std::max(0, std::min(sdlVolume, MIX_MAX_VOLUME));
    Mix_VolumeMusic(sdlVolume);
    return true;
}
