#include "SDL2Input.h"
#include "logger.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <SDL2/SDL_scancode.h>

SDL2Input::SDL2Input()
{
    SDL_Init(SDL_INIT_GAMECONTROLLER);

    // Check SDL2 version
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    std::cout << "SDL2 compiled version: " << (int)compiled.major << "." << (int)compiled.minor << "." << (int)compiled.patch << std::endl;
    std::cout << "SDL2 linked version: " << (int)linked.major << "." << (int)linked.minor << "." << (int)linked.patch << std::endl;
    

    this->gameController = nullptr;
    if (SDL_NumJoysticks() > 0) {
        std::cout << "Controllers found: " << SDL_NumJoysticks() << std::endl;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            std::cout << "Joystick " << i << " name: " << SDL_JoystickNameForIndex(i) << std::endl;
            if (SDL_IsGameController(i)) {
                const char* name = SDL_GameControllerNameForIndex(i);
                std::cout << "Detected controller at index " << i << ": " << (name ? name : "Unknown") << std::endl;
                gameController = SDL_GameControllerOpen(i);
                if (gameController) {
                    std::cout << "Game controller " << i << " opened: " << (name ? name : "Unknown") << std::endl;
                    break;
                }
            }
            else {
                std::cout << "Joystick at index " << i << " is not a game controller." << std::endl;
            }
        }
    }
    else
    {
        std::cout << "No game controller found." << std::endl;
    }
    if (!gameController) {
        std::cout << "No game controller found, using keyboard input." << std::endl;
    }
    else {
        std::cout << "Game controller found, using gamepad input." << std::endl;
    }
    set_up(false);
    set_last_up(false);
    set_down(false);
    set_last_down(false);
    set_left(false);
    set_last_left(false);
    set_right(false);
    set_last_right(false);
    set_attack(false);
    set_last_attack(false);
}

SDL2Input::~SDL2Input() {
    if (gameController) {
        SDL_GameControllerClose(gameController);
        gameController = nullptr;
    }
    SDL_Quit();
}

void SDL2Input::readInput() {
    SDL_PumpEvents();

    bool up_pressed = false;
    bool down_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool attack_pressed = false;

    if (gameController) {
        SDL_GameControllerUpdate();
        // Print once a second
        // static Uint32 lastPrintTime = 0;
        // Uint32 currentTime = SDL_GetTicks();
        // if (currentTime - lastPrintTime > 10000) {
        //     lastPrintTime = currentTime;
        //     std::cout << "Game controller is active." << std::endl;
        //     std::cout << "Controller name: " << SDL_GameControllerName(gameController) << std::endl;
        //     std::cout << "Controller attached: " << (SDL_GameControllerGetAttached(gameController) ? "YES" : "NO") << std::endl;
        //     std::cout << "Controller button X: " << (int)SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_X) << std::endl;
        //     std::cout << "Controller button A: " << (int)SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_A) << std::endl;
        //     std::cout << "Controller axis LEFTY: " << SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY) << std::endl;
        //     std::cout << "Controller axis LEFTX: " << SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX) << std::endl;
        //     std::cout << "Controller axis RIGHTY: " << SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTY) << std::endl;
        //     std::cout << "Controller axis RIGHTX: " << SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTX) << std::endl;
        // }
        // Gamepad controls (SDL2)
        up_pressed = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY) < -8000 ||
                     SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_UP);
        down_pressed = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY) > 8000 ||
                       SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        left_pressed = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX) < -8000 ||
                       SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        right_pressed = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX) > 8000 ||
                        SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        attack_pressed = SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_X); // WEST = X on SDL2
    } else {
        // Keyboard controls
        const Uint8* state = SDL_GetKeyboardState(NULL);
        up_pressed = state[SDL_SCANCODE_W];
        down_pressed = state[SDL_SCANCODE_S];
        left_pressed = state[SDL_SCANCODE_A];
        right_pressed = state[SDL_SCANCODE_D];
        attack_pressed = state[SDL_SCANCODE_K];
    }

    // if(up_pressed)
    // {
    //     std::cout << "Up pressed" << std::endl;
    // }
    // if(down_pressed)
    // {
    //     std::cout << "Down pressed" << std::endl;
    // }
    // if(left_pressed)
    // {
    //     std::cout << "Left pressed" << std::endl;
    // }
    // if(right_pressed)
    // {
    //     std::cout << "Right pressed" << std::endl;
    // }
    // if(attack_pressed)
    // {
    //     std::cout << "Attack pressed" << std::endl;
    // }
    set_left(left_pressed);
    set_right(right_pressed);
    set_up(up_pressed);
    set_down(down_pressed);

    if (attack_pressed && !get_last_attack()) {
        set_attack(true);
    } else {
        set_attack(false);
    }
    set_last_attack(attack_pressed);
}