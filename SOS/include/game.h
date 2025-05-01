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
#include "objects/platform.h"
#include "collision/CollisionManager.h"
#include "network/MultiplayerManager.h"
#include "LocalServerManager.h"

class Game {
public:
    Game(PlayerInput* input, std::string playerID);
    ~Game();

    void update(uint64_t deltaTime);
    bool isRunning() const;
    std::string generateRandomPlayerId();
    
    // Multiplayer functionality
    bool initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId);
    
    // New: Initialize single player mode with embedded server
    bool initializeSinglePlayerEmbedded();
    
    void shutdownMultiplayer();
    bool isMultiplayerActive() const;
    MultiplayerManager* getMultiplayerManager() { return multiplayerManager.get(); }
    void sendChatMessage(const std::string& message);
    void setChatMessageHandler(std::function<void(const std::string& sender, const std::string& message)> handler);
    std::vector<std::shared_ptr<Object>>& getObjects();
    std::vector<Actor*>& getActors();


private:
    void drawWord(const std::string& word, int x, int y);
    void mapCharacters();
    
    // Handle server-driven state updates
    void handleServerStateUpdate(const std::vector<uint8_t>& stateData);
    
    // Local client-side prediction methods
    void predictLocalPlayerMovement(uint64_t deltaTime);
    void reconcileWithServerState();

    bool running;
    bool isPaused = false;
    std::vector<std::shared_ptr<Object>> objects;
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
    
    // Handle messages from remote players
    void handleNetworkMessages();
    
    // Update remote players
    void updateRemotePlayers(const std::map<std::string, std::unique_ptr<RemotePlayer>>& remotePlayers);
    
    SpriteData* characters;
    std::map<char, int> characterMap;

    // Default ports
    static const int LOCAL_SERVER_PORT = 8081;
};

#endif // GAME_H