#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h> // For setenv

int main() {
    // Force SDL to use ALSA
    setenv("SDL_AUDIODRIVER", "alsa", 1);
    
    // Check if initialization with ALSA works
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize with ALSA! SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }
    
    Mix_Music *music = Mix_LoadMUS("/home/root/001.wav");
    if (!music) {
        printf("Failed to load music! SDL_mixer Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }
    
    Mix_PlayMusic(music, -1);
    SDL_Delay(5000); // Let it play 5 seconds
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}