#include "SDL2Input.h"
#include "logger.h"
#include <SDL2/SDL.h>

SDL2Input::SDL2Input(SDL_GameController* controller)
    : gameController(controller)
{
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

void SDLInput::readInput() {
    SDL_PumpEvents();

    bool up_pressed = false;
    bool down_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool attack_pressed = false;

    if (gameController) {
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