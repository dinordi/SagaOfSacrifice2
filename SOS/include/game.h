// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "object.h"
#include "interfaces/playerInput.h"
#include "objects/player.h"
#include "objects/minotaur.h"
#include "objects/tile.h"
#include "collision/CollisionManager.h"
#include "network/MultiplayerManager.h"
#include "LocalServerManager.h"
#include "player_manager.h"
#include "ServerConfig.h"
#include "level_manager.h"

enum class GameState {
    RUNNING,
    MENU,
    SERVER_SELECTION
};

enum class MenuOption {
    SINGLEPLAYER = 0,
    MULTIPLAYER = 1,
    EXIT = 2,
    CREDITS = 3,
    COUNT // Used to get the number of menu options
};

class Game {
public:
    Game();
    ~Game();

    void update(float deltaTime);
    bool isRunning() const;
    std::string generateRandomPlayerId();
    
    // Multiplayer functionality
    bool initializeServerConnection(const std::string& serverAddress, int serverPort, const uint16_t playerId);

    // New: Initialize single player mode with embedded server
    bool initializeSinglePlayerEmbeddedServer();
    
    // Set multiplayer configuration (to be used when menu option is selected)
    void setMultiplayerConfig(bool enableMultiplayer, const std::string& serverAddress, int serverPort);
    void setPlayerInput(PlayerInput* input) { this->input = input; }
    // Initialize server configuration from file
    void initializeServerConfig(const std::string& basePath);

    // Initialize SpriteSheets
    void initializeSpriteSheets();
    
    void shutdownServerConnection();
    bool isServerConnection() const;
    MultiplayerManager* getMultiplayerManager() { return multiplayerManager.get(); }

    // Methods to handle chatting
    void sendChatMessage(const std::string& message);
    void setChatMessageHandler(std::function<void(const uint16_t sender, const std::string& message)> handler);

    std::vector<std::shared_ptr<Object>>& getObjects();
    std::vector<Actor*>& getActors();
    void clearActors();

    Player* getPlayer() const { return player; }

    void movePlayerToEnd();

    // Method to add a game object dynamically
    void addObject(std::shared_ptr<Object> object);
    
    // Mutex to protect access to game objects
    std::mutex& getObjectsMutex() { return objectsMutex; }
    std::mutex& getActorsMutex() { return actorsMutex; }
    Player* getPlayer() { return player; }

    // Static instance getter for singleton access
    static Game& getInstance() { static Game instance_; return instance_; }

    void updatePlayer(uint16_t playerId, const Vec2& position);

private:
    void drawWord(const std::string& word, int x, int y, int letterSize = 0);
    void sortObjects();                     // sort objects by layer
    void drawWordWithHighlight(const std::string& word, int x, int y, bool isSelected);
    void mapCharacters();
    void drawMenu(float deltaTime);
    void handleMenuInput(float deltaTime);
    
    // Server selection methods
    void drawServerSelectionMenu(float deltaTime);
    void handleServerSelectionInput(float deltaTime);
    
    MenuOption selectedOption = MenuOption::SINGLEPLAYER;
    bool menuOptionChanged = true;
    float menuInputCooldown = 0.0f;
    const float MENU_INPUT_DELAY = 0.2f; // Cooldown in seconds to prevent rapid selection changes
    
    // Local client-side prediction methods
    void predictLocalPlayerMovement(float deltaTime);
    void reconcileWithServerState(float deltaTime);

    GameState state;
    bool running;
    bool isPaused = false;
    
    std::vector<std::shared_ptr<Object>> objects;
    std::mutex objectsMutex; // Mutex to protect access to objects vector
    std::mutex actorsMutex; // Mutex to protect access to actors vector

    std::vector<Actor*> actors; //Non-interactive objects i.e. text, background, etc.
    PlayerInput* input;
    CollisionManager* collisionManager;
    Player* player;
    
    // Local server management
    std::unique_ptr<LocalServerManager> localServerManager;
    bool usingSinglePlayerServer = false;
    
    // Multiplayer support
    std::unique_ptr<MultiplayerManager> multiplayerManager;
    bool multiplayerActive;
    
    // Multiplayer configuration (for deferred connection)
    bool multiplayerConfigured = false;
    std::string configuredServerAddress;
    int configuredServerPort;
    
    // Server selection
    ServerConfig serverConfig;
    size_t selectedServerIndex = 0;
    bool serverSelectionOptionChanged = true;
    
    // Update remote players
    void updateRemotePlayers(const std::map<uint16_t, std::shared_ptr<Player>>& remotePlayers);
    
    SpriteData* characters;
    std::map<char, int> characterMap;

    // Default ports
    static const int LOCAL_SERVER_PORT = 8080;
    
    std::filesystem::path basePath_; // Base path for all file operations
    // Static instance for singleton pattern
    std::unique_ptr<LevelManager> levelManager_;
    
};

#endif // GAME_H