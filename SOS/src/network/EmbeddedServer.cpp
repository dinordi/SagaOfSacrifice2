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
            
            // Find the socket using the temp clientId from the network endpoint
            std::string tempClientId;
            std::shared_ptr<boost::asio::ip::tcp::socket> clientSocket;
            std::string assignedPlayerId;
            
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex_);
                // Find the socket by checking all entries
                for (auto& entry : clientSockets_) {
                    try {
                        if (entry.second && entry.second->is_open()) {
                            std::string socketId = entry.second->remote_endpoint().address().to_string() + 
                                                 ":" + std::to_string(entry.second->remote_endpoint().port());
                            
                            if (entry.first.find(socketId) != std::string::npos) {
                                tempClientId = entry.first;
                                clientSocket = entry.second;
                                
                                // Find the assigned player ID for this temp client ID
                                auto it = tempToPlayerIdMap_.find(tempClientId);
                                if (it != tempToPlayerIdMap_.end()) {
                                    assignedPlayerId = it->second;
                                }
                                
                                break;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[EmbeddedServer] Error checking socket: " << e.what() << std::endl;
                    }
                }
                
                // If we found the socket and have an assigned player ID, update the mapping
                if (!tempClientId.empty() && clientSocket && !assignedPlayerId.empty()) {
                    std::cout << "[EmbeddedServer] Updating socket mapping from " << tempClientId 
                              << " to " << assignedPlayerId << std::endl;
                    
                    // Remove the old temp ID mapping
                    clientSockets_.erase(tempClientId);
                    
                    // Add the new mapping with the assigned player ID
                    clientSockets_[assignedPlayerId] = clientSocket;
                    
                    // Remove the temporary mapping
                    tempToPlayerIdMap_.erase(tempClientId);
                }
            }
            
            // Add the player to the game using the server-assigned ID
            if (!assignedPlayerId.empty()) {
                addPlayer(assignedPlayerId);
                // Send full game state to this new player
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


void EmbeddedServer::removePlayer(const std::string& playerId) {
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
    // Generate a temporary client ID based on the remote endpoint (just for socket tracking)
    std::string tempClientId = socket->remote_endpoint().address().to_string() + 
                           ":" + std::to_string(socket->remote_endpoint().port());
    
    // Generate a unique player ID for this client
    std::string generatedPlayerId = "player_" + std::to_string(nextPlayerId_++);
    
    std::cout << "[EmbeddedServer] New connection from: " << tempClientId << std::endl;
    std::cout << "[EmbeddedServer] Generated player ID: " << generatedPlayerId << std::endl;
    
    // Store the socket using the temporary ID, but also remember the generated player ID
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        clientSockets_[tempClientId] = socket;
        // Map the temporary ID to the generated player ID
        tempToPlayerIdMap_[tempClientId] = generatedPlayerId;
    }
    
    // Start the proper async read sequence to get a complete message
    // First read just the message header
    auto headerBuffer = std::make_shared<std::array<char, sizeof(MessageHeader)>>();
    boost::asio::async_read(*socket, 
        boost::asio::buffer(*headerBuffer, sizeof(MessageHeader)),
        [this, socket, headerBuffer, tempClientId](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (!error && bytes_transferred == sizeof(MessageHeader)) {
                // Extract the message size from the header
                MessageHeader* header = reinterpret_cast<MessageHeader*>(headerBuffer->data());
                uint32_t headerSize = header->size;
                std::cout << "[EmbeddedServer] Received message header with size: " << header->size << " bytes" << std::endl;
                
                // Validate header size
                if (header->size > MAX_MESSAGE_SIZE) {
                    std::cerr << "[EmbeddedServer] Message size exceeds maximum limit" << std::endl;
                    boost::system::error_code ec;
                    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    socket->close(ec);
                    return;
                }
                std::cout << "[EmbeddedServer] Valid header size, proceeding to read message body..." << std::endl;
                // Now read the message body
                auto bodyBuffer = std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
                boost::asio::async_read(*socket,
                    boost::asio::buffer(bodyBuffer->data(), header->size),
                    [this, socket, bodyBuffer, headerSize, tempClientId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                        std::cout << "[EmbeddedServer] Received message body with size: " << bytes_transferred << " bytes" << std::endl;
                        std::cout << "[EmbeddedServer] Header size: " << headerSize << std::endl;
                        if (!error && bytes_transferred == headerSize) {
                            // Process the complete message
                            std::vector<uint8_t> messageData(
                                bodyBuffer->data(),
                                bodyBuffer->data() + headerSize
                            );
                            
                            // Deserialize the message
                            NetworkMessage message = deserializeMessage(messageData, tempClientId);
                            
                            // Debug log - always log CONNECT messages
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
                            
                            // Process the message
                            processMessage(message);
                            
                            std::cout << "[EmbeddedServer] First message processed, now waiting for more messages..." << std::endl;
                            // Now start the normal read loop
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
        // Get the client ID
        std::string clientId;
        try {
            clientId = socket->remote_endpoint().address().to_string() + 
                      ":" + std::to_string(socket->remote_endpoint().port());
        } catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Error getting client ID: " << e.what() << std::endl;
            return;
        }
        
        std::shared_ptr<std::array<char, MAX_MESSAGE_SIZE>> buffer = 
            std::make_shared<std::array<char, MAX_MESSAGE_SIZE>>();
        
        // First read the message header to determine the size
        boost::asio::async_read(*socket, 
            boost::asio::buffer(buffer->data(), sizeof(MessageHeader)),
            [this, socket, buffer, clientId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error && bytes_transferred == sizeof(MessageHeader)) {
                    // Extract the message size from the header
                    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer->data());
                    // Read the message body
                    boost::asio::async_read(*socket,
                        boost::asio::buffer(buffer->data() + sizeof(MessageHeader), header->size),
                        [this, socket, buffer, header, clientId](const boost::system::error_code& error, std::size_t bytes_transferred) {
                            if (!error && bytes_transferred == header->size) {
                                // Process the complete message
                                std::vector<uint8_t> messageData(
                                    buffer->data() + sizeof(MessageHeader),
                                    buffer->data() + sizeof(MessageHeader) + header->size
                                );
                                
                                // Deserialize the message
                                NetworkMessage message = deserializeMessage(messageData, clientId);
                                
                                // Process the message
                                processMessage(message);
                                
                                // Continue reading
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

bool EmbeddedServer::sendToClient(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                 const NetworkMessage& message) 
{
    try {
        // Create a shared_ptr to manage the message buffer lifetime
        auto buffer_ptr = std::make_shared<std::vector<uint8_t>>();
        auto& buffer = *buffer_ptr;
        
        // 1. Message type - 1 byte
        buffer.push_back(static_cast<uint8_t>(message.type));
        
        // 2. Sender ID length - 1 byte
        buffer.push_back(static_cast<uint8_t>(message.senderId.size()));
        
        // 3. Sender ID content
        buffer.insert(buffer.end(), message.senderId.begin(), message.senderId.end());
        
        // 4. Data length - 4 bytes (important!)
        uint32_t dataSize = static_cast<uint32_t>(message.data.size());
        buffer.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(dataSize & 0xFF));
        
        // 5. Data content
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

void EmbeddedServer::addPlayer(const std::string& playerId) {
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
    
    // Send a player joined message to all clients
    NetworkMessage joinMsg;
    joinMsg.type = MessageType::PLAYER_JOINED;
    joinMsg.senderId = playerId;
    
    if (messageCallback_) {
        messageCallback_(joinMsg);
    }
}

void EmbeddedServer::sendPlayerToClient(const std::string& playerId, Player* player) {
    if (!player) {
        return;
    }
    
    // Create a message to send the player to the client
    NetworkMessage playerMsg;
    playerMsg.type = MessageType::PLAYER_ASSIGN;
    playerMsg.senderId = "server";
    playerMsg.targetId = playerId;
    
    // Serialize player data
    // For now we'll use position and ID. In a more advanced implementation,
    // you would serialize the complete player state.
    Vec2 position = player->getcollider().position;
    
    // Pack position data
    std::vector<uint8_t> data;
    data.resize(sizeof(float) * 2 + playerId.length() + 1);
    
    // Position X and Y
    float posX = position.x;
    float posY = position.y;
    std::memcpy(data.data(), &posX, sizeof(float));
    std::memcpy(data.data() + sizeof(float), &posY, sizeof(float));
    
    // Player ID
    std::memcpy(data.data() + sizeof(float) * 2, playerId.c_str(), playerId.length() + 1);
    
    playerMsg.data = std::move(data);
    
    // Send the message
    
    std::lock_guard<std::mutex> lock(clientSocketsMutex_);
    auto it = clientSockets_.find(playerId);
    if (it != clientSockets_.end() && it->second && it->second->is_open()) {
        sendToClient(it->second, playerMsg);
    } else {
        std::cerr << "[EmbeddedServer] Failed to find socket for player " << playerId << std::endl;
    }

    // Notify through callback if set
    if (messageCallback_) {
        messageCallback_(playerMsg);
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
    minimalMsg.senderId = "server";
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
        size_t objSize = 1 + 1 + obj->getObjID().size() + 16; // Type + ID length + ID + position/velocity
        
        // Type-specific data
        switch (obj->type) {
            case ObjectType::PLAYER:
            case ObjectType::MINOTAUR:
                objSize += 2; // Animation state and direction
                break;
            case ObjectType::TILE: {
                std::shared_ptr<Tile> tile = std::static_pointer_cast<Tile>(obj);
                objSize += 5; // Tile index (1) + flags (4) + tilemapname length + tilemapname
                objSize += 1 + tile->gettileMapName().size();
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
    stateMsg.senderId = "server";
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
    partMsg.senderId = "server";
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
    const std::string& playerId) {
    
    // Create a new message for this part
    NetworkMessage partMsg;
    partMsg.type = MessageType::GAME_STATE_PART;
    partMsg.senderId = "server";
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
    
    // Serialize each object in this range
    for (size_t j = startIndex; j < startIndex + count && j < objects.size(); j++) {
        serializeObject(objects[j], partMsg.data);
    }
    
    // Send to the specific client
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
    const std::string& playerId) {
    
    // We need to split the message
    size_t objectsPerPacket = MAX_GAMESTATE_PACKET_SIZE / (estimatedSize / objectsToSend.size()) - 15;
    size_t totalObjects = objectsToSend.size();
    size_t packetCount = (totalObjects + objectsPerPacket - 1) / objectsPerPacket;
    
    std::cout << "[EmbeddedServer] Splitting game state for client " << playerId
              << " into " << packetCount << " packets with ~" << objectsPerPacket 
              << " objects per packet (total objects: " << totalObjects << ")" << std::endl;
    
    // Send packets specifically to this client
    for (size_t i = 0; i < packetCount; i++) {
        size_t startIndex = i * objectsPerPacket;
        size_t count = std::min(objectsPerPacket, totalObjects - startIndex);
        
        bool isFirstPacket = (i == 0);
        bool isLastPacket = (i == packetCount - 1);
        
        sendPartialGameStateToClient(objectsToSend, startIndex, count, isFirstPacket, isLastPacket, playerId);
    }
}

void EmbeddedServer::sendSingleGameStatePacketToClient(
    const std::vector<std::shared_ptr<Object>>& objectsToSend, 
    const std::string& playerId) {
    
    // Message fits in a single packet
    std::vector<uint8_t> data;
    // First 2 bytes: object count
    uint16_t objectCount = static_cast<uint16_t>(objectsToSend.size());
    data.push_back(static_cast<uint8_t>(objectCount >> 8));
    data.push_back(static_cast<uint8_t>(objectCount & 0xFF));
    for (const auto& obj : objectsToSend) {
        serializeObject(obj, data);
    }
    
    NetworkMessage msg;
    // Use GAME_STATE instead of GAME_STATE_DELTA for full game states to a single client
    // This ensures the client knows to replace its entire state rather than apply a delta
    msg.type = MessageType::GAME_STATE;
    msg.senderId = "server";
    msg.targetId = playerId;
    msg.data = std::move(data);
    
    // Send to the specific client
    std::lock_guard<std::mutex> sockLock(clientSocketsMutex_);
    auto it = clientSockets_.find(playerId);
    if (it != clientSockets_.end() && it->second && it->second->is_open()) {
        std::cout << "[EmbeddedServer] Sending full game state to client " << playerId 
                  << " in a single packet (" << objectsToSend.size() << " objects)" << std::endl;
        sendToClient(it->second, msg);
    } else {
        std::cerr << "[EmbeddedServer] Could not send game state to client: " << playerId << std::endl;
    }
}

void EmbeddedServer::serializeObject(const std::shared_ptr<Object>& object, std::vector<uint8_t>& data) {

    Object* obj = object.get();
    if (!obj) {
        std::cerr << "[EmbeddedServer] Error: Attempted to serialize a null object" << std::endl;
        return; // Return empty data if object is null
    }
    static uint16_t objectCount = 0;
    size_t initialSize = data.size();
    // a) Type
    data.push_back(static_cast<uint8_t>(obj->type));
    
    // b) ID
    const std::string& id = obj->getObjID();
    data.push_back(static_cast<uint8_t>(id.size()));
    data.insert(data.end(), id.begin(), id.end());

    objectCount++;
    if(objectCount > PRINTNUM)
    {
        // std::cout << "[EmbeddedServer] Serializing object ID: " << id  << ", with type: " << static_cast<int>(object->type) << std::endl;
    }
    bool print = false;
    //if ID contains "Grass" do not print
    if (id.find("_map_75_64") != std::string::npos)
    {
        std::cout << "[EmbeddedServer] Serializing object ID: " << id << std::endl;
        std::cout << "With length: " << id.size() << std::endl;
        print = true;
    }

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
            if(print)
                std::cout << "[EmbeddedServer] Serializing tilemap name: " << tileMapName << std::endl;
            data.push_back(static_cast<uint8_t>(tileMapName.size()));
            // Tilemap name content
            data.insert(data.end(), tileMapName.begin(), tileMapName.end());
            size_t sizeAfterTile = data.size();
            // std::cout << "[EmbeddedServer] Tile serialized, size: " << sizeAfterTile - initialSize << " bytes" << std::endl;
            break;
        }
        case ObjectType::MINOTAUR: {
            auto* mino = static_cast<Minotaur*>(obj);
            data.push_back(static_cast<uint8_t>(mino->getAnimationState()));
            data.push_back(static_cast<uint8_t>(mino->getDir()));
            break;
        }
        case ObjectType::PLAYER: {
            auto* player = static_cast<Player*>(obj);
            data.push_back(static_cast<uint8_t>(player->getAnimationState()));
            data.push_back(static_cast<uint8_t>(player->getDir()));
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
void EmbeddedServer::sendFullGameStateToClient(const std::string& playerId) {

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

    deltaTracker_.updateState(objects);
}