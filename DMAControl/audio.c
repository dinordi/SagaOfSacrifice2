#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h> // For setenv
#include <stdio.h>

int main() {
    // Force SDL to use ALSA directly
    setenv("SDL_AUDIODRIVER", "alsa", 1);
    // Disable PulseAudio for ALSA
    setenv("ALSA_CARD", "Generic", 1);
    // Try to bypass PulseAudio plugin
    setenv("AUDIODEV", "hw:0,0", 1);
    
    // Check if initialization works
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    // Try using a lower sample rate and buffer size
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }
    
    printf("Audio driver in use: %s\n", SDL_GetCurrentAudioDriver());
    
    Mix_Music *music = Mix_LoadMUS("/home/root/001.wav");
    if (!music) {
        printf("Failed to load music! SDL_mixer Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }
    
    Mix_VolumeMusic(128);
    Mix_PlayMusic(music, -1);
    SDL_Delay(5000); // Let it play 5 seconds
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}