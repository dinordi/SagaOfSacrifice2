// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include "object.h"
#include <iostream>
#include <algorithm>

#include "interfaces/playerInput.h"
#include "objects/player.h"
#include "objects/platform.h"
#include "collision/CollisionManager.h"
#include "network/MultiplayerManager.h"

class Game {
public:
    Game(PlayerInput* input);
    ~Game();

    void update(uint64_t deltaTime);
    bool isRunning() const;
    std::string generateRandomPlayerId();
    
    // Multiplayer functionality
    bool initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId);
    void shutdownMultiplayer();
    bool isMultiplayerActive() const;
    MultiplayerManager* getMultiplayerManager() { return multiplayerManager.get(); }
    void sendChatMessage(const std::string& message);
    void setChatMessageHandler(std::function<void(const std::string& sender, const std::string& message)> handler);
    std::vector<Object*>& getObjects();
    std::vector<Actor*>& getActors();


private:
    void drawWord(const std::string& word, int x, int y);
    void mapCharacters();

    bool running;
    bool isPaused = false; // Added isPaused member variable
    std::vector<Object*> objects;
    std::vector<Actor*> actors; //Non-interactive objects i.e. text, background, etc.
    PlayerInput* input;
    CollisionManager* collisionManager;
    Player* player;

    std::vector<Object*> mpl_objects;
    
    // Multiplayer support
    std::unique_ptr<MultiplayerManager> multiplayerManager;
    bool multiplayerActive;
    
    // Handle messages from remote players
    void handleNetworkMessages();
    
    // Update remote player positions
    void updateRemotePlayers(const std::map<std::string, std::unique_ptr<RemotePlayer>>& remotePlayers);
    
    // Render remote players
    // void renderRemotePlayers();
    
    SpriteData* characters;
    std::map<char, int> characterMap;
};

#endif // GAME_H