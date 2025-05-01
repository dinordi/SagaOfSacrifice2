#ifndef SDL2_AUDIO_MANAGER_H
#define SDL2_AUDIO_MANAGER_H

#include "interfaces/AudioManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <unordered_map>
#include <string>
#include <memory>

class SDL2AudioManager : public AudioManager {
private:
    std::unordered_map<std::string, Mix_Chunk*> soundEffects;
    Mix_Music* backgroundMusic;
    bool isInitialized;
    float masterVolume;
    float musicVolume;
    
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
    
private:
    std::string getFilenameFromPath(const std::string& filePath);
    void cleanup();
    std::string basePath;
};

#endif // SDL2_AUDIO_MANAGER_H