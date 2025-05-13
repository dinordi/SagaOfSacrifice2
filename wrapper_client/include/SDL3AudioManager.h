#ifndef SDL3AUDIOMANAGER_H
#define SDL3AUDIOMANAGER_H

#include "interfaces/AudioManager.h" // Assuming this is the correct path to the interface
#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <vector>
#include <map>
#include <iostream> // For std::cout, std::cerr

class SDL3AudioManager : public AudioManager {
public:
    SDL3AudioManager(const std::string& basePath);
    virtual ~SDL3AudioManager();

    bool initialize(int frequency = 44100, Uint16 format = MIX_DEFAULT_FORMAT, int channels = 2, int chunksize = 2048) override;
    void shutdown() override;

    bool loadSound(const std::string& filePath) override; // Changed signature
    bool playSound(const std::string& soundName) override;
    bool stopSound(const std::string& soundName) override; // Added to match potential interface
    bool setVolume(float volume) override; // General sound effects volume

    bool loadMusic(const std::string& filePath) override;
    bool playMusic() override;
    bool pauseMusic() override;
    bool stopMusic() override;
    bool setMusicVolume(float volume) override;

    // Optional: Add methods to check if sound/music is playing, etc.
    // bool isSoundPlaying(const std::string& soundName) const;
    // bool isMusicPlaying() const;

private:
    std::string mBasePath;
    bool mInitialized;
    std::map<std::string, Mix_Chunk*> mSoundEffects;
    Mix_Music* mMusic;
    // Store the parameters used for initialization if needed for re-initialization or checks
    int mFrequency;
    Uint16 mFormat;
    int mChannels;
    int mChunksize;
};

#endif // SDL3AUDIOMANAGER_H
