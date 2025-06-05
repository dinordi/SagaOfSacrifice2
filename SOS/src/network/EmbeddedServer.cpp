#include <iostream>
#include <chrono>
#include <array>
#include <future>

#include <boost/bind.hpp>

#include "network/EmbeddedServer.h"
#include "network/NetworkConfig.h"
#include "collision/CollisionManager.h"
#include "objects/player.h"
#include "objects/tile.h"
#include "player_manager.h"

// Buffer size for incoming messages
constexpr size_t MAX_MESSAGE_SIZE = 1024;

EmbeddedServer::EmbeddedServer(int port, const std::filesystem::path& basePath)
    : port_(port), 
      running_(false),
      gameLoopRunning_(false),
      levelManager_(std::make_shared<LevelManager>(basePath)),
      collisionManager_(std::make_shared<CollisionManager>()) {
    std::cout << "[EmbeddedServer] Created on port " << port << std::endl;
  
    
    // Initialize the acceptor but don't start listening until start() is called
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_);
    serverId_ = Object::getNextObjectID(); // Assign a unique server ID
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
        std::cout << "[EmbeddedServer] All client connections closed" << std::endl;
    }
    
    // Cancel all pending operations and stop the io_context
    io_context_.stop();
    
    // Join the io_thread with a simple timeout approach
    if (io_thread_ && io_thread_->joinable()) {
        
        // Check if thread ID is valid and thread is actually running
        std::thread::id thread_id = io_thread_->get_id();
        
        // Try a direct join first with short timeout
        bool joined = false;
        {
            
            try {
                if (io_thread_->joinable()) {
                    io_thread_->join();
                    joined = true;
                } else {
                    std::cout << "[EmbeddedServer] IO thread not joinable (despite earlier check), skipping join" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[EmbeddedServer] First join attempt failed with error: " << e.what() << std::endl;
            }
        }
        
        // If first attempt failed, try with timeout approach
        if (!joined) {
            std::cout << "[EmbeddedServer] First join attempt failed, trying with timeout approach" << std::endl;
            
            const auto timeout = std::chrono::seconds(3);
            std::thread* thread_ptr = io_thread_.get();
            auto start_time = std::chrono::steady_clock::now();
            
            // Only continue if thread is still joinable
            if (!thread_ptr->joinable()) {
                std::cout << "[EmbeddedServer] Thread no longer joinable after first attempt, skipping further join attempts" << std::endl;
            } else {
                while (thread_ptr->joinable()) {
                    // Try to join with a small timeout to keep checking if we should give up
                    if (std::chrono::steady_clock::now() - start_time > timeout) {
                        thread_ptr->detach();  // Detach explicitly instead of leaking
                        break; // Give up after timeout
                    }
                    
                    // Try to join with a very short timeout
                    try {
                        // Thread is detached in destructor if not joined here
                        if (thread_ptr->joinable()) {
                            thread_ptr->join();
                            // std::cout << "[EmbeddedServer] IO thread joined successfully" << std::endl;
                        }
                        break; // We joined successfully
                    } catch (const std::exception& e) {
                        std::cerr << "[EmbeddedServer] Error joining IO thread: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }
        }
    } else {
        std::cout << "[EmbeddedServer] IO thread is null or not joinable, skipping join" << std::endl;
    }
    
    // Wait for game loop thread to finish with timeout
    if (gameLoopThread_ && gameLoopThread_->joinable()) {
        
        // Check if thread ID is valid
        std::thread::id thread_id = gameLoopThread_->get_id();
        
        // Try a direct join first with short timeout
        bool joined = false;
        {
            
            try {
                if (gameLoopThread_->joinable()) {
                    gameLoopThread_->join();
                    joined = true;
                } else {
                    std::cout << "[EmbeddedServer] Game loop thread not joinable (despite earlier check), skipping join" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[EmbeddedServer] First game loop join attempt failed with error: " << e.what() << std::endl;
            }
        }
        
        // If first attempt failed, try with timeout approach
        if (!joined) {
            std::cout << "[EmbeddedServer] First game loop join attempt failed, trying with timeout approach" << std::endl;
            
            const auto timeout = std::chrono::seconds(3);
            std::thread* thread_ptr = gameLoopThread_.get();
            auto start_time = std::chrono::steady_clock::now();
            
            // Only continue if thread is still joinable
            if (!thread_ptr->joinable()) {
                std::cout << "[EmbeddedServer] Game loop thread no longer joinable after first attempt, skipping further join attempts" << std::endl;
            } else {
                while (thread_ptr->joinable()) {
                    // Try to join with a small timeout to keep checking if we should give up
                    if (std::chrono::steady_clock::now() - start_time > timeout) {
                        thread_ptr->detach();  // Detach explicitly instead of leaking
                        break; // Give up after timeout
                    }
                    
                    // Try to join with a very short timeout
                    try {
                        if (thread_ptr->joinable()) {
                            thread_ptr->join();
                            // std::cout << "[EmbeddedServer] Game loop thread joined successfully" << std::endl;
                        }
                        break; // We joined successfully
                    } catch (const std::exception& e) {
                        std::cerr << "[EmbeddedServer] Error joining game loop thread: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }
        }
    } else {
        std::cout << "[EmbeddedServer] Game loop thread is null or not joinable, skipping join" << std::endl;
    }
    
    // Clear game state
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        // Clear game objects and players
        levelManager_->removeAllPlayersFromCurrentLevel();
        levelManager_->removeAllObjectsFromCurrentLevel();
    }
    
    std::cout << "[EmbeddedServer] Stopped" << std::endl;
}

void EmbeddedServer::processMessage(const NetworkMessage& message) {
    // Process based on message type
    switch (message.type) {
        case MessageType::CONNECT: {
            std::cout << "[EmbeddedServer] Processing connect message from client" << std::endl;
            uint16_t assignedPlayerId = message.senderId;
            std::cout << "[EmbeddedServer] Assigned player ID: " << assignedPlayerId << std::endl;
            std::shared_ptr<boost::asio::ip::tcp::socket> clientSocket;
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex_);
                auto it = clientSockets_.find(assignedPlayerId);
                if (it != clientSockets_.end()) {
                    clientSocket = it->second;
                }
            }
            if (clientSocket) {
                addPlayer(assignedPlayerId);
                sendFullGameStateToClient(assignedPlayerId);
            } else {
                std::cerr << "[EmbeddedServer] Error: Failed to find assigned player ID for connecting client" << std::endl;
            }
            break;
        }
            
        case MessageType::DISCONNECT:
            std::cout << "[EmbeddedServer] Processing disconnect message from " << message.senderId << std::endl;
            // Remove the player from the game
            removePlayer(message.senderId);
            // Remove the socket from the clientSockets_ map
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex_);
                clientSockets_.erase(message.senderId);
            }
            break;
            
        case MessageType::PLAYER_INPUT:
            processPlayerInput(message.senderId, message);
            break;
        case MessageType::PLAYER_POSITION:
            processPlayerPosition(message.senderId, message);
            break;
        case MessageType::ENEMY_STATE_UPDATE:
            // Process player actions, including enemy state updates
            processEnemyState(message.senderId, message);
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


void EmbeddedServer::removePlayer(const uint16_t playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    auto& pm = PlayerManager::getInstance();
    auto playerIt = pm.getPlayer(playerId);
    if (playerIt == nullptr) {
        std::cerr << "[EmbeddedServer] Player " << playerId << " not found" << std::endl;
        return;
    }
    
    std::cout << "[EmbeddedServer] Removing player " << playerId << std::endl;
    
    // Remove player from game objects
    //auto player = playerIt;
    pm.removePlayer(playerId);
    //auto objIt = std::find(gameObjects_.begin(), gameObjects_.end(), player);
    // if (objIt != gameObjects_.end()) {
    //     gameObjects_.erase(objIt);
    // }
    
    // Remove from players map
    //players_.erase(playerIt);
    
    // Send a player left message to all clients
    NetworkMessage leaveMsg;
    leaveMsg.type = MessageType::PLAYER_LEFT;
    leaveMsg.senderId = playerId;
    
    if (messageCallback_) {
        messageCallback_(leaveMsg);
    }
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
    // Generate a unique player ID for this client (now using uint16_t)
    uint16_t generatedPlayerId = Object::getNextObjectID();
    std::cout << "[EmbeddedServer] New connection from: "
              << socket->remote_endpoint().address().to_string() << ":"
              << socket->remote_endpoint().port() << std::endl;
    std::cout << "[EmbeddedServer] Generated player ID: " << generatedPlayerId << std::endl;
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        clientSockets_[generatedPlayerId] = socket;
        std::cout << "[EmbeddedServer] Added client socket for player ID: " << generatedPlayerId << std::endl;
    }
    // Start the proper async read sequence to get a complete message
    auto headerBuffer = std::make_shared<std::array<char, sizeof(MessageHeader)>>();
    boost::asio::async_read(*socket, 
        boost::asio::buffer(*headerBuffer, sizeof(MessageHeader)),
        [this, socket, headerBuffer, generatedPlayerId](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (!error && bytes_transferred == sizeof(MessageHeader)) {
                MessageHeader* header = reinterpret_cast<MessageHeader*>(headerBuffer->data());
                uint32_t headerSize = header->size;
                std::cout << "[EmbeddedServer] Received message header with size: " << header->size << " bytes" << std::endl;
                if (header->size > MAX_MESSAGE_SIZE) {
                    std::cerr << "[EmbeddedServer] Message size exceeds maximum limit" << std::endl;
                    boost::system::error_code ec;
                    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    socket->close(ec);
                    return;
                }
                std::cout << "[EmbeddedServer] Valid header size, proceeding to read message body..." << std::endl;
                auto bodyBuffer = std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
                boost::asio::async_read(*socket,
                    boost::asio::buffer(bodyBuffer->data(), header->size),
                    [this, socket, bodyBuffer, headerSize, generatedPlayerId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                        std::cout << "[EmbeddedServer] Received message body with size: " << bytes_transferred << " bytes" << std::endl;
                        std::cout << "[EmbeddedServer] Header size: " << headerSize << std::endl;
                        if (!error && bytes_transferred == headerSize) {
                            std::vector<uint8_t> messageData(
                                bodyBuffer->data(),
                                bodyBuffer->data() + headerSize
                            );
                            std::cout << "Deserializing from sender ID: " << generatedPlayerId << std::endl;
                            NetworkMessage message = deserializeMessage(messageData, generatedPlayerId);
                            std::cout << "[EmbeddedServer] Received first message from client - Type: " << static_cast<int>(message.type)
                                     << " from " << message.senderId << " with data size: " << message.data.size() << " bytes" << std::endl;
                            if (!message.data.empty()) {
                                try {
                                    std::string playerInfo(message.data.begin(), message.data.end());
                                    std::cout << "[EmbeddedServer] Message data: " << playerInfo << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "[EmbeddedServer] Error extracting message data: " << e.what() << std::endl;
                                }
                            }
                            message.senderId = generatedPlayerId; // Set the sender ID to the generated player ID
                            processMessage(message);
                            std::cout << "[EmbeddedServer] First message processed, now waiting for more messages..." << std::endl;
                            handleRead(socket, boost::system::error_code(), 0);
                        } else {
                            if (error) {
                                std::cerr << "[EmbeddedServer] Error reading message body: " << error.message() << std::endl;
                            }
                        }
                    });
            } else {
                if (error) {
                    std::cerr << "[EmbeddedServer] Error reading initial message header: " << error.message() << std::endl;
                }
            }
        });
}

void EmbeddedServer::handleRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                               const boost::system::error_code& error,
                               size_t bytes_transferred) {
    if (!error) {
        uint16_t playerId = 0;
        try {
            // Find the playerId by matching the socket in clientSockets_
            std::lock_guard<std::mutex> lock(clientSocketsMutex_);
            for (const auto& entry : clientSockets_) {
                if (entry.second == socket) {
                    playerId = entry.first;
                    break;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error getting player ID: " << e.what() << std::endl;
            return;
        }
        auto buffer = std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
        boost::asio::async_read(*socket, 
            boost::asio::buffer(buffer->data(), sizeof(MessageHeader)),
            [this, socket, buffer, playerId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error && bytes_transferred == sizeof(MessageHeader)) {
                    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer->data());
                    boost::asio::async_read(*socket,
                        boost::asio::buffer(buffer->data() + sizeof(MessageHeader), header->size),
                        [this, socket, buffer, header, playerId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                            if (!error && bytes_transferred == header->size) {
                                std::vector<uint8_t> messageData(
                                    buffer->data() + sizeof(MessageHeader),
                                    buffer->data() + sizeof(MessageHeader) + header->size
                                );
                                NetworkMessage message = deserializeMessage(messageData, playerId);
                                processMessage(message);
                                handleRead(socket, boost::system::error_code(), 0);
                            } else {
                                if (error) {
                                    std::cerr << "[EmbeddedServer] Error reading message body: " << error.message() << std::endl;
                                }
                            }
                        });
                } else {
                    if (error && error != boost::asio::error::operation_aborted) {
                        std::cerr << "[EmbeddedServer] Error reading message header: " << error.message() << std::endl;
                    }
                }
            });
    } else if (error == boost::asio::error::eof || 
              error == boost::asio::error::connection_reset) {
        uint16_t playerId = 0;
        try {
            std::lock_guard<std::mutex> lock(clientSocketsMutex_);
            for (const auto& entry : clientSockets_) {
                if (entry.second == socket) {
                    playerId = entry.first;
                    break;
                }
            }
            std::cout << "[EmbeddedServer] Client disconnected: " << playerId << std::endl;
            clientSockets_.erase(playerId);
            NetworkMessage disconnectMsg;
            disconnectMsg.type = MessageType::DISCONNECT;
            disconnectMsg.senderId = playerId;
            processMessage(disconnectMsg);
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error handling disconnection: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[EmbeddedServer] Read error: " << error.message() << std::endl;
    }
}

bool EmbeddedServer::sendToClient(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                 const NetworkMessage& message) 
{
    try {
        // Create a shared_ptr to manage the message buffer lifetime
        auto buffer_ptr = std::make_shared<std::vector<uint8_t>>();
        auto& buffer = *buffer_ptr;
        
        // 1. Message type - 1 byte
        buffer.push_back(static_cast<uint8_t>(message.type));
        
        // 2/. Sender ID - 2 bytes
        buffer.push_back(static_cast<uint8_t>((message.senderId >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(message.senderId & 0xFF));


        // 3. Data length - 4 bytes (important!)
        uint32_t dataSize = static_cast<uint32_t>(message.data.size());
        buffer.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(dataSize & 0xFF));
        
        // 4. Data content
        buffer.insert(buffer.end(), message.data.begin(), message.data.end());
        
        // Create the complete message with header + body
        std::vector<uint8_t> completeMessage;
        
        // Set the header size field to the size of the buffer (which is the message body)
        MessageHeader header;
        header.size = static_cast<uint32_t>(buffer.size());
        
        // Create a shared_ptr for the complete message
        auto complete_message_ptr = std::make_shared<std::vector<uint8_t>>();
        auto& complete_message = *complete_message_ptr;
        complete_message.resize(sizeof(header) + buffer.size());
        
        // Copy the header
        std::memcpy(complete_message.data(), &header, sizeof(header));

        // Add the message body after the header
        std::memcpy(complete_message.data() + sizeof(header), buffer.data(), buffer.size());
        
        if(message.type == MessageType::GAME_STATE)
        {
            std::cout << "[EmbeddedServer] Sending message to client - Type: " 
                      << static_cast<int>(message.type) 
                      << ", Sender ID: " << message.senderId 
                      << ", Data size: " << message.data.size() 
                      << " bytes, Total size: " << complete_message.size() 
                      << " bytes" << std::endl;
        }

        // Store the buffer to keep it alive
        {
            std::lock_guard<std::mutex> lock(outgoing_buffers_mutex_);
            outgoing_buffers_.push_back(complete_message_ptr);
        }

        // Send the message asynchronously
        boost::asio::async_write(*socket,
            boost::asio::buffer(complete_message),
            [this, complete_message_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
                // Clean up the buffer only after the operation completes
                {
                    std::lock_guard<std::mutex> lock(outgoing_buffers_mutex_);
                    outgoing_buffers_.erase(
                        std::remove(outgoing_buffers_.begin(), outgoing_buffers_.end(), complete_message_ptr),
                        outgoing_buffers_.end());
                }
                
                if (error) {
                    std::cerr << "[EmbeddedServer] Error sending to client" 
                              << ": " << error.message() << std::endl;
                    
                }
            });
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[EmbeddedServer] Error sending message to client: " << e.what() << std::endl;
     return false;
    }
    
}

void EmbeddedServer::run() {
    std::cout << "[EmbeddedServer] Game loop started" << std::endl;
    
    const float fixedDeltaSeconds = 1.0 / NetworkConfig::Server::TickRate;
    std::cout << "[EmbeddedServer] Fixed delta time: " << fixedDeltaSeconds << " seconds" << std::endl;
    const uint64_t fixedDeltaMilliseconds = static_cast<uint64_t>(fixedDeltaSeconds * 1000);
    unsigned int loopCounter = 0;

    while (gameLoopRunning_) {
        auto before = std::chrono::high_resolution_clock::now();
        try {
            updateGameState(fixedDeltaSeconds); // Always use fixed delta
        }
        catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Exception in game update: " << e.what() << std::endl;
        }
        
        // Check flag again before sleeping to respond faster to shutdown requests
        if (!gameLoopRunning_) {
            std::cout << "[EmbeddedServer] Game loop flag set to false, exiting loop" << std::endl;
            break;
        }
        
        auto after = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(after - before).count();
        // Sleep until next tick
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(fixedDeltaMilliseconds- static_cast<uint64_t>(duration / 1000)));
        }
        catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Exception during game loop sleep: " << e.what() << std::endl;
            std::this_thread::yield();
        }
    }

    std::cout << "[EmbeddedServer] Game loop stopped after " << loopCounter << " iterations" << std::endl;
}

bool EmbeddedServer::isRunning() const {
    return running_;
}

void EmbeddedServer::addPlayer(const uint16_t playerId) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    // 1) Create (or fetch) the player in the global manager:
    auto& pm = PlayerManager::getInstance();
    
    std::shared_ptr<Player> player;
    if (pm.getPlayer(playerId) != nullptr) {
        std::cout << "[EmbeddedServer] Player " << playerId << " already exists, using existing player" << std::endl;
        player = pm.getPlayer(playerId);
    } else {
        // Create a new player using the PlayerManager singleton
        player = pm.createPlayer(playerId, Vec2{100, 100});
        std::cout << "[EmbeddedServer] Created new player " << playerId << std::endl;
        levelManager_->addPlayerToCurrentLevel(playerId);
    }
    
    // Send the player to the client
    sendPlayerToClient(playerId, player.get());
    
    //Broadcast player object to all clients
    NetworkMessage joinMsg;
    joinMsg.type = MessageType::PLAYER_JOINED;
    joinMsg.senderId = 0; // 'server' as 0 or a reserved value

    // Serialize player data
    serializeObject(player, joinMsg.data);

    // Broadcast joinMsg to all clients, except the one that just joined
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        for (const auto& client : clientSockets_) {
            if (client.first != playerId && client.second && client.second->is_open()) {
                sendToClient(client.second, joinMsg);
            }
        }
    }

    if (messageCallback_) {
        messageCallback_(joinMsg);
    }
}

void EmbeddedServer::sendPlayerToClient(const uint16_t playerId, Player* player) {
    if (!player) return;
    NetworkMessage playerMsg;
    playerMsg.type = MessageType::PLAYER_ASSIGN;
    playerMsg.senderId = 0; // 'server' as 0 or a reserved value
    playerMsg.targetId = playerId;
    Vec2 position = player->getcollider().position;
    std::vector<uint8_t> data;
    data.resize(sizeof(float) * 2 + sizeof(uint16_t));
    float posX = position.x;
    float posY = position.y;
    std::memcpy(data.data(), &posX, sizeof(float));
    std::memcpy(data.data() + sizeof(float), &posY, sizeof(float));
    std::memcpy(data.data() + sizeof(float) * 2, &playerId, sizeof(uint16_t));
    playerMsg.data = std::move(data);
    std::lock_guard<std::mutex> lock(clientSocketsMutex_);
    auto it = clientSockets_.find(playerId);
    if (it != clientSockets_.end() && it->second && it->second->is_open()) {
        sendToClient(it->second, playerMsg);
    }
}

void EmbeddedServer::createInitialGameObjects() {
    std::lock_guard<std::mutex> lock(gameStateMutex_);
    
    std::cout << "[EmbeddedServer] Creating initial game objects" << std::endl;
    // initialize the level 
    if(levelManager_->initialize()){
    if(!levelManager_->loadLevel("level1")){
        std::cerr << "[EmbeddedServer] Failed to load level1" << std::endl;
        
    }
    } else {
        std::cerr << "[EmbeddedServer] Failed to initialize level manager" << std::endl;
        
    }

}

void EmbeddedServer::updateGameState(float deltaTime) {
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        if(clientSockets_.empty()) {
            // Print once every 5 seconds if no clients are connected
            static auto last = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last > std::chrono::seconds(5)) {
                last = now;
                std::cout << "[EmbeddedServer] No clients connected, skipping game update" << std::endl;
            }
            return; // nothing to update if no clients are connected
        }
        levelManager_->update(deltaTime);
        // Update all players
        

    }
    // Send game state to clients periodically
    static float updateTimer = 0;
    updateTimer += deltaTime;
    
    if (updateTimer*1000 >= NetworkConfig::Server::StateUpdateInterval) {
        updateTimer = 0;
        sendGameStateToClients();
    }
}

void EmbeddedServer::sendGameStateToClients() {
    // Get objects from the active level
    Level* lvl = levelManager_->getCurrentLevel();

    if (!lvl) {
        return; // nothing to send if no level loaded
    }
    auto objects = lvl->getObjects();

    // Track which objects to send
    std::vector<std::shared_ptr<Object>> objectsToSend = collectObjectsToSend(objects);
    
    // If we're just sending a heartbeat, no need to continue
    if (objectsToSend.empty()) {
        // std::cout << "[EmbeddedServer] No objects to send, sending minimal heartbeat" << std::endl;
        return;
    }

    // Calculate total message size to determine if we need to split
    size_t estimatedSize = calculateMessageSize(objectsToSend);
    
    // Check if we need to split the message into multiple packets
    if (estimatedSize > MAX_GAMESTATE_PACKET_SIZE) {
        sendSplitGameState(objectsToSend, estimatedSize);
    } else {
        sendSingleGameStatePacket(objectsToSend);
    }
}

std::vector<std::shared_ptr<Object>> EmbeddedServer::collectObjectsToSend(const std::vector<std::shared_ptr<Object>>& allObjects) {
    std::vector<std::shared_ptr<Object>> objectsToSend;
    
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        
        // Check if we have previous state to compare against
        if (deltaTracker_.getAllObjectIds().empty()) {
            // First time or after reset - send everything
            objectsToSend = allObjects;
            
            // Update the tracker with the current objects
            deltaTracker_.updateState(allObjects);
        } else {
            // Get only changed objects for delta update
            objectsToSend = deltaTracker_.getChangedObjects(allObjects);
            
            // If there are no changes, still send a minimal update (heartbeat)
            if (objectsToSend.empty()) {
                sendMinimalHeartbeat();
                return objectsToSend; // Return empty vector
            }
            
            // Update the tracker with all current objects
            deltaTracker_.updateState(allObjects);
        }
    }
    
    return objectsToSend;
}

void EmbeddedServer::sendMinimalHeartbeat() {
    // Send a minimal update with just packet type
    NetworkMessage minimalMsg;
    minimalMsg.type = MessageType::GAME_STATE_DELTA;
    minimalMsg.senderId = 0; // 'server' as 0 or a reserved value
    minimalMsg.data = {0, 0}; // 0 objects
    
    // Send to all clients
    std::lock_guard<std::mutex> lockSockets(clientSocketsMutex_);
    if (clientSockets_.empty()) {
        return;
    }
    
    for (auto& [id, sock] : clientSockets_) {
        if (sock && sock->is_open()) {
            sendToClient(sock, minimalMsg);
        }
    }
    
    // Also notify through callback
    if (messageCallback_) {
        messageCallback_(minimalMsg);
    }
}

size_t EmbeddedServer::calculateMessageSize(const std::vector<std::shared_ptr<Object>>& objectsToSend) {
    size_t estimatedSize = 2; // Object count (2 bytes)
    
    for (const auto& obj : objectsToSend) {
        if (!obj) continue;
        
        // Basic object data
        size_t objSize = 1 + sizeof(uint16_t) + 16; // Type + ID (uint16_t) + position/velocity
        
        // Type-specific data
        switch (obj->type) {
            case ObjectType::PLAYER:
            case ObjectType::MINOTAUR:
                objSize += 4; // Animation state and direction, health
                break;
            case ObjectType::TILE: {
                std::shared_ptr<Tile> tile = std::static_pointer_cast<Tile>(obj);
                objSize += 5; // Tile index (1) + flags (4)
                objSize += 1 + tile->gettileMapName().size();
                objSize += 1; //LayerID
                if(objSize > 56)
                {
                    std::cout << "[EmbeddedServer] Tile size: " << objSize << std::endl;
                    std::cout << "[EmbeddedServer] Tile name: " << tile->gettileMapName() << std::endl;
                    std::cout << "object ID: " << obj->getObjID() << std::endl;
                }
                
                break;
            }
            default:
                break;
        }
        
        estimatedSize += objSize;
    }
    
    return estimatedSize;
}

void EmbeddedServer::sendSplitGameState(const std::vector<std::shared_ptr<Object>>& objectsToSend, size_t estimatedSize) {
    std::cout << "[EmbeddedServer] Game state too large, estimated size: " 
              << estimatedSize << " bytes, splitting into packets" << std::endl;
    std::cout << "[EmbeddedServer] Actual size: " 
              << objectsToSend.size() << " objects to send" << std::endl;
    // We need to split the message
    size_t objectsPerPacket = MAX_GAMESTATE_PACKET_SIZE / (estimatedSize / objectsToSend.size()) - 15;
    std::cout << "[EmbeddedServer] Estimated objects per packet: " 
              << objectsPerPacket << std::endl;
    size_t totalObjects = objectsToSend.size();
    size_t packetCount = (totalObjects + objectsPerPacket - 1) / objectsPerPacket;
    
    std::cout << "[EmbeddedServer] Splitting game state update into " << packetCount 
              << " packets with ~" << objectsPerPacket << " objects per packet" << std::endl;
    
    // Send packets
    for (size_t i = 0; i < packetCount; i++) {
        size_t startIndex = i * objectsPerPacket;
        size_t count = std::min(objectsPerPacket, totalObjects - startIndex);
        
        bool isFirstPacket = (i == 0);
        bool isLastPacket = (i == packetCount - 1);
        
        sendPartialGameState(objectsToSend, startIndex, count, isFirstPacket, isLastPacket);
    }
}

void EmbeddedServer::sendSingleGameStatePacket(const std::vector<std::shared_ptr<Object>>& objectsToSend) {
    // Message fits in a single packet - use delta state message
    NetworkMessage stateMsg;
    stateMsg.type = MessageType::GAME_STATE_DELTA;
    stateMsg.senderId = 0; // 'server' as 0 or a reserved value
    stateMsg.data.clear();
    
    // Serialize object count (2 bytes)
    uint16_t count = static_cast<uint16_t>(objectsToSend.size());
    stateMsg.data.push_back(uint8_t(count >> 8));
    stateMsg.data.push_back(uint8_t(count & 0xFF));
    
    // std::cout << "[EmbeddedServer] Sending game state update with " 
    //           << count << " objects in a single packet" << std::endl;

    // Serialize each object
    for (auto& obj : objectsToSend) {
        // std::cout << "[EmbeddedServer] Serializing object of type " 
        //           << static_cast<int>(obj->type) << " with ID: " 
        //           << obj->getObjID() << std::endl;
        serializeObject(obj, stateMsg.data);
    }
    
    // Broadcast to all clients
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        if (clientSockets_.empty()) {
            static auto last = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last > std::chrono::seconds(5)) {
                last = now;
                std::cout << "[EmbeddedServer] No clients connected, skipping update\n";
            }
            return;
        }
        
        for (auto& [id, sock] : clientSockets_) {
            if (sock && sock->is_open()) {
                sendToClient(sock, stateMsg);
            }
        }
    }
    
    // Fire callback if needed
    if (messageCallback_) {
        messageCallback_(stateMsg);
    }
}

/**
 * Sends a partial game state update to all clients.
 * This is used when the game state is too large to fit in a single packet.
 * Each part will contain metadata to indicate if it's the first or last part.
 */
void EmbeddedServer::sendPartialGameState(
    const std::vector<std::shared_ptr<Object>>& objects, 
    size_t startIndex, size_t count, 
    bool isFirstPacket, bool isLastPacket) {
    
    // Create a new message for this part
    NetworkMessage partMsg;
    partMsg.type = MessageType::GAME_STATE_PART;
    partMsg.senderId = 0; // 'server' as 0 or a reserved value
    partMsg.data.clear();
    
    // Packet metadata (3 bytes):
    // - First byte: isFirstPacket (0x01) | isLastPacket (0x02)
    // - Second and third bytes: Total object count
    uint8_t flags = (isFirstPacket ? 0x01 : 0x00) | (isLastPacket ? 0x02 : 0x00);
    partMsg.data.push_back(flags);
    
    uint16_t totalCount = static_cast<uint16_t>(objects.size());
    partMsg.data.push_back(uint8_t(totalCount >> 8));
    partMsg.data.push_back(uint8_t(totalCount & 0xFF));
    
    // Packet specific data (2 bytes):
    // - Start index (2 bytes)
    partMsg.data.push_back(uint8_t(startIndex >> 8));
    partMsg.data.push_back(uint8_t(startIndex & 0xFF));
    
    // - Object count in this packet (2 bytes)
    partMsg.data.push_back(uint8_t(count >> 8));
    partMsg.data.push_back(uint8_t(count & 0xFF));
    
    std::cout << "Serializing " << count << " objects starting from index " 
              << startIndex << " of total " << objects.size() << " objects." << std::endl;
    // Serialize each object in this range
    for (size_t i = startIndex; i < startIndex + count && i < objects.size(); i++) {
        // if(i > PRINTNUM)
        // {
        //     std::cout << "[EmbeddedServer] Serializing object " << i << " of type " 
        //             << static_cast<int>(objects[i]->type) << "and ID: " << objects[i]->getObjID() << std::endl;
        // }
        serializeObject(objects[i], partMsg.data);
    }
    
    // Broadcast to all clients
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        if (clientSockets_.empty()) {
            return;
        }
        
        for (auto& [id, sock] : clientSockets_) {
            if (sock && sock->is_open()) {
                // std::cout << "[EmbeddedServer] Sending partial game state to client " 
                //           << id << " - Part: " << (isFirstPacket ? "First" : "Middle") 
                //           << ", Count: " << count << std::endl;
                sendToClient(sock, partMsg);
            }
        }
    }
    
    // Fire callback if needed
    if (messageCallback_) {
        messageCallback_(partMsg);
    }
}

void EmbeddedServer::sendPartialGameStateToClient(
    const std::vector<std::shared_ptr<Object>>& objects, 
    size_t startIndex, size_t count, 
    bool isFirstPacket, bool isLastPacket,
    uint16_t playerId) {
    NetworkMessage partMsg;
    partMsg.type = MessageType::GAME_STATE_PART;
    partMsg.senderId = 0;
    partMsg.targetId = playerId;
    partMsg.data.clear();
    
    // Packet metadata (3 bytes):
    // - First byte: isFirstPacket (0x01) | isLastPacket (0x02)
    // - Second and third bytes: Total object count
    uint8_t flags = (isFirstPacket ? 0x01 : 0x00) | (isLastPacket ? 0x02 : 0x00);
    partMsg.data.push_back(flags);
    
    uint16_t totalCount = static_cast<uint16_t>(objects.size());
    partMsg.data.push_back(uint8_t(totalCount >> 8));
    partMsg.data.push_back(uint8_t(totalCount & 0xFF));
    
    // Packet specific data (4 bytes):
    // - Start index (2 bytes)
    partMsg.data.push_back(uint8_t(startIndex >> 8));
    partMsg.data.push_back(uint8_t(startIndex & 0xFF));
    
    // - Object count in this packet (2 bytes)
    partMsg.data.push_back(uint8_t(count >> 8));
    partMsg.data.push_back(uint8_t(count & 0xFF));
    for (size_t i = startIndex; i < startIndex + count && i < objects.size(); ++i) {
        serializeObject(objects[i], partMsg.data);
    }
    std::lock_guard<std::mutex> sockLock(clientSocketsMutex_);
    auto it = clientSockets_.find(playerId);
    if (it != clientSockets_.end() && it->second && it->second->is_open()) {
        std::cout << "[EmbeddedServer] Sending partial game state to client " 
                  << playerId << " - Part: " << (isFirstPacket ? "First" : (isLastPacket ? "Last" : "Middle")) 
                  << ", byte size: " << partMsg.data.size()
                  << ", objects: " << count << std::endl;
        sendToClient(it->second, partMsg);
    } else {
        std::cerr << "[EmbeddedServer] Could not send partial game state to client: " << playerId << std::endl;
    }
}

void EmbeddedServer::sendSplitGameStateToClient(
    const std::vector<std::shared_ptr<Object>>& objectsToSend, 
    size_t estimatedSize, 
    uint16_t playerId) {
    size_t objectsPerPacket = MAX_GAMESTATE_PACKET_SIZE / (estimatedSize / objectsToSend.size()) - 25;
    size_t totalObjects = objectsToSend.size();
    size_t packetCount = (totalObjects + objectsPerPacket - 1) / objectsPerPacket;
    std::cout << "[EmbeddedServer] Splitting game state for client " << playerId
              << " into " << packetCount << " packets with ~" << objectsPerPacket 
              << " objects per packet (total objects: " << totalObjects << ")" << std::endl;
    for (size_t i = 0; i < packetCount; i++) {
        size_t startIndex = i * objectsPerPacket;
        size_t count = std::min(objectsPerPacket, totalObjects - startIndex);
        bool isFirstPacket = (i == 0);
        bool isLastPacket = (i == packetCount - 1);
        sendPartialGameStateToClient(objectsToSend, startIndex, count, isFirstPacket, isLastPacket, playerId);
        if(!isLastPacket)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void EmbeddedServer::sendSingleGameStatePacketToClient(
    const std::vector<std::shared_ptr<Object>>& objectsToSend, 
    uint16_t playerId) {
    std::vector<uint8_t> data;
    uint16_t objectCount = static_cast<uint16_t>(objectsToSend.size());
    data.push_back(static_cast<uint8_t>(objectCount >> 8));
    data.push_back(static_cast<uint8_t>(objectCount & 0xFF));
    for (const auto& obj : objectsToSend) {
        serializeObject(obj, data);
    }
    NetworkMessage msg;
    msg.type = MessageType::GAME_STATE;
    msg.senderId = 0;
    msg.targetId = playerId;
    msg.data = std::move(data);
    std::lock_guard<std::mutex> sockLock(clientSocketsMutex_);
    auto it = clientSockets_.find(playerId);
    if (it != clientSockets_.end() && it->second && it->second->is_open()) {
        sendToClient(it->second, msg);
    }
}

void EmbeddedServer::serializeObject(const std::shared_ptr<Object>& object, std::vector<uint8_t>& data) {

    Object* obj = object.get();
    if (!obj) {
        std::cerr << "[EmbeddedServer] Error: Attempted to serialize a null object" << std::endl;
        return; // Return empty data if object is null
    }
    size_t initialSize = data.size();
    // a) Type
    data.push_back(static_cast<uint8_t>(obj->type));
    
    // b) ID - now a uint16_t
    uint16_t id = obj->getObjID();
    data.push_back(static_cast<uint8_t>(id & 0xFF));
    data.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));

    // c) Position & Velocity
    auto writeFloat = [&](float v) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&v);
        for (size_t i = 0; i < sizeof(float); ++i)
            data.push_back(bytes[i]);
    };
    
    const Vec2& p = obj->getposition();
    const Vec2& v = obj->getvelocity();
    writeFloat(p.x);
    writeFloat(p.y);
    writeFloat(v.x);
    writeFloat(v.y);
    
    // Extra fields for specific object types
    switch(obj->type) {
        case ObjectType::TILE: {
            auto* plat = static_cast<Tile*>(obj);
            data.push_back(plat->gettileIndex());
            for (int i = 0; i < 4; ++i) {
                data.push_back(static_cast<uint8_t>((plat->getFlags() >> (i * 8)) & 0xFF));
            }
            // Tilemap name length
            const std::string& tileMapName = plat->gettileMapName();

            data.push_back(static_cast<uint8_t>(tileMapName.size()));
            // Tilemap name content
            data.insert(data.end(), tileMapName.begin(), tileMapName.end());
            size_t sizeAfterTile = data.size();
            // Layer
            data.push_back(static_cast<uint8_t>(plat->getLayer()));
            break;
        }
        case ObjectType::MINOTAUR: {
            auto* mino = static_cast<Minotaur*>(obj);
            data.push_back(static_cast<uint8_t>(mino->getAnimationState()));
            data.push_back(static_cast<uint8_t>(mino->getDir()));
            int16_t health = mino->getHealth();
            data.push_back(static_cast<uint8_t>(health >> 8));
            data.push_back(static_cast<uint8_t>(health & 0xFF));
            break;
        }
        case ObjectType::PLAYER: {
            auto* player = static_cast<Player*>(obj);
            data.push_back(static_cast<uint8_t>(player->getAnimationState()));
            data.push_back(static_cast<uint8_t>(player->getDir()));
            int16_t health = player->getHealth();
            data.push_back(static_cast<uint8_t>(health >> 8));
            data.push_back(static_cast<uint8_t>(health & 0xFF));
            break;
        }
        default:
            break;
    }
}


/**
 * Sends the full game state to a specific client.
 * If the game state is too large, it will be split into multiple packets.
 * Each packet will contain metadata to indicate if it's the first or last part.
 */
void EmbeddedServer::sendFullGameStateToClient(const uint16_t playerId) {

    std::lock_guard<std::mutex> lock(gameStateMutex_);
    Level* lvl = levelManager_->getCurrentLevel();
    if (!lvl) {
        std::cerr << "[EmbeddedServer] No active level for full game state sync" << std::endl;
        return;
    }
    auto objects = lvl->getObjects();
    
    // Calculate estimated size of the serialized data
    size_t estimatedSize = calculateMessageSize(objects);
    
    std::cout << "[EmbeddedServer] Sending full game state to client " << playerId 
              << " with " << objects.size() << " objects" << std::endl;
    
    // Check if we need to split the message into multiple packets
    if (estimatedSize > MAX_GAMESTATE_PACKET_SIZE) {
        sendSplitGameStateToClient(objects, estimatedSize, playerId);
    } else {
        sendSingleGameStatePacketToClient(objects, playerId);
    }
}