#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <string>

class AudioManager {
public:
    virtual ~AudioManager() = default;

    virtual bool initialize(const std::string& basePath) = 0;
    virtual bool loadSound(const std::string& filePath) = 0;
    virtual bool playSound(const std::string& soundName) = 0;
    virtual bool stopSound(const std::string& soundName) = 0;
    virtual bool isSfxPlaying(const std::string& soundName) = 0;
    virtual bool unloadSound(const std::string& soundName) = 0;
    virtual bool setVolume(float volume) = 0;

    virtual bool loadMusic(const std::string& filePath) = 0;
    virtual bool playMusic() = 0;
    virtual bool pauseMusic() = 0;
    virtual bool stopMusic() = 0;
    virtual bool isMusicPlaying() const = 0;
    virtual bool setMusicVolume(float volume) = 0;

    // Singleton accessor
    static AudioManager& Instance() { return *instance_; }
    static void SetInstance(AudioManager* inst) { instance_ = inst; }
private:
    static AudioManager* instance_;
};

#endif // AUDIO_MANAGER_H