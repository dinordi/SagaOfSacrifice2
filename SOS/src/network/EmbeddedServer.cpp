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

EmbeddedServer::EmbeddedServer(int port)
    : port_(port), 
      running_(false),
      gameLoopRunning_(false),
      levelManager_(std::make_shared<LevelManager>()),
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
    
    std::cout << "[EmbeddedServer] New connection from: " << tempClientId << std::endl;
    std::cout << "[EmbeddedServer] Waiting for client's CONNECT message to get real player ID..." << std::endl;
    
    // Store the socket using the temporary ID
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        clientSockets_[tempClientId] = socket;
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

        // Sleep until next tick
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(fixedDeltaMilliseconds));
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
    }
    
    
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

void EmbeddedServer::processMessage(const NetworkMessage& message) {
    // Process based on message type
    switch (message.type) {
        case MessageType::CONNECT: {
            std::cout << "[EmbeddedServer] Processing connect message from " << message.senderId << std::endl;
            
            // Find the socket using the temp clientId from the network endpoint
            std::string tempClientId;
            std::shared_ptr<boost::asio::ip::tcp::socket> clientSocket;
            
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
                                break;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[EmbeddedServer] Error checking socket: " << e.what() << std::endl;
                    }
                }
                
                // If we found the socket with a temporary ID, update the mapping
                if (!tempClientId.empty() && clientSocket && tempClientId != message.senderId) {
                    std::cout << "[EmbeddedServer] Updating socket mapping from " << tempClientId 
                              << " to " << message.senderId << std::endl;
                    
                    // Remove the old temp ID mapping
                    clientSockets_.erase(tempClientId);
                    
                    // Add the new mapping with the proper player ID
                    clientSockets_[message.senderId] = clientSocket;
                }
            }
            
            // Add the player to the game
            addPlayer(message.senderId);
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
        
        levelManager_->update(deltaTime);
        // Update all players
        

    }
    // Send game state to clients periodically
    static float updateTimer = 0;
    updateTimer += deltaTime*1000;
    
    if (updateTimer*1000 >= NetworkConfig::Server::StateUpdateInterval) {
        updateTimer = 0;
        sendGameStateToClients();
    }
}

void EmbeddedServer::sendGameStateToClients() {
    // 1) Build the header
    NetworkMessage stateMsg;
    stateMsg.type     = MessageType::GAME_STATE;
    stateMsg.senderId = "server";
    stateMsg.data.clear();

    // 2) Get objects from the active level
    Level* lvl = levelManager_->getCurrentLevel();

    if (!lvl) {
        return; // nothing to send if no level loaded
    }
    auto objects = lvl->getObjects();

    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);

        // 3) Serialize object count (2 bytes)
        uint16_t count = static_cast<uint16_t>(objects.size());
        stateMsg.data.push_back(uint8_t(count >> 8));
        stateMsg.data.push_back(uint8_t(count & 0xFF));

        // 4) Serialize each object
        for (auto& obj : objects) {
            if (!obj) {
                std::cerr << "[EmbeddedServer] Warning: null object in level\n";
                continue;
            }

            // a) Type
            stateMsg.data.push_back(static_cast<uint8_t>(obj->type));

            // b) ID
            const std::string& id = obj->getObjID();
            stateMsg.data.push_back(uint8_t(id.size()));
            stateMsg.data.insert(stateMsg.data.end(), id.begin(), id.end());

            // c) Position & Velocity
            auto writeFloat = [&](float v) {
                uint8_t* bytes = reinterpret_cast<uint8_t*>(&v);
                for (size_t i = 0; i < sizeof(float); ++i)
                    stateMsg.data.push_back(bytes[i]);
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
                    auto* plat = static_cast<Tile*>(obj.get());
                    writeFloat(plat->getCurrentSpriteData()->width);
                    writeFloat(plat->getCurrentSpriteData()->height);
                    break;
                }
                case ObjectType::MINOTAUR: 
                {
                    auto* mino = static_cast<Minotaur*>(obj.get());
                    stateMsg.data.push_back(static_cast<uint8_t>(mino->getAnimationState()));
                    stateMsg.data.push_back(static_cast<uint8_t>(mino->getDir()));
                    break;
                }
                
                default:
                    break;
            }


        }
    }

    // 5) Broadcast to all clients
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

    // 6) Finally, still fire your callback if needed
    if (messageCallback_) {
        messageCallback_(stateMsg);
    }
}


void EmbeddedServer::processPlayerInput(
    const std::string& playerId,
    const NetworkMessage& message
) {
    std::lock_guard<std::mutex> lock(gameStateMutex_);

    // 1) Lookup the player
    auto player = PlayerManager::getInstance().getPlayer(playerId);
    if (!player) {
        std::cerr << "[EmbeddedServer] Player not found for input: "
                  << playerId << std::endl;
        return;
    }

    // 2) Validate payload
    if (message.data.size() < 1) {
        std::cerr << "[EmbeddedServer] Invalid input data size: "
                  << message.data.size() << " bytes" << std::endl;
        return;
    }

    // 3) Unpack the bitflags
    uint8_t bits  = message.data[0];
    bool    left  = (bits & 1) != 0;
    bool    right = (bits & 2) != 0;
    bool    up    = (bits & 4) != 0;
    bool    down  = (bits & 8) != 0;
    bool    atk   = (bits & 16) != 0;

    // 4) Feed into a TempInput and give it to the Player
    auto input = std::make_unique<TempInput>();
    input->setInputs(up, down, left, right, atk);
    player->setInput(input.get());

    // 5) Immediately invoke the player’s handleInput (or let your game loop do it)
    player->handleInput(input.get(), /*dtFrames=*/16);
}

NetworkMessage EmbeddedServer::deserializeMessage(const std::vector<uint8_t>& data, const std::string& clientId) {
    NetworkMessage message;
    size_t offset = 0;
    
    if (data.empty()) {
        message.senderId = clientId;  // Default to the client ID if we can't read it from the data
        return message;
    }
    
    // 1. Message type (1 byte)
    message.type = static_cast<MessageType>(data[offset++]);
    
    if (offset >= data.size()) {
        message.senderId = clientId;
        return message;
    }
    
    // 2. Sender ID length (1 byte)
    uint8_t senderIdLength = data[offset++];
    
    // 3. Sender ID content
    if (offset + senderIdLength <= data.size()) {
        message.senderId.assign(data.begin() + offset, data.begin() + offset + senderIdLength);
        offset += senderIdLength;
    } else {
        message.senderId = clientId;  // Use client ID if sender ID is invalid
    }
    
    // If we've reached the end of the data, return what we have
    if (offset + 4 > data.size()) {
        return message;
    }
    
    // 4. Data length (4 bytes)
    uint32_t dataSize = 
        (static_cast<uint32_t>(data[offset]) << 24) |
        (static_cast<uint32_t>(data[offset + 1]) << 16) |
        (static_cast<uint32_t>(data[offset + 2]) << 8) |
        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // 5. Data content
    if (offset + dataSize <= data.size()) {
        message.data.assign(data.begin() + offset, data.begin() + offset + dataSize);
    }
    
    return message;
}