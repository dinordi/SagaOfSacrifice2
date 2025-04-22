#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

int main() {
    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
    Mix_Music *music = Mix_LoadMUS("001.wav");
    Mix_PlayMusic(music, -1);
    SDL_Delay(5000); // Let it play 5 seconds
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}