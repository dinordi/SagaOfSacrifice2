// SOS/src/game.cpp

#include "game.h"
#include "characters.h"

Game::Game(PlayerInput* input) : running(true), input(input) {
    
    CollisionManager* collisionManager = new CollisionManager();
    this->collisionManager = collisionManager;
    // Initialize game objects here
    player = new Player(Vec2(500,100), new SpriteData(std::string("playermap"), 128, 128, 5));
    player->setInput(input);
    objects.push_back(player);

    Platform* floor = new Platform(500, 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor2 = new Platform(500 + (1*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor3 = new Platform(500 + (2*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    Platform* floor4 = new Platform(500 + (3*128), 850, new SpriteData(std::string("tiles"), 128, 128, 4));
    objects.push_back(floor);
    objects.push_back(floor2);
    objects.push_back(floor3);
    objects.push_back(floor4);

    characters = new SpriteData(std::string("letters"), 127, 127, 3);
    mapCharacters();

    drawWord("Saga Of Sacrifice 2", 100, 100);

}

Game::~Game() {
    // Clean up game objects here
}

void Game::mapCharacters()
{
    // Get all the letters from a-z and 0-9
    std::vector<char> letters;
    for (char c = 'a'; c <= 'z'; ++c) {
        letters.push_back(c);
    }
    for (char c = '0'; c <= '9'; ++c) {
        letters.push_back(c);
    }
    int index = 0;
    for(char c : letters) {
        characterMap[c] = index++;
    }
}

void Game::update(uint64_t deltaTime) {

    static uint64_t timeseconds = 0.0f;
    timeseconds += deltaTime;
    if (timeseconds >= 1000.0f) {
        // std::cout << "Hello World" << std::endl;
        timeseconds = 0.0f;
    }
    input->readInput();
    // player->handleInput(input, deltaTime);
    // Update player input here

    collisionManager->detectCollisions(objects);

    for(auto object : objects) {
        object->update(deltaTime);
    }

}

void Game::drawWord(const std::string& word, int x, int y) {
    // Draw the word at the specified position
    //Change word to lower case
    std::string lowerWord = word;
    std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
    std::vector<char> chars(lowerWord.begin(), lowerWord.end());
    int start_x = x - 64;
    int start_y = y;
    for (char c : chars) {
        start_x += 64; // Add space between characters
        if(c == ' ') {
            continue;
        }
        Actor* actor = new Actor(Vec2(start_x, start_y), new SpriteData(std::string("letters"), 64, 64, 3), characterMap[c]);
        // Set the position of the actor based on the character width
        actors.push_back(actor);
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<Object*>& Game::getObjects() {
    return objects;
}

std::vector<Actor*>& Game::getActors() {
    //Return actors and empty the vector
    // std::vector<Actor*> temp = actors;
    // actors.clear();
    return actors;
}