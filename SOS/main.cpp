// SOS/main.cpp

#include "engine.h"
#include "game.h"
#include <iostream>
#include <chrono>
#include <thread>

#include "petalinux/input_pl.h"

#include "Renderer.h"

float FPS = 60.0f;

uint32_t get_ticks() {
	auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<uint32_t>(duration.count());
}


int main() {
	PhysicsEngine physicsEngine;
	Renderer renderer;
	Game game;
	std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
	renderer.init();
	
	auto lastTime = get_ticks();
	auto lastRenderTime = lastTime;

	EvdevController controller;

	std::cout << "Entering gameloop..." << std::endl;
	while (game.isRunning()) {
		auto currentTime = get_ticks();
		uint32_t deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		physicsEngine.update(deltaTime, game.getObjects());
		game.update(deltaTime);
		uint32_t renderElapsedTime = currentTime - lastRenderTime;
		if (renderElapsedTime > 1000.0f / FPS)
		{
			renderer.render(game.getObjects());
			controller.update();
			if (controller.isKeyPressed(BTN_SOUTH)) { // X knop op PlayStation layout
                std::cout << "X (BTN_SOUTH) pressed!\n";
            }
		}
		
	}

	return 0;
}