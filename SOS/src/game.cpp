// SOS/src/game.cpp

#include "game.h"
#include "characters.h"
#include "LocalServerManager.h"
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include <iostream>
#include <set> // Add missing header for std::set

// Initialize static instance pointer
Game* Game::instance_ = nullptr;

// External function declaration
extern uint32_t get_ticks(); // Declare the get_ticks function

// Default port for local server in single-player mode
const int LOCAL_SERVER_PORT = 8081;

Game::Game(PlayerInput* input, std::string playerID) : running(true), input(input), multiplayerActive(false), usingSinglePlayerServer(false), menuInputCooldown(0), menuOptionChanged(true), selectedOption(MenuOption::SINGLEPLAYER) {
    // Set this as the active instance
    instance_ = this;
    
    // Initialize the local server manager
    localServerManager = std::make_unique<LocalServerManager>();
    
    // Initialize network manager (will be connected later)
    multiplayerManager = std::make_unique<MultiplayerManager>();
    
    // Initialize collision manager (used for local prediction only)
    this->collisionManager = new CollisionManager();;
    
    // Create player using PlayerManager
    auto playerSharedPtr = PlayerManager::getInstance().createPlayer(playerID, Vec2(500, 100));
    
    // Set player's input handler
    player = playerSharedPtr.get();
    player->setInput(input);
    mapCharacters();
    state = GameState::MENU;
}

Game::~Game() {
    // If we are the current instance, clear the static pointer
    if (instance_ == this) {
        instance_ = nullptr;
    }
    
    // Clean up game objects
    // No need to manually delete objects as they are managed by shared_ptr
    objects.clear();
    
    delete collisionManager;
    
    // Shutdown multiplayer if active
    shutdownServerConnection();
    std::cout << "[Game] Shut down multiplayer connection" << std::endl;
    localServerManager->stopEmbeddedServer();
}

std::string Game::generateRandomPlayerId() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int idLength = 8;
    std::string result;
    result.reserve(idLength);
    
    for (int i = 0; i < idLength; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return result;
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
    // Print the mapping
    for (const auto& pair : characterMap) {
        std::cout << pair.first << " -> " << pair.second << std::endl;
    }

    characterMap['>'] = 36;
}

void Game::update(float deltaTime) {
    // Process local input
    input->readInput();
    
    switch(state)
    {
        case GameState::RUNNING:
            for(auto& obj : objects) {
                if (obj) {
                    if(obj->getObjID() == player->getObjID()) {
                        continue; // Skip updating the local player
                    }
                    // Update the player object
                    obj->updateAnimation(deltaTime * 1000);
                }
            }
            break;
        case GameState::MENU:
            // Handle menu state
            drawMenu(deltaTime);
            handleMenuInput(deltaTime);
            return;
        default:
            std::cerr << "[Game] Unknown game state" << std::endl;
            return;
    }

    // If multiplayer is active, update network state
    if (multiplayerActive && multiplayerManager) {
        // Update the network state
        multiplayerManager->update(deltaTime);
        
        // Send player input to server
        multiplayerManager->setPlayerInput(input);
        
        // In the server-authoritative model:
        // 1. We still apply local input immediately for responsive feel
        // 2. But the server will correct our position if needed
        predictLocalPlayerMovement(deltaTime);

        // reconcileWithServerState(deltaTime);
        
        // Update remote players based on server data
        // updateRemotePlayers(multiplayerManager->getRemotePlayers());
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<std::shared_ptr<Object>>& Game::getObjects() {
    return objects;
}

bool Game::initializeSinglePlayerEmbeddedServer() {
    // Initialize multiplayer with embedded server
    if (usingSinglePlayerServer) {
        return true; // Already initialized
    }
    
    std::cout << "[Game] Setting up single player with embedded server" << std::endl;
    
    // Start the embedded server
    if (!localServerManager->startEmbeddedServer(LOCAL_SERVER_PORT)) {
        std::cerr << "[Game] Failed to start embedded server" << std::endl;
        return false;
    }
    
    // Connect to the local server
    std::string playerId = player->getObjID();
    if (!initializeServerConnection("localhost", LOCAL_SERVER_PORT, playerId)) {
        std::cerr << "[Game] Failed to connect to embedded server" << std::endl;
        localServerManager->stopEmbeddedServer();
        return false;
    }
    
    usingSinglePlayerServer = true;
    std::cout << "[Game] Single player mode with embedded server initialized" << std::endl;
    
    return true;
}


bool Game::initializeServerConnection(const std::string& serverAddress, int serverPort, const std::string& playerId) {
    if (!multiplayerManager) {
        multiplayerManager = std::make_unique<MultiplayerManager>();
    }
    
    bool success = multiplayerManager->initialize(serverAddress, serverPort, playerId);
    
    if (success) {
        multiplayerActive = true;
        multiplayerManager->setLocalPlayer(player);
        multiplayerManager->setPlayerInput(input);
        std::cout << "[Game] (Multiplayer)server initialized successfully" << std::endl;
    } else {
        std::cerr << "[Game] Failed to initialize (multiplayer) server" << std::endl;
    }
    
    return success;
}

void Game::shutdownServerConnection() {
    if (multiplayerManager && multiplayerActive) {
        multiplayerManager->shutdown();
        multiplayerActive = false;
        std::cout << "[Game] (Multiplayer) server shut down" << std::endl;
    }
    
    if ((usingSinglePlayerServer) && localServerManager) {
        usingSinglePlayerServer = false;
        std::cout << "[Game] Local server shut down" << std::endl;
    }
}

bool Game::isServerConnection() const {
    return multiplayerActive && multiplayerManager && multiplayerManager->isConnected();
}

std::vector<Actor*>& Game::getActors() {
    return actors;
}

void Game::clearActors() {
    for(auto& actor : actors) {
        delete actor; // Clean up each actor
    }
    actors.clear();
}

void Game::sendChatMessage(const std::string& message) {
    if (multiplayerActive && multiplayerManager) {
        multiplayerManager->sendChatMessage(message);
    }
}

void Game::setChatMessageHandler(std::function<void(const std::string& sender, const std::string& message)> handler) {
    if (multiplayerManager) {
        multiplayerManager->setChatMessageHandler(handler);
    }
}

void Game::updateRemotePlayers(const std::map<std::string, std::unique_ptr<RemotePlayer>>& remotePlayers) {
    // loop through all remote players
    for (const auto& pair : remotePlayers) {
        //Find the remote player in the gameobjects
        //If not found, create a new one, otherwise continue
        auto it = std::find_if(objects.begin(), objects.end(), [&](const std::shared_ptr<Object>& obj) {
            return obj->getObjID() == pair.first;
        });
        if (it == objects.end()) {
            // Create a new remote player
            std::cout << "[Game] Creating new remote player: " << pair.first << std::endl;
            RemotePlayer* remotePlayer = new RemotePlayer(pair.first);
            remotePlayer->setcollider(pair.second->getcollider());
            remotePlayer->setvelocity(pair.second->getvelocity());
            objects.push_back(std::shared_ptr<RemotePlayer>(remotePlayer));
        } else {
            if((*it)->getObjID() == player->getObjID())
            {
                // Skip updating the local player
                continue;
            }
            else
            {
                // Update existing remote player
                (*it)->setcollider(pair.second->getcollider());
                (*it)->setvelocity(pair.second->getvelocity());
            }
        }
    }
}

// New methods for server-authoritative gameplay

void Game::predictLocalPlayerMovement(float deltaTime) {
    // Apply local input immediately for responsive gameplay
    // This is a simple client-side prediction that will be corrected by the server if needed
    player->handleInput(input, deltaTime);
    // player->update(deltaTime);
}

void Game::reconcileWithServerState(float deltaTime) {
    // Compare the server's authoritative state with our predicted state
    // and correct any discrepancies
    
    // This would happen after receiving a server state update that includes
    // the client's input sequence number
    
    // Get the server position for our player from the MultiplayerManager
    if (multiplayerManager && player) {

        const std::map<std::string, std::unique_ptr<RemotePlayer>>& remotePlayers = multiplayerManager->getRemotePlayers();
        auto it = remotePlayers.find(player->getObjID());
        static uint64_t lastUpdateTime = 0;
        static uint64_t remotePlayerWaitTime = 0;
        lastUpdateTime += deltaTime;
        remotePlayerWaitTime += deltaTime;

        if (it == remotePlayers.end()) {
            // If we've been waiting too long for the server to recognize our player, 
            // let's try resending our player information
            if (remotePlayerWaitTime > 5000) { // 5 seconds
                std::cout << "[Game] Resynchronizing player with server: " << player->getObjID() << std::endl;
                // Force resend of player info
                multiplayerManager->sendPlayerState();
                remotePlayerWaitTime = 0;
            }
            
            // Only log the error occasionally to avoid console spam
            if (lastUpdateTime > 5000) { // Every 5 seconds
                std::cout << "[Game] Waiting for server to synchronize player ID: " << player->getObjID() << std::endl;
                lastUpdateTime = 0;
            }
            return;
        }
        
        // Reset the wait timer once we find our player
        remotePlayerWaitTime = 0;
        RemotePlayer* remotePlayer = it->second.get();
        BoxCollider serverCollider = remotePlayer->getcollider();
        BoxCollider clientCollider = player->getcollider();
        Vec2 serverPosition = serverCollider.position;
        Vec2 clientPosition = clientCollider.position;
        
        // Calculate position difference
        float dx = serverPosition.x - clientPosition.x;
        float dy = serverPosition.y - clientPosition.y;
        float distSquared = dx*dx + dy*dy;
        
        // If difference is significant (beyond a small threshold)
        if (distSquared > 4.0f) { // Small threshold to ignore minor differences
            // Option 1: Smoothly interpolate toward server position
            Vec2 newPosition = clientPosition;
            
            // Interpolate 20% of the way to the server position
            newPosition.x += dx * 0.2f;
            newPosition.y += dy * 0.2f;
            
            // Update player position
            player->setcollider(BoxCollider(newPosition, clientCollider.size));
        }
    }
}

// New method to add objects to the game
void Game::addObject(std::shared_ptr<Object> object) {
    if (object) {
        // Check if object with this ID already exists
        auto it = std::find_if(objects.begin(), objects.end(), 
                              [&](const std::shared_ptr<Object>& obj) {
                                  return obj->getObjID() == object->getObjID();
                              });
        
        if (it == objects.end()) {
            // Add new object if it doesn't exist
            objects.push_back(object);
            std::cout << "[Game] Added new object with ID: " << object->getObjID() << std::endl;
        } 
        // else {
        //     std::cerr << "[Game] Object with ID " << object->getObjID() << " already exists" << std::endl;
        // }
    }
}

// // Initialize and load a level
// bool Game::initializeLevel(const std::string& levelId) {
//     if (!levelManager) {
//         std::cerr << "[Game] LevelManager not initialized" << std::endl;
//         return false;
//     }
    
//     // Initialize level manager if not done already
//     if (!levelManager->initialize()) {
//         std::cerr << "[Game] Failed to initialize LevelManager" << std::endl;
//         return false;
//     }
    
//     // Load the specified level
//     if (!levelManager->loadLevel(levelId)) {
//         std::cerr << "[Game] Failed to load level: " << levelId << std::endl;
//         return false;
//     }
    
//     // Add the player to the level
//     std::string playerId = player->getObjID();
//     if (!levelManager->addPlayerToCurrentLevel(playerId)) {
//         std::cerr << "[Game] Failed to add player to level" << std::endl;
//         return false;
//     }
    
//     std::cout << "[Game] Successfully initialized level: " << levelId << std::endl;
//     return true;
// }

void Game::drawMenu(float deltaTime) {
    bool print = false;
    static float lastTime = 0;
    lastTime += deltaTime;
    if(lastTime > 1.0f)
    {
        if(lastTime > 2.0f)
        {
            lastTime = 0;
        }
        print = true;
    }
    
    if(actors.size() != 0 && !menuOptionChanged)
        return;
    
    // Clear previous actors if selection changed
    if (menuOptionChanged) {
        clearActors();
        menuOptionChanged = false;
    }

    // Draw the menu screen
    drawWord("Saga of sacrifice 2", 250, 100);

    // Draw menu options with highlighting for the selected option
    drawWordWithHighlight("Singleplayer", 400, 200, selectedOption == MenuOption::SINGLEPLAYER);
    drawWordWithHighlight("Multiplayer", 400, 300, selectedOption == MenuOption::MULTIPLAYER);
    drawWordWithHighlight("Exit", 400, 400, selectedOption == MenuOption::EXIT);
    drawWordWithHighlight("Credits", 400, 500, selectedOption == MenuOption::CREDITS);

    if(print)
    {
        drawWord("Use UP/DOWN to select", 200, 600); 
        drawWord("Square to confirm", 200, 680);
    }
}

void Game::drawWord(const std::string& word, int x, int y) {
    // lowercase the word
    std::string lowerWord = word;
    std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
    // Draw the word at the specified position
    for (char c : lowerWord) {
        auto it = characterMap.find(c);
        if (it != characterMap.end()) {
            int index = it->second;
            Actor* character = new Actor(Vec2(x,y), new SpriteData("letters", 64, 64, 3), index);
            actors.push_back(character);
            x += 64; // Move to the right for the next character
        }
        else
        {
            x += 64; // Space
        }
    }
}

void Game::drawWordWithHighlight(const std::string& word, int x, int y, bool isSelected) {
    // Clear any existing actors at this position (to refresh selection highlight)
    if (isSelected) {
        // Add a simple visual indicator for the selected item
        Actor* selector = new Actor(Vec2(x - 80, y), new SpriteData("letters", 64, 64, 3), 36);
        actors.push_back(selector);
    }
    
    // Draw the actual word
    drawWord(word, x, y);
}

void Game::handleMenuInput(float deltaTime) {
    // Update cooldown timer
    if (menuInputCooldown > 0) {
        menuInputCooldown -= deltaTime;
        return;
    }
    
    // Check for up/down input
    bool inputDetected = false;
    
    // Using virtual key codes for UP and DOWN
    if (input->get_up()) {
        // Move selection up
        int newOption = static_cast<int>(selectedOption) - 1;
        if (newOption < 0) {
            newOption = static_cast<int>(MenuOption::COUNT) - 1;
        }
        selectedOption = static_cast<MenuOption>(newOption);
        inputDetected = true;
    } else if (input->get_down()) {
        // Move selection down
        int newOption = static_cast<int>(selectedOption) + 1;
        if (newOption >= static_cast<int>(MenuOption::COUNT)) {
            newOption = 0;
        }
        selectedOption = static_cast<MenuOption>(newOption);
        inputDetected = true;
    } else if (input->get_attack()) {
        // Select current option
        switch(selectedOption) {
            case MenuOption::SINGLEPLAYER:
                // Start single player game
                state = GameState::RUNNING;
                objects.push_back(std::shared_ptr<Player>(player)); // Add player to objects
                clearActors(); // Clear the menu
                break;
            case MenuOption::MULTIPLAYER:
                // Start multiplayer game
                state = GameState::RUNNING;
                objects.push_back(std::shared_ptr<Player>(player)); // Add player to objects
                clearActors(); // Clear the menu
                break;
            case MenuOption::EXIT:
                // Exit game
                running = false;
                break;
            case MenuOption::CREDITS:
                // Show credits (not implemented)
                break;
            default:
                break;
        }
        inputDetected = true;
    }
    
    if (inputDetected) {
        menuInputCooldown = MENU_INPUT_DELAY;
        menuOptionChanged = true;
    }
}