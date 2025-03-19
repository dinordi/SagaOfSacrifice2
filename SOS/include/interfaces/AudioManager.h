#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <string>

class AudioManager {
public:
    virtual ~AudioManager() = default;

    virtual void initialize() = 0;
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

#endif // AUDIO_MANAGER_H