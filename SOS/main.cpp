// SOS/main.cpp

#include "game.h"
#include <iostream>
#include <chrono>
#include <thread>

#include "petalinux/input_pl.h"

// Example usage in your game initialization
#include "petalinux/SDL2AudioManager.h"

#include "Renderer.h"

float FPS = 60.0f;

uint32_t get_ticks() {
	auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<uint32_t>(duration.count());
}


int main() {
	Renderer renderer;
	// PlayerInput* controller = new EvdevController();
	// Game game(controller);
	std::cout << "Starting game Saga Of Sacrifice 2..." << std::endl;
	renderer.init();
	
	auto lastTime = get_ticks();
	auto lastRenderTime = lastTime;
	

	// In your game initialization code
	std::unique_ptr<AudioManager> audioManager = std::make_unique<SDL2AudioManager>();
	audioManager->initialize("/home/root/SagaOfSacrifice2/SOS/assets/");

	// Load sounds and music
	// audioManager->loadSound("assets/music/effect1.wav");
	audioManager->loadMusic("music/menu/menu.wav");
	audioManager->loadSound("sfx/jump.wav");

	// Play music
	audioManager->setMusicVolume(0.9f);
	// audioManager->playMusic();

	// Play sound effects during gameplay
	// audioManager->playSound("jump");

	std::cout << "Entering gameloop..." << std::endl;
	// while (game.isRunning()) {
	while (true) {
		auto currentTime = get_ticks();
		uint32_t deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		
		//Every second play sound
		static int lastSoundTime = 0;
		if (currentTime - lastSoundTime > 1000) {
			audioManager->playSound("jump");
			std::cout << "Playing sound effect" << std::endl;
			lastSoundTime = currentTime;
		}

		// game.update(deltaTime);
		uint32_t renderElapsedTime = currentTime - lastRenderTime;
		if (renderElapsedTime > 1000.0f)
		{
			// renderer.render(game.getObjects());
			// controller->readInput();
		}
		
	}

	return 0;
}