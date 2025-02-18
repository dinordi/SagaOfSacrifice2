#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <string_view>
#include <filesystem>

constexpr uint32_t windowStartWidth = 1920;
constexpr uint32_t windowStartHeight = 1080;


typedef struct entityData {
    float x;
    float y;
    int ID;
};

// Create a list of entityData
std::vector<entityData> entityList = {
    {100.0, 200.0, 1},
    {150.0, 250.0, 2},
    {200.0, 300.0, 3},
};

// A map of sprites with IDs
std::unordered_map<int, SDL_Texture*> spriteMap;

// Function to load a sprite
SDL_Texture* LoadSprite(SDL_Renderer* renderer, const std::filesystem::path& path) {
    auto surface = IMG_Load(path.string().c_str());
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load sprite: %s", path.string().c_str());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* messageTex, *imageTex;
    SDL_Rect imageDest;
    SDL_FRect messageDest;
    SDL_AudioDeviceID audioDevice;
    Mix_Music* music;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

SDL_AppResult SDL_Fail(){
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // init the library, here we make a window so we only need the Video capabilities.
    if (not SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)){
        return SDL_Fail();
    }
    
    // init TTF
    if (not TTF_Init()) {
        return SDL_Fail();
    }
    
    // create a window
   
    SDL_Window* window = SDL_CreateWindow("SDL Minimal Sample", windowStartWidth, windowStartHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (not window){
        return SDL_Fail();
    }
    
    // create a renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (not renderer){
        return SDL_Fail();
    }
    
    // load the font
#if __ANDROID__
    std::filesystem::path basePath = "";   // on Android we do not want to use basepath. Instead, assets are available at the root directory.
#else
    auto basePathPtr = SDL_GetBasePath();
     if (not basePathPtr){
        return SDL_Fail();
    }
     const std::filesystem::path basePath = basePathPtr;

    //  std::filesystem::path basePath = SDL_GetBasePath();
    std::string basePathStr = basePath.string();
    std::size_t pos = basePathStr.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        basePathStr = basePathStr.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePathSOS = std::filesystem::path(basePathStr);

#endif

    const auto fontPath = basePath / "Inter-VariableFont.ttf";
    TTF_Font* font = TTF_OpenFont(fontPath.string().c_str(), 36);
    if (not font) {
        return SDL_Fail();
    }

    // render the font to a surface
    const std::string_view text = "Hello SDL!";
    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text.data(), text.length(), { 255,255,255 });

    // make a texture from the surface
    SDL_Texture* messageTex = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

    // we no longer need the font or the surface, so we can destroy those now.
    TTF_CloseFont(font);
    SDL_DestroySurface(surfaceMessage);

    // load the SVG
    // auto svg_surface = IMG_Load((basePathSOS / "sprites/player.png").string().c_str());
    // SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, svg_surface);
    // SDL_DestroySurface(svg_surface);
    

    //Load spritemap
    spriteMap[1] = LoadSprite(renderer, basePathSOS / "SOS/sprites/playerBig.png");
    spriteMap[2] = LoadSprite(renderer, basePathSOS / "SOS/sprites/player.png");
    spriteMap[3] = LoadSprite(renderer, basePathSOS / "SOS/sprites/fatbat.png");

    for(const auto& [id, texture] : spriteMap) {
        if (!texture) {
            return SDL_Fail();
        }
    }

    // load the PNG
    auto png_surface = IMG_Load((basePathSOS / "SOS/sprites/playerBig.png").string().c_str());
    if (!png_surface) {
        return SDL_Fail();
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, png_surface);
    int png_width = png_surface->w;
    int png_height = png_surface->h;

    SDL_Rect imageDest{
        .x = 800,
        .y = 400,
        .w = png_width,
        .h = png_height
    };
    SDL_DestroySurface(png_surface);

    // get the on-screen dimensions of the text. this is necessary for rendering it
    auto messageTexProps = SDL_GetTextureProperties(messageTex);
    SDL_FRect text_rect{
            .x = 200,
            .y = 200,
            .w = float(SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0)),
            .h = float(SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0))
    };

    // init SDL Mixer
    auto audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (not audioDevice) {
        return SDL_Fail();
    }
    if (not Mix_OpenAudio(audioDevice, NULL)) {
        return SDL_Fail();
    }

    // load the music

    // Resolve the full path to the music folder
    auto musicPath = basePathSOS / "SOS/music";

    // Combine the resolved path with the music file name
    auto menuMusic = musicPath / "menu/001.mp3";
    SDL_Log("Music path: %s", menuMusic.string().c_str());
    auto music = Mix_LoadMUS(menuMusic.string().c_str());
    if (not music) {
        return SDL_Fail();
    }

    // play the music (does not loop)
    Mix_PlayMusic(music, 0);
    
    // print some information about the window
    SDL_ShowWindow(window);
    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
        SDL_Log("Window size: %ix%i", width, height);
        SDL_Log("Backbuffer size: %ix%i", bbwidth, bbheight);
        if (width != bbwidth){
            SDL_Log("This is a highdpi environment.");
        }
    }

    // set up the application data
    *appstate = new AppContext{
       .window = window,
       .renderer = renderer,
       .messageTex = messageTex,
       .imageTex = tex,
       .imageDest = imageDest,
       .messageDest = text_rect,
       .audioDevice = audioDevice,
       .music = music,
    };
    
    SDL_SetRenderVSync(renderer, -1);   // enable vysnc
    
    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    auto* app = (AppContext*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*)appstate;

    // draw a color
    auto time = SDL_GetTicks() / 1000.f;
    auto red = (std::sin(time) + 1) / 2.0 * 255;
    auto green = (std::sin(time / 2) + 1) / 2.0 * 255;
    auto blue = (std::sin(time) * 2 + 1) / 2.0 * 255;
    
    SDL_SetRenderDrawColor(app->renderer, red, green, blue, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(app->renderer);

    SDL_FRect imageDestFRect {
        .x = static_cast<float>(app->imageDest.x),
        .y = static_cast<float>(app->imageDest.y),
        .w = static_cast<float>(app->imageDest.w),
        .h = static_cast<float>(app->imageDest.h)
    };

    
    for(const auto& entity : entityList) {
        SDL_Texture* texture = spriteMap[entity.ID];
        if (!texture) {
            return SDL_APP_FAILURE;
        }
        SDL_FRect destRect{
            .x = entity.x,
            .y = entity.y,
            .w = 127,
            .h = 127
        };
        SDL_RenderTexture(app->renderer, texture, NULL, &destRect);
    }


    SDL_RenderPresent(app->renderer);

    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = (AppContext*)appstate;
    if (app) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);

        Mix_FadeOutMusic(1000);  // prevent the music from abruptly ending.
        Mix_FreeMusic(app->music); // this call blocks until the music has finished fading
        Mix_CloseAudio();
        SDL_CloseAudioDevice(app->audioDevice);

        delete app;
    }
    TTF_Quit();
    Mix_Quit();

    SDL_Log("Application quit successfully!");
    SDL_Quit();
}
