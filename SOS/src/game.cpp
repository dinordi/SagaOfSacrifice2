// SOS/src/game.cpp

#include "game.h"
#include "characters.h"
#include "LocalServerManager.h"
#include "network/MultiplayerManager.h"
#include "network/AsioNetworkClient.h"
#include "utils/TimeUtils.h"  // Include proper header for get_ticks()
#include <iostream>
#include <set> // Add missing header for std::set
#include <filesystem> // For std::filesystem::path
#include <mutex> // Add mutex header

// External function declaration
extern uint32_t get_ticks(); // Declare the get_ticks function

// Default port for local server in single-player mode
const int LOCAL_SERVER_PORT = 8081;

Game::Game() : running(true), multiplayerActive(false), usingSinglePlayerServer(false), menuInputCooldown(0), menuOptionChanged(true), selectedOption(MenuOption::SINGLEPLAYER) {
    
    // Initialize the local server manager
    localServerManager = std::make_unique<LocalServerManager>();    //Only used for single-player mode with embedded server
    // Initialize network manager (will be connected later)
    multiplayerManager = std::make_unique<MultiplayerManager>();// "Server" manager would be more appropriate, since it is used in both single and multiplayer modes.
    
    // Initialize collision manager (used for local checks only)
    this->collisionManager = new CollisionManager();

    std::filesystem::path base = std::filesystem::current_path();
    std::string temp = base.string();
    std::size_t pos = temp.find("SagaOfSacrifice2/");
    if (pos != std::string::npos) {
        temp = temp.substr(0, pos + std::string("SagaOfSacrifice2/").length());
    }
    auto basePath = std::filesystem::path(temp);
    basePath /= "SOS/assets/spriteatlas";
    basePath_ = basePath; // Store base path for later use
    std::cout << "Got base path for game" << std::endl;
    player = nullptr; // Initialize player to null
    // Store player ID but don't create a player yet - server will create and send it
    mapCharacters();    //Map characters to their indices
    state = GameState::MENU;
}

Game::~Game() {
    // Clean up game objects
    {
        std::lock_guard<std::mutex> lock(objectsMutex);
        objects.clear();
    }
    
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

void Game::initializeSpriteSheets()
{
    // For all tpsheets in basePath_, load them
    std::cout << "[Game] Initializing sprite sheets from: " << basePath_ << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(basePath_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tpsheet") {
            std::string tpsheetPath = entry.path().string();
            std::cout << "[Game] Loading sprite sheet: " << tpsheetPath << std::endl;
            SpriteData::getSharedInstance(tpsheetPath);
        }
    }
}

void Game::update(float deltaTime) {
    // Process local input
    input->readInput();
    
    switch(state)
    {
        case GameState::RUNNING:
            {
                std::lock_guard<std::mutex> lock(objectsMutex);
                // Check for and remove dead enemies
                objects.erase(
                    std::remove_if(objects.begin(), objects.end(), 
                        [this](const std::shared_ptr<Object>& obj) {
                            if (obj && obj->type == ObjectType::MINOTAUR) {
                                Enemy* enemy = static_cast<Enemy*>(obj.get());
                                
                                // If the enemy is dead, notify the server before removing it
                                if (enemy && enemy->isDead()) {
                                    // Only notify if multiplayer is active
                                    if (multiplayerActive && multiplayerManager) {
                                        multiplayerManager->sendEnemyStateUpdate(
                                            enemy->getObjID(), 
                                            true,  // isDead = true
                                            0      // Health = 0
                                        );
                                    }
                                    clearActors();
                                    return true; // Remove this enemy
                                }
                            }
                            return false;
                        }
                    ), 
                    objects.end()
                );

                if(!player)
                {
                    break;
                }
                
                // Update all objects
                for(auto& obj : objects) {
                    if (obj) {
                        if(obj->getObjID() == player->getObjID()) {
                            continue; // Skip updating the local player
                        }
                        // Update the object's animation
                        obj->updateAnimation(deltaTime * 1000);
                        // Update healthbar if it exists
                        if (obj->type == ObjectType::PLAYER || obj->type == ObjectType::MINOTAUR) {
                            Entity* entity = static_cast<Entity*>(obj.get());
                            if (entity) {
                                if(obj->getObjID() != player->getObjID())
                                {
                                    entity->Entity::update(deltaTime); // Update entity state
                                }
                                entity->updateHealthbar();
                                Healthbar* healthbar = entity->getHealthbar();
                                if(healthbar) {
                                    //Check if healthbar is already in actors
                                    auto it = std::find_if(actors.begin(), actors.end(),
                                        [&healthbar](Actor* actor) {
                                            return actor->getObjID() == healthbar->getObjID();
                                        });
                                    if (it == actors.end()) {
                                        actors.push_back(healthbar); // Add healthbar to actors for rendering
                                    }
                                }
                            }
                        }
                    }

                }
            }
            // Ensure objects are sorted before rendering
            if(objects.size() > 1) {
                sortObjects();
            }
            break;
        case GameState::MENU:
            // If nothing pressed for a while, reset menu option
            static float menuIdleTime = 0.0f;
            menuIdleTime += deltaTime;
            if (menuIdleTime > 5.0f) { // Reset after 5 seconds of inactivity
                // Initialize single player mode with embedded server
                if (!initializeSinglePlayerEmbeddedServer()) {
                    std::cerr << "[Game] Failed to initialize single player mode." << std::endl;
                    // Could show error message in menu or fallback
                    break;
                } else {
                    std::cout << "[Game] Single player mode initialized successfully!" << std::endl;
                }
                // Start single player game
                state = GameState::RUNNING;
                {
                    std::lock_guard<std::mutex> lock(objectsMutex);
                    objects.push_back(std::shared_ptr<Player>(player)); // Add player to objects
                }
                clearActors(); // Clear the menu
            }
            // Handle menu state
            drawMenu(deltaTime);
            handleMenuInput(deltaTime);
            return;
        case GameState::SERVER_SELECTION:
            // Handle server selection state
            drawServerSelectionMenu(deltaTime);
            handleServerSelectionInput(deltaTime);
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
        if(player)
        {
            predictLocalPlayerMovement(deltaTime);
    
            reconcileWithServerState(deltaTime);
            
            // Update remote players based on server data
            // updateRemotePlayers(multiplayerManager->getRemotePlayers());
        }
    }
}

bool Game::isRunning() const {
    return running;
}

std::vector<std::shared_ptr<Object>>& Game::getObjects() {
    // Note: This returns a reference, caller should use their own locking if needed
    // or consider returning a copy for thread safety
    return objects;
}

bool Game::initializeSinglePlayerEmbeddedServer() {
    // Initialize multiplayer with embedded server
    if (usingSinglePlayerServer) {
        return true; // Already initialized
    }
    
    std::cout << "[Game] Setting up single player with embedded server" << std::endl;
    
    // Start the embedded server
    if (!localServerManager->startEmbeddedServer(LOCAL_SERVER_PORT, basePath_)) {
        std::cerr << "[Game] Failed to start embedded server" << std::endl;
        return false;
    }
    
    // Connect to the local server
    uint16_t playerId = 65000;
    if (!initializeServerConnection("localhost", LOCAL_SERVER_PORT, playerId)) {
        std::cerr << "[Game] Failed to connect to embedded server" << std::endl;
        localServerManager->stopEmbeddedServer();
        return false;
    }
    
    usingSinglePlayerServer = true;
    std::cout << "[Game] Single player mode with embedded server initialized" << std::endl;
    
    return true;
}


bool Game::initializeServerConnection(const std::string& serverAddress, int serverPort, const uint16_t playerId) {
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

// Add this new helper method
void Game::movePlayerToEnd() {
    if (!player) return;
    
    // Find the player in the objects vector
    auto playerIt = std::find_if(objects.begin(), objects.end(),
        [this](const std::shared_ptr<Object>& obj) {
            return obj.get() == player;
        });
    
    // If found and not already at the end, move it
    if (playerIt != objects.end() && playerIt != objects.end() - 1) {
        std::shared_ptr<Object> playerObj = *playerIt;
        objects.erase(playerIt);
        objects.push_back(playerObj);
    }
}

void Game::setMultiplayerConfig(bool enableMultiplayer, const std::string& serverAddress, int serverPort) {
    multiplayerConfigured = enableMultiplayer;
    configuredServerAddress = serverAddress;
    configuredServerPort = serverPort;
    std::cout << "[Game] Multiplayer configuration set: " << (enableMultiplayer ? "enabled" : "disabled") 
              << ", server: " << serverAddress << ":" << serverPort << std::endl;
}

void Game::initializeServerConfig(const std::string& basePath) {
    this->basePath_ = std::filesystem::path(basePath);
    std::filesystem::path configPath = std::filesystem::path(basePath) / "SOS" / "assets" / "server.json";
    std::cout << "[Game] Loading server configuration from: " << configPath.string() << std::endl;
    
    if (!serverConfig.loadFromFile(configPath.string())) {
        std::cout << "[Game] Failed to load server config, using defaults" << std::endl;
    }
    
    // Set the default server as the first selected server
    selectedServerIndex = 0;
    const ServerInfo* defaultServer = serverConfig.getDefaultServer();
    if (defaultServer) {
        // Find the index of the default server
        for (size_t i = 0; i < serverConfig.getServerCount(); ++i) {
            const ServerInfo* server = serverConfig.getServer(i);
            if (server && server->isDefault) {
                selectedServerIndex = i;
                break;
            }
        }
    }
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
    std::lock_guard<std::mutex> lock(objectsMutex);
    for(auto& actor : actors) {
        if(actor->gettype() == ActorType::HEALTHBAR)
            continue;   // Healthbar is managed by unique_ptr in Entity, so skip it
        delete actor; // Clean up each actor
    }
    actors.clear();
}

void Game::sendChatMessage(const std::string& message) {
    if (multiplayerActive && multiplayerManager) {
        multiplayerManager->sendChatMessage(message);
    }
}

void Game::setChatMessageHandler(std::function<void(const uint16_t sender, const std::string& message)> handler) {
    if (multiplayerManager) {
        multiplayerManager->setChatMessageHandler(handler);
    }
}

void Game::updateRemotePlayers(const std::map<uint16_t, std::shared_ptr<Player>>& remotePlayers) {
    std::lock_guard<std::mutex> lock(objectsMutex);
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
            Vec2 position = pair.second->getposition();
            Player* remotePlayer = new Player(position.x, position.y, pair.first);
            remotePlayer->setcollider(pair.second->getcollider());
            remotePlayer->setvelocity(pair.second->getvelocity());
            remotePlayer->setDir(pair.second->getDir());
            remotePlayer->setAnimationState(pair.second->getAnimationState());
            
            objects.push_back(std::shared_ptr<Player>(remotePlayer));

        } else {
            if(player)
            {
                if((*it)->getObjID() == player->getObjID())
                {
                    // Skip updating the local player
                    continue;
                }    
            }
            Vec2 position = pair.second->getposition();
            Vec2 oldPosition = (*it)->getposition();
            std::cout << "[Game] Updating remote player: " << pair.first 
                      << " from " << oldPosition.x << "," << oldPosition.y 
                      << " to " << position.x << "," << position.y << std::endl;
            // Update existing remote player
            (*it)->setcollider(pair.second->getcollider());
            (*it)->setvelocity(pair.second->getvelocity());
            (*it)->setDir(pair.second->getDir());
            (*it)->setAnimationState(pair.second->getAnimationState());
        }
    }
}

void Game::predictLocalPlayerMovement(float deltaTime) {
    if( !player || !input) {
        return; // No player or input to process
    }
    // Apply local input immediately for responsive gameplay
    player->handleInput(input, deltaTime);
    player->update(deltaTime);
    
    // Handle player attack hit registration
    static bool wasAttacking = false;
    static std::set<uint16_t> hitEnemiesThisAttack;
    
    // Check if player just started attacking this frame
    bool justStartedAttacking = player->isAttacking() && !wasAttacking;
    
    // Clear hit tracking when starting a new attack
    if (justStartedAttacking) {
        hitEnemiesThisAttack.clear();
    }
    
    if (player->isAttacking()) {
        std::lock_guard<std::mutex> lock(objectsMutex);
        // Check for enemies that may have been hit
        for (auto& obj : objects) {
            // Skip non-enemy objects
            if (obj->type != ObjectType::MINOTAUR) continue;
            
            // Skip enemies we've already hit in this attack
            if (hitEnemiesThisAttack.find(obj->getObjID()) != hitEnemiesThisAttack.end()) continue;
            
            // Try to cast to Enemy pointer
            Enemy* enemy = dynamic_cast<Enemy*>(obj.get());
            if (!enemy || enemy->isDead()) continue;
            
            // Check if the player's attack hits this enemy
            if (player->checkAttackHit(obj.get())) {
                // Mark this enemy as hit for this attack
                hitEnemiesThisAttack.insert(obj->getObjID());
                
                // Deal damage to the enemy
                int damageAmount = player->getAttackDamage();
                enemy->takeDamage(damageAmount);
                
                // If the enemy died from this hit, notify the server immediately
                if (multiplayerActive && multiplayerManager) {
                    multiplayerManager->sendEnemyStateUpdate(
                        enemy->getObjID(), 
                        true,  // isDead = true
                        0      // Health = 0
                    );
                }
                // Only hit each enemy once per attack frame
                break;
            }
        }
    }
    
    // Update attack tracking state
    wasAttacking = player->isAttacking();
    
    // Check for regular collisions (like with platforms)
    {
        std::lock_guard<std::mutex> lock(objectsMutex);
        collisionManager->detectPlayerCollisions(objects, player);
    }
}

void Game::reconcileWithServerState(float deltaTime) {
    // Compare the server's authoritative state with our predicted state
    // and correct any discrepancies
    
    // This would happen after receiving a server state update that includes
    // the client's input sequence number
    
    // Get the server position for our player from the MultiplayerManager
    if (multiplayerManager && player) {

        const std::map<uint16_t, std::shared_ptr<Player>>& remotePlayers = multiplayerManager->getRemotePlayers();
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
        Player* remotePlayer = it->second.get();

        Vec2 serverPosition = remotePlayer->getTargetPosition();
        BoxCollider* clientCollider = &player->getcollider();
        Vec2* clientPosition = &clientCollider->position;
        
        // Calculate position difference
        float dx = serverPosition.x - clientPosition->x;
        float dy = serverPosition.y - clientPosition->y;
        float distSquared = dx*dx + dy*dy;
        
        // Only perform sanity checks, not constant corrections
        const float MAX_ALLOWED_DEVIATION = 2500.0f; // 50 units squared
        const float TELEPORT_THRESHOLD = 10000.0f; // 100 units squared
        
        // if (distSquared > TELEPORT_THRESHOLD) {
        //     // Potential cheating or severe desync - force correction
        //     clientPosition->x = serverPosition.x;
        //     clientPosition->y = serverPosition.y;
            
        //     // Log potential cheating attempt
        //     std::cout << "[Game] Position sanity check failed - forced correction" << std::endl;
        // }
        // Server side validation check, can be implemented later
        // else if (distSquared > MAX_ALLOWED_DEVIATION) {  
        //     // Server validation failed - report to server we need validation
        //     multiplayerManager->requestPositionValidation();
        // }
    }
}
void Game::sortObjects() {
    std::sort(objects.begin(), objects.end(),
        [](const std::shared_ptr<Object>& a, const std::shared_ptr<Object>& b) {
            if (!a || !b) return false; // Handle null pointers gracefully
            if (a->getLayer() != b->getLayer())
                return a->getLayer() < b->getLayer();
            if (a->getposition().y != b->getposition().y)
                return a->getposition().y < b->getposition().y;
            return a->type < b->type; // Fallback comparison
        }
    );
}

// New method to add objects to the game
void Game::addObject(std::shared_ptr<Object> object) {
    if (object) {
        std::lock_guard<std::mutex> lock(objectsMutex);
        // Check if object with this ID already exists
        auto it = std::find_if(objects.begin(), objects.end(), 
                              [&](const std::shared_ptr<Object>& obj) {
                                  return obj->getObjID() == object->getObjID();
                              });
                           
        
        if (it == objects.end()) {
            // Add new object if it doesn't exist
            objects.push_back(object);

            if(player)
            {
                movePlayerToEnd();
            }
        } 
        // else {
        //     std::cerr << "[Game] Object with ID " << object->getObjID() << " already exists" << std::endl;
        // }
    }
}

void Game::drawMenu(float deltaTime) {
    bool print = false;
    static bool lastPrint = false;
    static float lastTime = 0;
    lastTime += deltaTime;
    if(lastTime > 0.5f)
    {
        if(lastTime > 1.0f)
        {
            lastTime = 0;
        }
        print = true;
    }
    std::lock_guard<std::mutex> lock(actorsMutex);
    if(actors.size() != 0 && !menuOptionChanged && lastPrint == print)
        return;
    
    // Clear previous actors if selection changed or print state changed
    if (menuOptionChanged || lastPrint != print) {
        clearActors();
        menuOptionChanged = false;
    }
    lastPrint = print;

    // Draw the menu screen
    drawWord("Saga of sacrifice 2", 250, 100);

    // Draw menu options with highlighting for the selected option
    drawWordWithHighlight("Singleplayer", 400, 200, selectedOption == MenuOption::SINGLEPLAYER);
    drawWordWithHighlight("Multiplayer", 400, 300, selectedOption == MenuOption::MULTIPLAYER);
    drawWordWithHighlight("Exit", 400, 400, selectedOption == MenuOption::EXIT);
    drawWordWithHighlight("Credits", 400, 500, selectedOption == MenuOption::CREDITS);

    if(print)
    {
        drawWord("Use UP/DOWN to select", 200, 600, 1); 
        drawWord("Square to confirm", 200, 680, 1);
    }
}

// letterSize 0=default 64x64, 1=32x32
void Game::drawWord(const std::string& word, int x, int y, int letterSize) {
    int letterWidth = (letterSize == 0) ? 64 : 32; // Default letter width
    int startX = x; // Store the starting X position for potential line wrapping
    // lowercase the word
    std::string lowerWord = word;
    std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
    // Draw the word at the specified position
    for (char c : lowerWord) {
        auto it = characterMap.find(c);
        if (it != characterMap.end()) {
            int index = it->second;
            Actor* character;
            if(letterSize == 0)
            {
                character = new Actor(Vec2(x,y), basePath_ / "SOS/assets/spriteatlas/letters.tpsheet", index);
            }
            else
            {
                character = new Actor(Vec2(x,y), basePath_ / "SOS/assets/spriteatlas/letters_small.tpsheet", index);
            }
            actors.push_back(character);
            x += letterWidth; // Move to the right for the next character
            if(x > 1600)
            {
                y += letterWidth; // Move down if we exceed screen width
                x = startX;
            }
        }
        else
        {
            x += letterWidth; // Space
        }
    }
}

void Game::drawWordWithHighlight(const std::string& word, int x, int y, bool isSelected) {
    // Clear any existing actors at this position (to refresh selection highlight)
    if (isSelected) {
        // Add a simple visual indicator for the selected item
        Actor* selector = new Actor(Vec2(x - 80, y), basePath_ / "SOS/assets/spriteatlas/letters.tpsheet", characterMap['>']);
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
                // Initialize single player mode with embedded server
                if (!initializeSinglePlayerEmbeddedServer()) {
                    std::cerr << "[Game] Failed to initialize single player mode." << std::endl;
                    // Could show error message in menu or fallback
                    break;
                } else {
                    std::cout << "[Game] Single player mode initialized successfully!" << std::endl;
                }
                // Start single player game
                state = GameState::RUNNING;
                {
                    std::lock_guard<std::mutex> lock(objectsMutex);
                    objects.push_back(std::shared_ptr<Player>(player)); // Add player to objects
                }
                clearActors(); // Clear the menu
                break;
            case MenuOption::MULTIPLAYER:
                // Go to server selection menu
                state = GameState::SERVER_SELECTION;
                selectedServerIndex = 0; // Reset to first server
                serverSelectionOptionChanged = true;
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

void Game::drawServerSelectionMenu(float deltaTime) {
    bool print = false;
    static bool lastPrint = false;
    static float lastTime = 0;
    lastTime += deltaTime;
    if(lastTime > 0.5f)
    {
        if(lastTime > 1.0f)
        {
            lastTime = 0;
        }
        print = true;
    }
    
    if(actors.size() != 0 && !serverSelectionOptionChanged && lastPrint == print)
        return;
    
    // Clear previous actors if selection changed or print state changed
    if (serverSelectionOptionChanged || lastPrint != print) {
        clearActors();
        serverSelectionOptionChanged = false;
    }
    lastPrint = print;

    // Draw the server selection screen
    drawWord("Select Server", 300, 100);
    
    // Draw server list
    int yOffset = 200;
    for (size_t i = 0; i < serverConfig.getServerCount(); ++i) {
        const ServerInfo* server = serverConfig.getServer(i);
        if (server) {
            bool isSelected = (i == selectedServerIndex);
            
            // Draw server name
            drawWordWithHighlight(server->name, 200, yOffset, isSelected);
            
            // Draw server address:port on the right
            std::string addressPort = server->address + ":" + std::to_string(server->port);
            drawWord(addressPort, 600, yOffset+60, 1);
            
            // Draw description if available
            if (!server->description.empty() && isSelected) {
                drawWord(server->description, 200, yOffset + 100, 1);
            }
            
            yOffset += (isSelected && !server->description.empty()) ? 200 : 150;
        }
    }

    if(print)
    {
        drawWord("Use UP/DOWN to select", 200, yOffset + 40, 1); 
        drawWord("Square to connect", 200, yOffset + 80, 1);
        drawWord("Circle to go back", 200, yOffset + 120, 1);
    }
}

void Game::handleServerSelectionInput(float deltaTime) {
    // Update cooldown timer
    if (menuInputCooldown > 0) {
        menuInputCooldown -= deltaTime;
        return;
    }
    
    // Check for up/down input
    bool inputDetected = false;
    
    if (input->get_up()) {
        // Move selection up
        if (selectedServerIndex > 0) {
            selectedServerIndex--;
        } else {
            selectedServerIndex = serverConfig.getServerCount() - 1;
        }
        inputDetected = true;
    } else if (input->get_down()) {
        // Move selection down
        selectedServerIndex = (selectedServerIndex + 1) % serverConfig.getServerCount();
        inputDetected = true;
    } else if (input->get_attack()) {
        // Connect to selected server
        const ServerInfo* selectedServer = serverConfig.getServer(selectedServerIndex);
        if (selectedServer) {
            std::cout << "[Game] Connecting to server: " << selectedServer->name 
                      << " (" << selectedServer->address << ":" << selectedServer->port << ")" << std::endl;
            uint16_t playID;
            if (player) {
                playID = player->getObjID(); // Use existing player ID
            } else {
                // Generate a new random player ID if not already set
                playID = 65000;
            }
            if (!initializeServerConnection(selectedServer->address, selectedServer->port, playID)) {
                std::cerr << "[Game] Failed to connect to server: " << selectedServer->name << std::endl;
                // Could show error message and stay in server selection
                // For now, go back to main menu
                state = GameState::MENU;
                menuOptionChanged = true;
            } else {
                std::cout << "[Game] Successfully connected to server: " << selectedServer->name << std::endl;
                // Start multiplayer game
                state = GameState::RUNNING;
                clearActors(); // Clear the menu
            }
        }
        inputDetected = true;
    } else if (input->get_left() || input->get_right()) {
        // Go back to main menu (using left/right as back button for now)
        // Note: You might want to add a dedicated back button input
        state = GameState::MENU;
        menuOptionChanged = true;
        clearActors();
        inputDetected = true;
    }
    
    if (inputDetected) {
        menuInputCooldown = MENU_INPUT_DELAY;
        serverSelectionOptionChanged = true;
    }
}

void Game::updatePlayer(uint16_t playerId, const Vec2& position) {
    // Find the player object by ID
    if(player)
    {
        if(player->getObjID() != playerId)
        {
            std::cerr << "[Game] Player ID mismatch: expected " << player->getObjID() << ", received " << playerId << std::endl;
            return; // Ignore update if IDs don't match
        }
        player->setposition(position);
    }
    else
    {
        // If player is not initialized, create a new one
        player = new Player(position.x, position.y, playerId);
        player->setInput(input); // Set input handler for the player
        multiplayerManager->setLocalPlayer(player); // Set the local player in the multiplayer manager
        multiplayerManager->setPlayerInput(input); // Set input for multiplayer manager
        objects.push_back(std::shared_ptr<Player>(player)); // Add player to objects
    }
}