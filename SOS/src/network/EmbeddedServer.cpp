#include <iostream>
#include <chrono>
#include <array>
#include <future>

#include <boost/bind.hpp>

#include "network/EmbeddedServer.h"
#include "network/NetworkConfig.h"
#include "collision/CollisionManager.h"
#include "objects/player.h"
#include "objects/platform.h"

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
        
        
        // Start accepting connections
        startAccept();
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
        gameObjects_.clear();
        players_.clear();
    }
    
    std::cout << "[EmbeddedServer] Stopped" << std::endl;
}

void EmbeddedServer::startAccept() {
    try {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        acceptor_->async_accept(*socket,
            [this, socket](const boost::system::error_code& error) {
                handleAccept(error, socket);
            });
    } catch (const std::exception& e) {
        std::cerr << "[EmbeddedServer] Exception in startAccept: " << e.what() << std::endl;
    }
}

void EmbeddedServer::handleAccept(const boost::system::error_code& error, 
                                  std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    std::cout << "[EmbeddedServer] handleAccept called, error:" << error.message() << std::endl;
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
    }
}

void EmbeddedServer::run() {
    std::cout << "[EmbeddedServer] Game loop started" << std::endl;
    
    const auto tickDuration = std::chrono::milliseconds(1000 / NetworkConfig::Server::TickRate);
    unsigned int loopCounter = 0;

    while (gameLoopRunning_) {
        
        auto now = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime_).count();
        lastUpdateTime_ = now;
        
        try {
            updateGameState(deltaTime);
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
            std::this_thread::sleep_for(tickDuration);
        }
        catch (const std::exception& e) {
            std::cerr << "[EmbeddedServer] Exception during game loop sleep: " << e.what() << std::endl;
            // Brief pause to avoid spinning if something is wrong with sleep
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
    
    if (players_.find(playerId) != players_.end()) {
        std::cerr << "[EmbeddedServer] Player " << playerId << " already exists" << std::endl;
        return;
    }
    
    std::cout << "[EmbeddedServer] Adding player " << playerId << std::endl;
    
    // Create a new player object
    auto player = std::make_shared<Player>(Vec2(500, 100), 
                                           new SpriteData(std::string("playermap"), 128, 128, 5), 
                                           playerId);
    TempInput* input = new TempInput();
    input->setInputs(false, false, false, false, false); // Initialize inputs to false
    player->setInput(input);
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
            std::cout << "[EmbeddedServer] Processing player input message from " << message.senderId << std::endl;
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
    players_.clear();
    
    // Create platform at the bottom of the screen
    auto platform = std::make_shared<Platform>(400, 500, 
                                             new SpriteData(std::string("tiles"), 128, 128, 1),
                                             "platform_ground");
    gameObjects_.push_back(platform);
    
    // Add more platforms as needed
    auto platform2 = std::make_shared<Platform>(200, 500, 
                                              new SpriteData(std::string("tiles"), 128, 128, 1),
                                              "platform_1");
    gameObjects_.push_back(platform2);
    
    auto platform3 = std::make_shared<Platform>(600, 500, 
                                              new SpriteData(std::string("tiles"), 128, 128, 1),
                                              "platform_2");
    gameObjects_.push_back(platform3);
}

void EmbeddedServer::updateGameState(uint64_t deltaTime) {
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        
        // Update all game objects
        for (auto& object : gameObjects_) {
            object->update(deltaTime);
        }
        
        // Detect and resolve collisions
        detectAndResolveCollisions();
    }
    // Send game state to clients periodically
    static uint64_t updateTimer = 0;
    updateTimer += deltaTime;
    
    if (updateTimer >= NetworkConfig::Server::StateUpdateInterval) {
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

    // Serialize game state into binary format
    {
        std::lock_guard<std::mutex> lock(gameStateMutex_);
        
        // First, add the count of objects we're sending
        uint16_t objectCount = gameObjects_.size();
        stateMsg.data.push_back(static_cast<uint8_t>((objectCount >> 8) & 0xFF)); // High byte
        stateMsg.data.push_back(static_cast<uint8_t>(objectCount & 0xFF));        // Low byte
        
        // Add all game objects to the state update
        for (const auto& object : gameObjects_) {
            // Skip null objects
            if (!object) {
                std::cerr << "[EmbeddedServer] Warning: Null object in gameObjects_" << std::endl;
                continue;
            }
            
            // Add object type identifier (1 byte)
            uint8_t objectType = 0;
            
            
            if (dynamic_cast<Player*>(object.get())) {
                objectType = static_cast<uint8_t>(ObjectType::PLAYER); // Player
            } else if (dynamic_cast<Platform*>(object.get())) {
                objectType = static_cast<uint8_t>(ObjectType::PLATFORM); // Platform
            } else {
                objectType = 99; // Unknown/Other
            }
            
            stateMsg.data.push_back(objectType);
            
            // Add object ID
            const std::string& objId = object->getObjID();
            uint8_t idLength = static_cast<uint8_t>(objId.size());
            stateMsg.data.push_back(idLength);
            stateMsg.data.insert(stateMsg.data.end(), objId.begin(), objId.end());
            
            // Add position and velocity for all objects
            const Vec2& pos = object->getposition();
            const Vec2& vel = object->getvelocity();
            
            // Position X, Y (each 4 bytes)
            float posX = pos.x;
            float posY = pos.y;
            uint8_t* posXBytes = reinterpret_cast<uint8_t*>(&posX);
            uint8_t* posYBytes = reinterpret_cast<uint8_t*>(&posY);
            for (size_t i = 0; i < sizeof(float); i++) {
                stateMsg.data.push_back(posXBytes[i]);
            }
            for (size_t i = 0; i < sizeof(float); i++) {
                stateMsg.data.push_back(posYBytes[i]);
            }
            
            // Velocity X, Y (each 4 bytes)
            float velX = vel.x;
            float velY = vel.y;
            uint8_t* velXBytes = reinterpret_cast<uint8_t*>(&velX);
            uint8_t* velYBytes = reinterpret_cast<uint8_t*>(&velY);
            for (size_t i = 0; i < sizeof(float); i++) {
                stateMsg.data.push_back(velXBytes[i]);
            }
            for (size_t i = 0; i < sizeof(float); i++) {
                stateMsg.data.push_back(velYBytes[i]);
            }
            
            // For platforms, add width and height
            if (objectType == static_cast<uint8_t>(ObjectType::PLATFORM)) {
                Platform* platform = dynamic_cast<Platform*>(object.get());
                if (platform) {
                    // Add platform width and height
                    float width = platform->spriteData->width;
                    float height = platform->spriteData->height;
                    
                    uint8_t* widthBytes = reinterpret_cast<uint8_t*>(&width);
                    uint8_t* heightBytes = reinterpret_cast<uint8_t*>(&height);
                    
                    for (size_t i = 0; i < sizeof(float); i++) {
                        stateMsg.data.push_back(widthBytes[i]);
                    }
                    for (size_t i = 0; i < sizeof(float); i++) {
                        stateMsg.data.push_back(heightBytes[i]);
                    }
                }
            }
        }
    }

    // Send to each connected client
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        
        // Check if we have any clients to send to
        if (clientSockets_.empty()) {
            // Print every 5 seconds to avoid spam
            static auto lastPrintTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - lastPrintTime > std::chrono::seconds(5)) {
                lastPrintTime = now;
                std::cout << "[EmbeddedServer] No clients connected, skipping game state update" << std::endl;
            }
            return;
        }
        
        for (const auto& clientPair : clientSockets_) {
            try {
                if (!clientPair.second || !clientPair.second->is_open()) {
                    std::cerr << "[EmbeddedServer] Client socket is null or closed: " << clientPair.first << std::endl;
                    continue;
                }
                
                bool success = sendToClient(clientPair.second, stateMsg);
            } catch (const std::exception& e) {
                std::cerr << "[EmbeddedServer] Error sending game state to client " 
                          << clientPair.first << ": " << e.what() << std::endl;
            }
        }
    }

    // Call the message callback if it exists
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
    
    // Check if we have enough data (at least 1 byte for input state)
    if (message.data.size() < 1) {
        std::cerr << "[EmbeddedServer] Invalid input data size: " << message.data.size() << " bytes" << std::endl;
        return;
    }
    
    // Extract input values from the binary format sent by MultiplayerManager
    uint8_t inputState = message.data[0];
    
    // Extract individual input flags (matches MultiplayerManager::serializePlayerInput)
    bool left = (inputState & 1) != 0;
    bool right = (inputState & 2) != 0;
    bool up = (inputState & 4) != 0;
    bool down = (inputState & 8) != 0;
    bool attack = (inputState & 16) != 0;

    // Create input handler and apply to player
    auto input = std::make_unique<TempInput>();
    input->setInputs(up, down, left, right, attack);
    player->setInput(input.get());
    
    // Let the player process the input
    player->handleInput(input.get(), 16); // Assume ~60fps
    
    // The player's update function will be called in the updateGameState method
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