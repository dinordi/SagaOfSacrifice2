#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <string_view>
#include <filesystem>
#include <string> // Added for string manipulation
#include <cstdlib> // Added for atoi and rand
#include <vector> // Added for vector usage if needed indirectly
#include <iostream> // Added for std::cerr, std::cout

#include "game.h"
#include "sprites_sdl.h"
#include "logger_sdl.h"
#include "sdl_input.h"


constexpr uint32_t windowStartWidth = 1920;
constexpr uint32_t windowStartHeight = 1080;

SDL_Gamepad* getGamepad();


struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* messageTex, *backgroundTex;
    SDL_Rect imageDest;
    SDL_AudioDeviceID audioDevice;
    Mix_Music* music;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
    Game* game;
    std::filesystem::path basePathSOS;
};

SDL_AppResult SDL_Fail(){
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

// Helper function from SOS/main.cpp
std::string generateRandomPlayerId() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int idLength = 8;
    std::string result;
    result.reserve(idLength);

    // Seed random number generator
    // srand(time(NULL)); // Requires <ctime>
    
    for (int i = 0; i < idLength; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }

    return result;
}


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // --- Multiplayer Argument Parsing ---
    bool enableMultiplayer = false;
    std::string serverAddress = "localhost";
    int serverPort = 8080;
    std::string playerId = generateRandomPlayerId(); // Generate default random ID

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Note: No -h/--help here as SDL might handle it, or it's less common in SDL apps
        if (arg == "-m" || arg == "--multiplayer") {
            enableMultiplayer = true;
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                serverAddress = argv[++i];
            } else {
                 SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing argument for %s option.", arg.c_str());
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    serverPort = std::stoi(argv[++i]);
                    if (serverPort <= 0 || serverPort > 65535) {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Invalid port number '%s'. Using default port %d.", argv[i], 8080);
                        serverPort = 8080;
                    }
                } catch (const std::invalid_argument& e) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Invalid port argument '%s'. Using default port %d.", argv[i], 8080);
                    serverPort = 8080;
                } catch (const std::out_of_range& e) {
                     SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Port argument '%s' out of range. Using default port %d.", argv[i], 8080);
                    serverPort = 8080;
                }
            } else {
                 SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing argument for %s option.", arg.c_str());
            }
        } 
        else if (arg == "-id" || arg == "--playerid") {
            if (i + 1 < argc) {
                playerId = argv[++i];
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing argument for %s option.", arg.c_str());
            }
        } 
        else {
            // Handle other arguments if necessary, e.g., image name from original main.cpp
             SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown option: %s", arg.c_str());
        }
    }


    // init the library
    if (not SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)){
        return SDL_Fail();
    }

    // init TTF
    // Probably not needed for our project, but keeping it in since it was in the original code
    if (not TTF_Init()) {
        return SDL_Fail();
    }

    // create a window
    SDL_Window* window = SDL_CreateWindow("Saga Of Sacrifice 2", windowStartWidth, windowStartHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (not window){
        return SDL_Fail();
    }

    // create a renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (not renderer){
        return SDL_Fail();
    }

    // Get base path
#if __ANDROID__
    std::filesystem::path basePath = "";   // on Android we do not want to use basepath. Instead, assets are available at the root directory.
#else
    auto basePathPtr = SDL_GetBasePath();
     if (not basePathPtr){
        return SDL_Fail();
    }
     const std::filesystem::path basePath = basePathPtr;

    std::string basePathStr = basePath.string();
    std::size_t pos = basePathStr.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        basePathStr = basePathStr.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePathSOS = std::filesystem::path(basePathStr);
#endif

    Logger::setInstance(new LoggerSDL());

    // FONT STUFF
    const auto fontPath = basePath / "Inter-VariableFont.ttf";
    TTF_Font* font = TTF_OpenFont(fontPath.string().c_str(), 36);
    if (not font) {
        return SDL_Fail();
    }

    // render the font to a surface
    const std::string_view text = "Welcome to Saga Of Sacrifice 2!";
    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text.data(), text.length(), { 255,255,255 });
    
    // make a texture from the surface
    SDL_Texture* messageTex = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
    // we no longer need the font or the surface, so we can destroy those now.
    TTF_CloseFont(font);
    SDL_DestroySurface(surfaceMessage);
    // get the on-screen dimensions of the text. this is necessary for rendering it
    auto messageTexProps = SDL_GetTextureProperties(messageTex);
    SDL_FRect text_rect{
            .x = 200,
            .y = 200,
            .w = float(SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0)),
            .h = float(SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0))
    };
    
    //END FONT STUFF


    //Load sprites from PNG into memory
    loadAllSprites(renderer, basePathSOS);


    // init SDL Mixer
    auto audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (not audioDevice) {
        return SDL_Fail();
    }
    if (not Mix_OpenAudio(audioDevice, NULL)) {
        return SDL_Fail();
    }

    // Resolve the full path to the music folder
    auto musicPath = (basePathSOS / "SOS/assets/music").make_preferred();

    // Combine the resolved path with the music file name
    auto menuMusic = (musicPath / "menu/001.mp3").make_preferred();
    SDL_Log("Music path: %s", menuMusic.string().c_str());
    auto music = Mix_LoadMUS(menuMusic.string().c_str());
    if (not music) {
        return SDL_Fail();
    }

    // play the music (does not loop)
    // Mix_PlayMusic(music, 0);


    Logger::getInstance()->log("Logger started successfully!");

    PlayerInput* input = new SDLInput(getGamepad());

    //Load game
    Game* game = new Game(input, playerId);
    // --- Initialize Multiplayer ---
    if (enableMultiplayer) {
        if (!game->initializeServerConnection(serverAddress, serverPort, playerId)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize multiplayer. Continuing in single player mode.");
            return SDL_APP_FAILURE;
        } else {
            SDL_Log("Multiplayer initialized successfully!");
        }
    }
    else
    {
        // Initialize single player mode with local server
        if (!game->initializeSinglePlayerEmbeddedServer()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize single player mode.");
            return SDL_APP_FAILURE;
        } else {
            SDL_Log("Single player mode initialized successfully!");
        }
    }
    // --- End Initialize Multiplayer ---


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

    // Load the background image
    auto backgroundPath = (basePathSOS / "SOS/assets/backgrounds/background.png").make_preferred();
    SDL_Surface* backgroundSurface = IMG_Load(backgroundPath.string().c_str());
    if (!backgroundSurface) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load background image: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Texture* backgroundTex = SDL_CreateTextureFromSurface(renderer, backgroundSurface);
    SDL_DestroySurface(backgroundSurface); // Free the surface after creating the texture
    if (!backgroundTex) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to create background texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // set up the application data
    *appstate = new AppContext{
       .window = window,
       .renderer = renderer,
       .messageTex = messageTex,
       .backgroundTex = backgroundTex,
       .audioDevice = audioDevice,
       .music = music,
       .game = game,
       .basePathSOS = basePathSOS,
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

    // Pass event to input handler if needed (SDLInput might handle events internally or need polling)
    // Example: if (app->game) { static_cast<SDLInput*>(app->game->getInput())->handleEvent(event); }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*)appstate;


    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(app->renderer);

    SDL_RenderTexture(app->renderer, app->backgroundTex, NULL, NULL);

    // static Uint64 lastIDPrint = 0;
    // bool printID = false;
    // Uint64 printtime = SDL_GetTicks();
    // if (printtime - lastIDPrint > 1000) {
    //     printID = true;
    //     lastIDPrint = printtime;
    // }

    //Load game objects (Entities, player(s), platforms)
    for(const auto& entity : app->game->getObjects()) {
        if (!entity || !entity->spriteData) continue; // Basic safety check
        SDL_Texture* sprite_tex = nullptr;
        // if(printID)
        // {
        //     SDL_Log("Entity ID: %s", entity->spriteData->getid_().c_str());
        // }
        // Get the pre-loaded sprite info from the map
        auto it = spriteMap2.find(entity->spriteData->getid_());
        if (it == spriteMap2.end()) {
             SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Sprite ID %s not found in spriteMap for Object", entity->spriteData->getid_().c_str());
            //  continue; // Skip rendering if sprite not found
            sprite_tex = spriteMap2["fatbat.png"];
        }
        else
        {
            sprite_tex = it->second; // Use the existing texture from the map
        }

        // Use the current sprite index from animation system
        int spriteIndex = entity->getCurrentSpriteIndex();
        const SpriteRect& currentSpriteRect = entity->spriteData->getSpriteRect(spriteIndex);
        
        SDL_FRect srcRect = {
            static_cast<float>(currentSpriteRect.x),
            static_cast<float>(currentSpriteRect.y),
            static_cast<float>(currentSpriteRect.w),
            static_cast<float>(currentSpriteRect.h)
        };

        bool isFacingRight = entity->getDir() == FacingDirection::RIGHT;
        SDL_FRect destRect{
            .x = static_cast<float>(entity->getposition().x),
            .y = static_cast<float>(entity->getposition().y),
            .w = static_cast<float>(currentSpriteRect.w * (isFacingRight ? 1.0f : -1.0f)),
            .h = static_cast<float>(currentSpriteRect.h)
        };

        // Adjust the dest rect's position when flipped
        if (!isFacingRight) {
            destRect.x -= destRect.w; // Adjust x position when flipped
        }

        // Normal rendering without flip
        SDL_RenderTexture(app->renderer, sprite_tex, &srcRect, &destRect);
        
    }

    for(const auto& actor : app->game->getActors()) {
         if (!actor || !actor->spriteData) continue; // Basic safety check

        // Get the pre-loaded sprite info for the character texture (ID 11)
        auto it_char_tex = spriteMap2.find(actor->spriteData->getid_()); // Should be ID 11 for letters
         if (it_char_tex == spriteMap2.end()) {
             SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Sprite ID %s not found in spriteMap for Actor", actor->spriteData->getid_().c_str());
             continue;
         }
        SDL_Texture* sprite_tex = it_char_tex->second; // Base texture for letters.png

        // Get the specific source rect for this character index
        const SpriteRect& charSpriteRect = actor->spriteData->getSpriteRect(actor->spriteIndex);
        SDL_FRect srcRect = {
            static_cast<float>(charSpriteRect.x),
            static_cast<float>(charSpriteRect.y),
            static_cast<float>(charSpriteRect.w),
            static_cast<float>(charSpriteRect.h)
        };


        SDL_FRect destRect{
            .x = static_cast<float>(actor->getposition().x),
            .y = static_cast<float>(actor->getposition().y),
            .w = static_cast<float>(charSpriteRect.w),
            .h = static_cast<float>(charSpriteRect.h)
        };
        SDL_RenderTexture(app->renderer, sprite_tex, &srcRect, &destRect); // Use pre-loaded texture
    }

    // If 16ms has passed since the last update, update the game
    static Uint64 lastUpdate = 0;

    Uint64 currentTime = SDL_GetTicks();
    Uint64 deltaTime = currentTime - lastUpdate;

    // Simple delta time clamping
    const Uint64 maxDeltaTime = 33; // ~30 FPS minimum update rate
    if (deltaTime > maxDeltaTime) {
        // SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Large delta time detected: %llu ms. Clamping to %llu ms.", deltaTime, maxDeltaTime);
        deltaTime = maxDeltaTime;
    }


    if (app->game) {
        app->game->update(deltaTime); // Update game logic
    }
    lastUpdate = currentTime;
    
    SDL_RenderPresent(app->renderer);

    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = (AppContext*)appstate;
    if (app) {
        // --- Shutdown Multiplayer ---
        // if (app->game && app->game->isMultiplayerActive()) {
        //     app->game->shutdownMultiplayer();
        // }
        // --- End Shutdown Multiplayer ---

        // Clean up game object
        delete app->game; // Delete the game instance

        // Clean up SDL resources
        SDL_DestroyTexture(app->messageTex);
        SDL_DestroyTexture(app->backgroundTex);
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);

        // Clean up audio
        Mix_FadeOutMusic(1000);  // prevent the music from abruptly ending.
        Mix_FreeMusic(app->music); // this call blocks until the music has finished fading
        Mix_CloseAudio();
        SDL_CloseAudioDevice(app->audioDevice);

        // Clean up input (if dynamically allocated in AppInit)
        // delete static_cast<SDLInput*>(app->game->getInput()); // Be careful with ownership

        delete app; // Delete the context itself
    }
    TTF_Quit();
    Mix_Quit();
    // IMG_Quit(); // Added IMG_Quit

    SDL_Log("Application quit with result %d.", result);
    SDL_Quit();
}


SDL_Gamepad* getGamepad()
{
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID targetInstanceID = 0; // SDL_InstanceID is Uint32, 0 is invalid/none

    // 1. Get the list of joystick instance IDs
    int count = 0;
    SDL_JoystickID* joystick_ids = SDL_GetJoysticks(&count); // SDL_GetJoysticks allocates memory

    if (joystick_ids == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Could not get joysticks! SDL Error: %s", SDL_GetError());
        count = 0; // Ensure count is 0 if allocation failed
    }

    SDL_Log("Found %d joystick(s).", count);

    // 2. Iterate through instance IDs and check if they are gamepads
    for (int i = 0; i < count; ++i) {
        SDL_JoystickID current_id = joystick_ids[i];
        const char* name = SDL_GetJoystickNameForID(current_id); // Get name even before opening
        SDL_Log("Checking device ID %u: %s", current_id, (name ? name : "Unknown Name"));

        if (SDL_IsGamepad(current_id)) { // Use SDL_IsGamepad
            SDL_Log("  -> Device ID %u is a recognized gamepad.", current_id);

            // Found a gamepad, let's target this one
            targetInstanceID = current_id;
            break; // Stop searching, we'll open the first one found
        } else {
             SDL_Log("  -> Device ID %u is NOT a recognized gamepad.", current_id);
        }
    }

    // 3. Free the array returned by SDL_GetJoysticks
    SDL_free(joystick_ids);
    joystick_ids = nullptr; // Good practice to null the pointer after freeing


    // 4. If we found a suitable Instance ID, try to open it
    if (targetInstanceID != 0) { // Check against the invalid ID 0
        gamepad = SDL_OpenGamepad(targetInstanceID);
        if (!gamepad) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Could not open gamepad with ID %u! SDL Error: %s", targetInstanceID, SDL_GetError());
        } else {
            // Get the name from the opened gamepad handle (preferred method)
            const char* gamepadName = SDL_GetGamepadName(gamepad);
            SDL_Log("Gamepad connected: %s (ID: %u)", (gamepadName ? gamepadName : "Unknown Name"), targetInstanceID);
        }
    } else {
         SDL_Log("No recognized gamepad found.");
    }

    return gamepad; // Return the opened gamepad or nullptr if none found
}