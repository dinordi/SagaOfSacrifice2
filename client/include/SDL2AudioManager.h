#ifndef SDL2_AUDIO_MANAGER_H
#define SDL2_AUDIO_MANAGER_H

#include "interfaces/AudioManager.h"
#include <SDL2/SDL_mixer.h> 
#include <string>
#include <map>

class SDL2AudioManager : public AudioManager {
public:
    // Singleton accessor
    static SDL2AudioManager& Instance() {
        static SDL2AudioManager instance;
        return instance;
    }

    SDL2AudioManager(const SDL2AudioManager&) = delete;
    SDL2AudioManager& operator=(const SDL2AudioManager&) = delete;

    ~SDL2AudioManager() override;

    bool initialize(const std::string& basePath) override;
    bool loadSound(const std::string& filePath) override;
    bool unloadSound(const std::string& soundName) override;
    bool playSound(const std::string& soundName) override;
    bool stopSound(const std::string& soundName) override;
    bool isSfxPlaying(const std::string& soundName) override;
    bool setVolume(float volume) override;
    bool setSfxVolume(float volume, const std::string& soundName) override;

    bool loadMusic(const std::string& filePath) override;
    bool playMusic() override;
    bool pauseMusic() override;
    bool stopMusic() override;
    bool isMusicPlaying() const override;
    bool setMusicVolume(float volume) override;

private:
    SDL2AudioManager();

    std::string mBasePath;
    std::map<std::string, Mix_Chunk*> mSoundEffects;
    Mix_Music* mMusic;
    bool mInitialized;
};

#endif // SDL2_AUDIO_MANAGER_H