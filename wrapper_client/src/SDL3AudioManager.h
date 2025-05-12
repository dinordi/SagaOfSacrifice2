#ifndef SDL3_AUDIO_MANAGER_H
#define SDL3_AUDIO_MANAGER_H

#include "interfaces/AudioManager.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <map>

class SDL3AudioManager : public AudioManager {
public:
    SDL3AudioManager();
    ~SDL3AudioManager() override;

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

private:
    std::string mBasePath;
    std::map<std::string, Mix_Chunk*> mSoundEffects;
    Mix_Music* mMusic;
    bool mInitialized;
};

#endif // SDL3_AUDIO_MANAGER_H
