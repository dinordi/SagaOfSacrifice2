#include "network/EmbeddedServer.h"
#include "collision/CollisionManager.h"
#include "objects/player.h"
#include "objects/platform.h"
#include <iostream>
#include <chrono>
#include <array>
#include <boost/bind.hpp>
#include <future>

// Buffer size for incoming messages
constexpr size_t MAX_MESSAGE_SIZE = 1024;

EmbeddedServer::EmbeddedServer(int port)
    : port_(port), 
      running_(false),
      gameLoopRunning_(false),
      collisionManager_(std::make_shared<CollisionManager>()) {
    std::cout << "[EmbeddedServer] Created on port " << port << std::endl;
    
    // Initialize the acceptor but don't start listening until start() is called
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_);
}

EmbeddedServer::~EmbeddedServer() {
    stop();
}

void EmbeddedServer::start() {
    if (running_) {
        std::cerr << "[EmbeddedServer] Already running" << std::endl;
        return;
    }
    
    try {
        // Setup TCP acceptor
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port_);
        std::cout << "[EmbeddedServer] Attempting to bind to port " << port_ << std::endl;
        
        acceptor_->open(endpoint.protocol());
        acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_->bind(endpoint);
        
        std::cout << "[EmbeddedServer] Successfully bound to port " << port_ << std::endl;
        acceptor_->listen();
        std::cout << "[EmbeddedServer] Now listening on port " << port_ << std::endl;
        
        // Start accepting connections
        startAccept();
        
        // Start io_context in a separate thread
        io_thread_ = std::make_unique<std::thread>([this]() {
            try {
                std::cout << "[EmbeddedServer] Network thread started" << std::endl;
                io_context_.run();
                std::cout << "[EmbeddedServer] Network thread stopped" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EmbeddedServer] Network thread error: " << e.what() << std::endl;
            }
        });
        
        running_ = true;
        std::cout << "[EmbeddedServer] Started" << std::endl;
        
        // Initialize game world with static objects
        createInitialGameObjects();
        
        // Start game loop in a separate thread
        gameLoopRunning_ = true;
        lastUpdateTime_ = std::chrono::high_resolution_clock::now();
        
        gameLoopThread_ = std::make_unique<std::thread>([this]() {
            run();
        });
    } catch (const std::exception& e) {
        std::cerr << "[EmbeddedServer] Failed to start server: " << e.what() << std::endl;
        // Make sure resources are cleaned up
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }
        io_context_.stop();
        running_ = false;
    }
}

void EmbeddedServer::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "[EmbeddedServer] Stopping..." << std::endl;
    
    // Set flags first to ensure loops break
    running_ = false;
    gameLoopRunning_ = false;
    
    // Add a timeout for thread joining
    const auto timeout = std::chrono::seconds(3);
    
    // Stop network operations first
    if (acceptor_ && acceptor_->is_open()) {
        try {
            boost::system::error_code ec;
            acceptor_->cancel(ec); // Cancel any pending async operations
            if (ec) {
                std::cerr << "[EmbeddedServer] Error cancelling acceptor: " << ec.message() << std::endl;
            }
            
            acceptor_->close(ec);
            if (ec) {
                std::cerr << "[EmbeddedServer] Error closing acceptor: " << ec.message() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error closing acceptor: " << e.what() << std::endl;
        }
    }
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        for (auto& client : clientSockets_) {
            try {
                if (client.second && client.second->is_open()) {
                    boost::system::error_code ec;
                    client.second->cancel(ec); // Cancel any pending async operations
                    client.second->close(ec);
                }
            } catch (const std::exception& e) {
                std::cerr << "[EmbeddedServer] Error closing client socket: " << e.what() << std::endl;
            }
        }
        clientSockets_.clear();
    }
    
    // Cancel all pending operations and stop the io_context
    io_context_.stop();
    
    // Join the io_thread with timeout using standard C++ techniques
    if (io_thread_ && io_thread_->joinable()) {
        // Create a future to track the joining
        std::promise<void> exitPromise;
        std::future<void> exitFuture = exitPromise.get_future();
        
        // Create a thread to join the IO thread safely
        std::thread joinThread([&exitPromise, this]() {
            if (io_thread_->joinable()) {
                try {
                    io_thread_->join();
                    exitPromise.set_value();
                } catch (...) {
                    try {
                        exitPromise.set_exception(std::current_exception());
                    } catch (...) {} // Set_exception might throw if promise is already satisfied
                }
            }
        });
        
        // Wait with timeout
        if (exitFuture.wait_for(timeout) == std::future_status::ready) {
            std::cout << "[EmbeddedServer] Network thread joined successfully" << std::endl;
            joinThread.join(); // Safe to join here
        } else {
            std::cerr << "[EmbeddedServer] Network thread join timed out, will be detached" << std::endl;
            joinThread.detach(); // Can't wait for join thread any longer
            // Note: io_thread will leak, but attempting to force terminate it would be unsafe
        }
    }
    
    // Wait for game loop thread to finish with timeout
    if (gameLoopThread_ && gameLoopThread_->joinable()) {
        // Create a future to track the joining
        std::promise<void> exitPromise;
        std::future<void> exitFuture = exitPromise.get_future();
        
        // Create a thread to join the game loop thread safely
        std::thread joinThread([&exitPromise, this]() {
            if (gameLoopThread_->joinable()) {
                try {
                    gameLoopThread_->join();
                    exitPromise.set_value();
                } catch (...) {
                    try {
                        exitPromise.set_exception(std::current_exception());
                    } catch (...) {} // Set_exception might throw if promise is already satisfied
                }
            }
        });
        
        // Wait with timeout
        if (exitFuture.wait_for(timeout) == std::future_status::ready) {
            std::cout << "[EmbeddedServer] Game loop thread joined successfully" << std::endl;
            joinThread.join(); // Safe to join here
        } else {
            std::cerr << "[EmbeddedServer] Game loop thread join timed out, will be detached" << std::endl;
            joinThread.detach(); // Can't wait for join thread any longer
            // Note: gameLoopThread will leak, but attempting to force terminate it would be unsafe
        }
    }
    
    // Clear game state
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        gameObjects_.clear();
        players_.clear();
    }
    
    std::cout << "[EmbeddedServer] Stopped" << std::endl;
}

void EmbeddedServer::startAccept() {
    // Create a new socket for the incoming connection
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
    
    // Set up an asynchronous accept operation to wait for a new connection
    acceptor_->async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handleAccept(error, socket);
        });
}

void EmbeddedServer::handleAccept(const boost::system::error_code& error, 
                                  std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    if (!error) {
        std::cout << "[EmbeddedServer] New client connected: " 
                  << socket->remote_endpoint().address().to_string() << ":"
                  << socket->remote_endpoint().port() << std::endl;
        
        // Handle the connection in a separate function
        handleClientConnection(socket);
    } else {
        std::cerr << "[EmbeddedServer] Accept error: " << error.message() << std::endl;
    }
    
    // Continue accepting more connections if still running
    if (running_) {
        startAccept();
    }
}

void EmbeddedServer::handleClientConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    // Generate a client ID based on the remote endpoint
    std::string clientId = socket->remote_endpoint().address().to_string() + 
                           ":" + std::to_string(socket->remote_endpoint().port());
    
    // Store the socket
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        clientSockets_[clientId] = socket;
    }
    
    // Start reading from the socket
    auto buffer = std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
    socket->async_read_some(boost::asio::buffer(*buffer),
        [this, socket, buffer, clientId](const boost::system::error_code& error, std::size_t bytes_transferred) {
            handleRead(socket, error, bytes_transferred);
        });
    
    // Create a connect message for this client
    NetworkMessage connectMsg;
    connectMsg.type = MessageType::CONNECT;
    connectMsg.senderId = clientId;
    
    // Process the connection
    processMessage(connectMsg);
}

void EmbeddedServer::handleRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                               const boost::system::error_code& error,
                               size_t bytes_transferred) {
    if (!error) {
        // Get the client ID
        std::string clientId;
        try {
            clientId = socket->remote_endpoint().address().to_string() + 
                      ":" + std::to_string(socket->remote_endpoint().port());
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error getting client ID: " << e.what() << std::endl;
            return;
        }
        
        // Process the message
        // In a real implementation, we would deserialize the binary message here
        // For simplicity, we'll assume a very basic message format
        // ...
        
        // Continue reading
        auto buffer = std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
        socket->async_read_some(boost::asio::buffer(*buffer),
            [this, socket, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                handleRead(socket, error, bytes_transferred);
            });
    } else if (error == boost::asio::error::eof || 
              error == boost::asio::error::connection_reset) {
        // Client disconnected
        std::string clientId;
        try {
            clientId = socket->remote_endpoint().address().to_string() + 
                      ":" + std::to_string(socket->remote_endpoint().port());
                      
            std::cout << "[EmbeddedServer] Client disconnected: " << clientId << std::endl;
            
            // Remove the client from our maps
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex_);
                clientSockets_.erase(clientId);
            }
            
            // Create a disconnect message
            NetworkMessage disconnectMsg;
            disconnectMsg.type = MessageType::DISCONNECT;
            disconnectMsg.senderId = clientId;
            
            // Process the disconnection
            processMessage(disconnectMsg);
            
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error handling disconnection: " << e.what() << std::endl;
        }
    } else {
        // Some other error
        std::cerr << "[EmbeddedServer] Read error: " << error.message() << std::endl;
    }
}

void EmbeddedServer::sendToClient(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                 const NetworkMessage& message) {
    // In a real implementation, we would serialize the message to a binary format
    // For simplicity, we'll use a very basic format
    std::string serialized = std::to_string(static_cast<int>(message.type)) + "|" +
                           message.senderId + "|";
    
    if (!message.data.empty()) {
        serialized.append(message.data.begin(), message.data.end());
    }
    
    serialized += "\n";
    
    try {
        boost::asio::async_write(*socket, 
            boost::asio::buffer(serialized),
            [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (error) {
                    std::cerr << "[EmbeddedServer] Write error: " << error.message() << std::endl;
                }
            });
    } catch (const std::exception& e) {
        std::cerr << "[EmbeddedServer] Error sending message to client: " << e.what() << std::endl;
    }
}

void EmbeddedServer::run() {
    std::cout << "[EmbeddedServer] Game loop started" << std::endl;
    
    const auto tickDuration = std::chrono::milliseconds(1000 / SERVER_TICK_RATE);
    
    while (gameLoopRunning_) {
        auto now = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime_).count();
        lastUpdateTime_ = now;
        
        // Update game state
        updateGameState(deltaTime);
        
        // Sleep until next tick
        std::this_thread::sleep_for(tickDuration);
    }
    
    std::cout << "[EmbeddedServer] Game loop stopped" << std::endl;
}

bool EmbeddedServer::isRunning() const {
    return running_;
}

void EmbeddedServer::addPlayer(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    if (players_.find(playerId) != players_.end()) {
        std::cerr << "[EmbeddedServer] Player " << playerId << " already exists" << std::endl;
        return;
    }
    
    std::cout << "[EmbeddedServer] Adding player " << playerId << std::endl;
    
    // Create a new player object
    auto player = std::make_shared<Player>(Vec2(500, 100), 
                                           new SpriteData(std::string("playermap"), 128, 128, 5), 
                                           playerId);
    
    // Add player to the game
    players_[playerId] = player;
    gameObjects_.push_back(player);
    
    // Send a player joined message to all clients
    NetworkMessage joinMsg;
    joinMsg.type = MessageType::PLAYER_JOINED;
    joinMsg.senderId = playerId;
    
    if (messageCallback_) {
        messageCallback_(joinMsg);
    }
}

void EmbeddedServer::removePlayer(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    auto playerIt = players_.find(playerId);
    if (playerIt == players_.end()) {
        std::cerr << "[EmbeddedServer] Player " << playerId << " not found" << std::endl;
        return;
    }
    
    std::cout << "[EmbeddedServer] Removing player " << playerId << std::endl;
    
    // Remove player from game objects
    auto player = playerIt->second;
    auto objIt = std::find(gameObjects_.begin(), gameObjects_.end(), player);
    if (objIt != gameObjects_.end()) {
        gameObjects_.erase(objIt);
    }
    
    // Remove from players map
    players_.erase(playerIt);
    
    // Send a player left message to all clients
    NetworkMessage leaveMsg;
    leaveMsg.type = MessageType::PLAYER_LEFT;
    leaveMsg.senderId = playerId;
    
    if (messageCallback_) {
        messageCallback_(leaveMsg);
    }
}

void EmbeddedServer::processMessage(const NetworkMessage& message) {
    // Process based on message type
    switch (message.type) {
        case MessageType::CONNECT:
            addPlayer(message.senderId);
            break;
            
        case MessageType::DISCONNECT:
            removePlayer(message.senderId);
            break;
            
        case MessageType::PLAYER_INPUT:
            processPlayerInput(message.senderId, message);
            break;
            
        case MessageType::CHAT:
            // Just relay chat messages to all clients
            if (messageCallback_) {
                messageCallback_(message);
            }
            break;
            
        default:
            std::cerr << "[EmbeddedServer] Unknown message type: " << static_cast<int>(message.type) << std::endl;
            break;
    }
}

void EmbeddedServer::setMessageCallback(std::function<void(const NetworkMessage&)> callback) {
    messageCallback_ = callback;
}

void EmbeddedServer::createInitialGameObjects() {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    std::cout << "[EmbeddedServer] Creating initial game objects" << std::endl;
    
    // Clear any existing objects
    gameObjects_.clear();
    
    // Create platform at the bottom of the screen
    auto platform = std::make_shared<Platform>(400, 500, 
                                             new SpriteData(std::string("platform"), 600, 32, 1),
                                             "platform_ground");
    gameObjects_.push_back(platform);
    
    // Add more platforms as needed
    auto platform2 = std::make_shared<Platform>(200, 400, 
                                              new SpriteData(std::string("platform"), 200, 32, 1),
                                              "platform_1");
    gameObjects_.push_back(platform2);
    
    auto platform3 = std::make_shared<Platform>(600, 350, 
                                              new SpriteData(std::string("platform"), 200, 32, 1),
                                              "platform_2");
    gameObjects_.push_back(platform3);
}

void EmbeddedServer::updateGameState(uint64_t deltaTime) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Update all game objects
    for (auto& object : gameObjects_) {
        object->update(deltaTime);
    }
    
    // Detect and resolve collisions
    detectAndResolveCollisions();
    
    // Send game state to clients periodically
    static uint64_t updateTimer = 0;
    updateTimer += deltaTime;
    
    if (updateTimer >= 50) { // Send updates 20 times per second
        updateTimer = 0;
        sendGameStateToClients();
    }
}

void EmbeddedServer::detectAndResolveCollisions() {
    collisionManager_->detectCollisions(gameObjects_);
}

void EmbeddedServer::sendGameStateToClients() {
    // Create state update message
    NetworkMessage stateMsg;
    stateMsg.type = MessageType::GAME_STATE;
    stateMsg.senderId = "server";
    
    // We would normally serialize all game objects here
    // For now, just send player positions
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        for (const auto& playerPair : players_) {
            const auto& playerId = playerPair.first;
            const auto& player = playerPair.second;
            
            // Serialize player state
            const Vec2& pos = player->getposition();
            const Vec2& vel = player->getvelocity();
            
            // Format: playerId|x|y|vx|vy
            std::string playerData = playerId + "|" +
                                  std::to_string(pos.x) + "|" +
                                  std::to_string(pos.y) + "|" +
                                  std::to_string(vel.x) + "|" +
                                  std::to_string(vel.y);
            
            // Add to state data
            stateMsg.data.insert(stateMsg.data.end(), playerData.begin(), playerData.end());
            stateMsg.data.push_back(','); // Separator between players
        }
    }
    
    // Send to each connected client
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        for (const auto& clientPair : clientSockets_) {
            auto& socket = clientPair.second;
            if (socket && socket->is_open()) {
                sendToClient(socket, stateMsg);
            }
        }
    }
    
    // Also call the message callback if it exists
    if (messageCallback_) {
        messageCallback_(stateMsg);
    }
}

void EmbeddedServer::processPlayerInput(const std::string& playerId, const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // Find the player
    auto playerIt = players_.find(playerId);
    if (playerIt == players_.end()) {
        std::cerr << "[EmbeddedServer] Player not found for input: " << playerId << std::endl;
        return;
    }
    
    auto player = playerIt->second;
    
    // Deserialize input data
    // Format is expected to be: jump|left|right|attack
    if (message.data.size() < 7) {
        std::cerr << "[EmbeddedServer] Invalid input data size" << std::endl;
        return;
    }
    
    // Extract input values (this is a simplified example)
    bool jump = message.data[0] == '1';
    bool left = message.data[2] == '1';
    bool right = message.data[4] == '1';
    bool attack = message.data[6] == '1';
    
    // Create a temporary input object to handle player movement
    class TempInput : public PlayerInput {
    public:
        void readInput() override {}
        
        void setInputs(bool j, bool l, bool r, bool a) {
            set_jump(j);
            set_left(l);
            set_right(r);
            set_attack(a);
        }
    };
    
    auto input = std::make_unique<TempInput>();
    input->setInputs(jump, left, right, attack);
    
    // Let the player process the input
    player->handleInput(input.get(), 16); // Assume ~60fps
    
    // The player's update function will be called in the updateGameState method
}