#ifndef SDL2_AUDIO_MANAGER_H
#define SDL2_AUDIO_MANAGER_H

#include "interfaces/AudioManager.h"
#include <SDL2/SDL_mixer.h> 
#include <string>
#include <map>

class SDL2AudioManager : public AudioManager {
public:
    SDL2AudioManager();
    ~SDL2AudioManager() override;

    bool initialize(const std::string& basePath) override;
    bool loadSound(const std::string& filePath) override;
    bool playSound(const std::string& soundName) override;
    bool stopSound(const std::string& soundName) override;
    bool setVolume(float volume) override;

    bool loadMusic(const std::string& filePath) override;
    bool playMusic() override;
    bool pauseMusic() override;
    bool stopMusic() override;
    bool setMusicVolume(float volume) override;

private:
    std::string mBasePath;
    std::map<std::string, Mix_Chunk*> mSoundEffects;
    Mix_Music* mMusic;
    bool mInitialized;
};

#endif // SDL2_AUDIO_MANAGER_H
