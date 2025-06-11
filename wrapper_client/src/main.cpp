#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3_ttf/SDL_ttf.h>
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
#include "SDL3AudioManager.h"
#include "graphics/Camera.h"

constexpr uint32_t windowStartWidth = 1920;
constexpr uint32_t windowStartHeight = 1080;

SDL_Gamepad* getGamepad();


struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* messageTex, *backgroundTex;
    // Parallax layers
    SDL_Texture* parallaxLayer1Tex = nullptr;
    SDL_Texture* parallaxLayer2Tex = nullptr;
    SDL_Rect imageDest;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
    Uint64 fixed_timestep_us = 16666; // For ~60 updates per second (1,000,000 microseconds / 60)
                                      // You can adjust this (e.g., 33333 for 30 UPS)
    Uint64 accumulator_us = 0;
    Uint64 last_time_us = 0; // Will be initialized on the first run of AppIterate
    Game& game;
    std::filesystem::path basePathSOS;
    Camera* camera;
    
    // Multiplayer configuration
    bool enableMultiplayer;
    std::string serverAddress;
    int serverPort;
    std::string playerId;
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
        else if (arg == "-h" || arg == "--help") {
            SDL_Log("Usage: %s [options]\n", argv[0]);
            SDL_Log("Options:\n");
            SDL_Log("  -m, --multiplayer        Enable multiplayer mode\n");
            SDL_Log("  -s, --server <address>   Set server address (default: localhost)\n");
            SDL_Log("  -p, --port <port>       Set server port (default: 8080)\n");
            SDL_Log("  -id, --playerid <id>    Set player ID (default: random)\n");
            SDL_Log("  -h, --help              Show this help message\n");
            return SDL_APP_FAILURE;
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
    SDL_SetWindowSize(window, windowStartWidth, windowStartHeight);


    // create a renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (not renderer){
        return SDL_Fail();
    }
    SDL_SetRenderLogicalPresentation(renderer, 1920, 1080, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

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


    // play the music (does not loop)
    // Mix_PlayMusic(music, 0);


    Logger::getInstance()->log("Logger started successfully!");

    PlayerInput* input = new SDLInput(getGamepad());    // Initialize AudioManager
    AudioManager& audio = SDL3AudioManager::Instance();
    // Set the global AudioManager instance to point to our SDL3AudioManager implementation
    AudioManager::SetInstance(&audio);
    
    if(audio.initialize(basePathSOS.string())){
        SDL_Log("AudioManager initialized successfully!");
        // audio.loadMusic("SOS/assets/music/menu/001.mp3");
        // audio.loadSound("SOS/assets/sfx/walking.mp3");
        // audio.playMusic();

    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize AudioManager.");
        return SDL_APP_FAILURE;
    }
    //Load game
    Game& game = Game::getInstance();
    game.setPlayerInput(input);
    
    // Initialize server configuration
    game.initializeServerConfig(basePathSOS.string());
    
    // Set multiplayer configuration for later use when menu option is selected
    game.setMultiplayerConfig(enableMultiplayer, serverAddress, serverPort);
    
    SDL_SetWindowSize(window, 1920, 1080);
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
    auto backgroundPath = (basePathSOS / "SOS/assets/backgrounds/background2.png").make_preferred();
    SDL_Surface* backgroundSurface = IMG_Load(backgroundPath.string().c_str());
    if (!backgroundSurface) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load background image: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Texture* backgroundTex = SDL_CreateTextureFromSurface(renderer, backgroundSurface);
    SDL_DestroySurface(backgroundSurface);
    if (!backgroundTex) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to create background texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    // Load parallax layers
    SDL_Texture* parallaxLayer1Tex = nullptr;
    SDL_Texture* parallaxLayer2Tex = nullptr;
    {
        auto layer1Path = (basePathSOS / "SOS/assets/backgrounds/platform.png").make_preferred();
        SDL_Surface* layer1Surface = IMG_Load(layer1Path.string().c_str());
        if (layer1Surface) {
            parallaxLayer1Tex = SDL_CreateTextureFromSurface(renderer, layer1Surface);
            SDL_DestroySurface(layer1Surface);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM, "Could not load parallax layer 1: %s", SDL_GetError());
        }
        auto layer2Path = (basePathSOS / "SOS/assets/backgrounds/islands2.png").make_preferred();
        SDL_Surface* layer2Surface = IMG_Load(layer2Path.string().c_str());
        if (layer2Surface) {
            parallaxLayer2Tex = SDL_CreateTextureFromSurface(renderer, layer2Surface);
            SDL_DestroySurface(layer2Surface);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM, "Could not load parallax layer 2: %s", SDL_GetError());
        }
    }

    // Initialize camera
    Camera* camera = new Camera(windowStartWidth, windowStartHeight);
    // set up the application data
    *appstate = new AppContext{
       .window = window,
       .renderer = renderer,
       .messageTex = messageTex,
       .backgroundTex = backgroundTex,
       .parallaxLayer1Tex = parallaxLayer1Tex,
       .parallaxLayer2Tex = parallaxLayer2Tex,
       .game = game,
       .basePathSOS = basePathSOS,
       .camera = camera,
       .enableMultiplayer = enableMultiplayer,
       .serverAddress = serverAddress,
       .serverPort = serverPort,
       .playerId = playerId,
    };

    SDL_SetRenderVSync(renderer, 0);   // enable vysnc

    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    auto* app = (AppContext*)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_APP_SUCCESS;
    }

    // Pass event to input handler if needed (SDLInput might handle events internally or need polling)
    // Example: if (app->game) { static_cast<SDLInput*>(app->game.getInput())->handleEvent(event); }

    return SDL_APP_CONTINUE;
}

void updateCamera(AppContext* app) {
    // Find player object to center camera on
    std::shared_ptr<Player> playerObject = app->game.getPlayer();
    
    // Update camera to follow player
    if (playerObject) {
        app->camera->update(playerObject);
    }
}

void updateTiming(AppContext* app) {
    Uint64 current_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (app->last_time_us == 0) { // First frame initialization
        app->last_time_us = current_time_us;
        return; // Skip first frame to avoid large deltaTime
    }
    
    Uint64 delta_time_us = current_time_us - app->last_time_us;
    app->last_time_us = current_time_us;
    
    // Cap deltaTime to prevent huge jumps (e.g., when debugging or window is minimized)
    // 33333 microseconds = ~30 FPS minimum (1/30 second)
    const Uint64 max_delta_time_us = 33333;
    if (delta_time_us > max_delta_time_us) {
        delta_time_us = max_delta_time_us;
    }
    
    // Convert to seconds for the game update
    float delta_time_seconds = static_cast<float>(delta_time_us) / 1000000.0f;
    
    // Print once per second for debugging
    static Uint64 last_print_time_us = 0;
    if (current_time_us - last_print_time_us >= 1000000) {
        // SDL_Log("Delta time: %.6f seconds", delta_time_seconds);
        last_print_time_us = current_time_us;
    }

    // Update game with variable deltaTime
    app->game.update(delta_time_seconds);
    
    // Update camera with variable deltaTime
    updateCamera(app);
}

void renderParallaxBackgrounds(AppContext* app) {
    float camX = app->camera->getPosition().x;
    float camY = app->camera->getPosition().y;
    // Parallax factors (0 = static, 1 = moves with camera)
    float bgFactor = 0.1f; // background almost static
    float layer1Factor = 0.3f;
    float layer2Factor = 0.6f;

    // Helper lambda to render a parallax layer at native size, moving with camera and tiling if needed
    auto renderParallaxLayer = [&](SDL_Texture* tex, float factor) {
        if (!tex) return;
        auto props = SDL_GetTextureProperties(tex);
        int texW = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
        int texH = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);
        float offsetX = fmod(camX * factor, texW);
        float offsetY = fmod(camY * factor, texH);
        if (offsetX < 0) offsetX += texW;
        if (offsetY < 0) offsetY += texH;
        // Tile the image to fill the window
        for (float y = -offsetY; y < windowStartHeight; y += texH) {
            for (float x = -offsetX; x < windowStartWidth; x += texW) {
                SDL_FRect dest = { x, y, (float)texW, (float)texH };
                SDL_RenderTexture(app->renderer, tex, nullptr, &dest);
            }
        }
    };
    // Render farthest background (sky)
    renderParallaxLayer(app->backgroundTex, bgFactor);
    // Render parallax layer 1 (islands far)
    renderParallaxLayer(app->parallaxLayer1Tex, layer1Factor);
    // Render parallax layer 2 (islands closer)
    renderParallaxLayer(app->parallaxLayer2Tex, layer2Factor);
}

void renderEntities(AppContext* app) {
    //Load game objects (Entities, player(s), platforms)
    for(const auto& entity : app->game.getObjects()) {
        
        if (!entity || !entity->getCurrentSpriteData()) continue; // Basic safety check
        
        // Skip rendering objects that are outside the camera view (viewport culling)
        float entityX = entity->getcollider().position.x - (entity->getCurrentSpriteData()->getSpriteRect(0).w / 2);
        float entityY = entity->getcollider().position.y - (entity->getCurrentSpriteData()->getSpriteRect(0).h / 2);
        float entityWidth = entity->getCurrentSpriteData()->getSpriteRect(0).w;
        float entityHeight = entity->getCurrentSpriteData()->getSpriteRect(0).h;
        
        if (!app->camera->isVisible(entityX, entityY, entityWidth, entityHeight)) {
            continue; // Skip rendering this object
        }
        
        SDL_Texture* sprite_tex = nullptr;
        // Get the pre-loaded sprite info from the map
        auto it = spriteMap2.find(entity->getCurrentSpriteData()->getid_());
        if (it == spriteMap2.end()) {
             SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Sprite ID %s not found in spriteMap for Object", entity->getCurrentSpriteData()->getid_().c_str());
            //  continue; // Skip rendering if sprite not found
            sprite_tex = spriteMap2["fatbat.png"];
        }
        else
        {
            sprite_tex = it->second; // Use the existing texture from the map
        }

        // Use the current sprite index from animation system
        int spriteIndex = entity->getCurrentSpriteIndex();
        const SpriteRect& currentSpriteRect = entity->getCurrentSpriteData()->getSpriteRect(spriteIndex);
        
        SDL_FRect srcRect = {
            static_cast<float>(currentSpriteRect.x),
            static_cast<float>(currentSpriteRect.y),
            static_cast<float>(currentSpriteRect.w),
            static_cast<float>(currentSpriteRect.h)
        };

        
        // Convert world coordinates to screen coordinates using the camera
        Vec2 screenPos = app->camera->worldToScreen(
            entity->getcollider().position.x - (currentSpriteRect.w / 2),
            entity->getcollider().position.y - (currentSpriteRect.h / 2)
        );

        SDL_FRect destRect{
            .x = screenPos.x,
            .y = screenPos.y,
            .w = static_cast<float>(currentSpriteRect.w),
            .h = static_cast<float>(currentSpriteRect.h)
        };

        // Normal rendering without flip
        SDL_RenderTexture(app->renderer, sprite_tex, &srcRect, &destRect);
    }
}

void renderActors(AppContext* app) {
    for(const auto& actor : app->game.getActors()) {
         if (!actor) continue; // Basic safety check

         const SpriteData* spriteData = actor->getCurrentSpriteData();
         // Get the pre-loaded sprite info for the character texture (ID 11)
         auto it_char_tex = spriteMap2.find(spriteData->getid_()); // Should be ID 11 for letters
          if (it_char_tex == spriteMap2.end()) {
              SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Sprite ID %s not found in spriteMap for Actor", spriteData->getid_().c_str());
              continue;
          }
         SDL_Texture* sprite_tex = it_char_tex->second; // Base texture for letters.png

        ActorType actorType = actor->gettype();
        if(actorType == ActorType::HEALTHBAR) {
            std::vector<SpriteRect> spriteRects = static_cast<Healthbar*>(actor)->getSpriteRects();
            int count = 0;
            int maxCount = spriteRects.size();
            std::vector<float> offsets = static_cast<Healthbar*>(actor)->getOffsets(maxCount); // Get personal offsets
            for(int count = 0; count < maxCount; count++)
            {
                auto rect = spriteRects[count];
                // Note: We assume the healthbar sprite rects are already in the correct format
                SDL_FRect srcRect = {
                    static_cast<float>(rect.x),
                    static_cast<float>(rect.y),
                    static_cast<float>(rect.w),
                    static_cast<float>(rect.h)
                };

                // Use personal offset for this specific count/part
                float centerOffset = 0.0f;
                if (count < offsets.size()) {
                    centerOffset = offsets[count];
                } else {
                    // Fallback to old calculation if offset not provided
                    if (count == 2 || count == maxCount - 1) {
                        centerOffset = 0.0f;
                    } else {
                        centerOffset = (count - 2) * rect.w;
                    }
                }

                float x = actor->getposition().x + centerOffset;

                Vec2 screenPos = app->camera->worldToScreen(
                    x - (rect.w / 2),
                    actor->getposition().y - (rect.h / 2)
                );

                SDL_FRect destRect{
                    .x = screenPos.x,
                    .y = screenPos.y,
                    .w = static_cast<float>(rect.w),
                    .h = static_cast<float>(rect.h)
                };
                SDL_RenderTexture(app->renderer, sprite_tex, &srcRect, &destRect); // Use pre-loaded texture
            }
        }
        else    //Render text
        {
    
            // Get the specific source rect for this character index
            const SpriteRect& charSpriteRect = spriteData->getSpriteRect(actor->getdefaultIndex());
            SDL_FRect srcRect = {
                static_cast<float>(charSpriteRect.x),
                static_cast<float>(charSpriteRect.y),
                static_cast<float>(charSpriteRect.w),
                static_cast<float>(charSpriteRect.h)
            };
            
            // Note: For UI elements like actors, we might want to keep them in screen space
            // rather than applying the camera transform. This depends on whether they're
            // part of the HUD or part of the game world.
            
            // Convert world coordinates to screen coordinates using the camera
            Vec2 screenPos = app->camera->worldToScreen(
                actor->getposition().x,
                actor->getposition().y
            );
            
            SDL_FRect destRect{
                .x = screenPos.x,
                .y = screenPos.y,
                .w = static_cast<float>(charSpriteRect.w),
                .h = static_cast<float>(charSpriteRect.h)
            };
            SDL_RenderTexture(app->renderer, sprite_tex, &srcRect, &destRect); // Use pre-loaded texture
        }
    }
    // app->game.clearActors(); // Clear actors after rendering
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*)appstate;

    if(!app->game.isRunning())
    {
        app->app_quit = SDL_APP_SUCCESS;
        return app->app_quit;
    }

    // Update timing and game logic at fixed timestep
    updateTiming(app);

    // Clear screen
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(app->renderer);

    // Render parallax backgrounds
    renderParallaxBackgrounds(app);

    // Render game entities
    renderEntities(app);

    // Render UI actors
    renderActors(app);

    // Present the rendered frame
    SDL_RenderPresent(app->renderer);

    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = (AppContext*)appstate;
    if (app) {

        // Clean up SDL resources
        SDL_DestroyTexture(app->messageTex);
        SDL_DestroyTexture(app->backgroundTex);
        SDL_DestroyTexture(app->parallaxLayer1Tex);
        SDL_DestroyTexture(app->parallaxLayer2Tex);
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);

        // Clean up input (if dynamically allocated in AppInit)
        // delete static_cast<SDLInput*>(app->game.getInput()); // Be careful with ownership

        delete app; // Delete the context itself
    }
    TTF_Quit();
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