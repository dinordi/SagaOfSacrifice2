#ifndef SDL3_AUDIO_MANAGER_H
#define SDL3_AUDIO_MANAGER_H

#include "interfaces/AudioManager.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <map>

class SDL3AudioManager : public AudioManager {
public:
    // Singleton accessor
    static SDL3AudioManager& Instance() {
        static SDL3AudioManager instance;
        return instance;
    }

    // Delete copy/move constructors and assignment operators
    SDL3AudioManager(const SDL3AudioManager&) = delete;
    SDL3AudioManager& operator=(const SDL3AudioManager&) = delete;

    bool initialize(const std::string& basePath) override;
    bool loadSound(const std::string& filePath) override;
    bool unloadSound(const std::string& soundName) override;
    bool playSound(const std::string& soundName) override;
    bool stopSound(const std::string& soundName) override;
    bool setVolume(float volume) override;

    bool loadMusic(const std::string& filePath) override;
    bool playMusic() override;
    bool pauseMusic() override;
    bool stopMusic() override;
    bool setMusicVolume(float volume) override;

private:
    SDL3AudioManager();
    ~SDL3AudioManager() override;

    std::string mBasePath;
    std::map<std::string, Mix_Chunk*> mSoundEffects;
    Mix_Music* mMusic;
    bool mInitialized;
};

#endif // SDL3_AUDIO_MANAGER_H