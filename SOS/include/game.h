// SOS/include/game.h

#ifndef GAME_H
#define GAME_H

#include <vector>
#include "object.h"
#include <iostream>

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
    void render();
    bool isRunning() const;
    std::vector<Object*>& getObjects();
    
    // Multiplayer functionality
    bool initializeMultiplayer(const std::string& serverAddress, int serverPort, const std::string& playerId);
    void shutdownMultiplayer();
    bool isMultiplayerActive() const;
    MultiplayerManager* getMultiplayerManager() { return multiplayerManager.get(); }
    void sendChatMessage(const std::string& message);
    void setChatMessageHandler(std::function<void(const std::string& sender, const std::string& message)> handler);

private:
    bool running;
    std::vector<Object*> objects;
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
    void updateRemotePlayers(uint64_t deltaTime);
    
    // Render remote players
    void renderRemotePlayers();
};

#endif // GAME_H