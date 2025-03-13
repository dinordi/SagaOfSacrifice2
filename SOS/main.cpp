// SOS/main.cpp

#include "engine.h"
#include "game.h"
#include <iostream>
#include <chrono>
#include <thread>

float FPS = 60.0f;

int main() {
	PhysicsEngine physicsEngine;
	Game game;
	std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
	// Add game objects to the physics engine
	for (Object* object : game.getObjects()) {
		physicsEngine.addObject(object);
	}
	std::cout << "Added game objects to the physics engine." << std::endl;

	auto lastTime = std::chrono::high_resolution_clock::now();
	auto lastRenderTime = lastTime;

	std::cout << "Entering gameloop..." << std::endl;
	while (game.isRunning()) {
		auto currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> elapsedTime = currentTime - lastTime;
		float deltaTime = elapsedTime.count();
		lastTime = currentTime;

		physicsEngine.update(deltaTime);
		game.update(deltaTime);
		std::chrono::duration<float> renderElapsedTime = currentTime - lastRenderTime;
		if (renderElapsedTime.count() > 1.0f / FPS)
		{
			game.render();
		}
		
	}

	return 0;
}