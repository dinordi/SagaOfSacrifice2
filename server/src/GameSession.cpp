#include "../include/GameSession.h"
#include "../include/GameServer.h"
#include <iostream>
#include <cstring>

GameSession::GameSession(boost::asio::ip::tcp::socket socket, GameServer& server)
    : socket_(std::move(socket)), 
      server_(server),
      connected_(false) {
    readBuffer_.resize(max_buffer_size);
}

GameSession::~GameSession() {
    // Clean up any resources if needed
    if (connected_) {
        try {
            // Notify the server that this player has disconnected
            NetworkMessage disconnectMsg;
            disconnectMsg.type = MessageType::DISCONNECT;
            disconnectMsg.senderId = playerId_;
            
            // Broadcast to other players
            server_.broadcastMessageExcept(disconnectMsg, playerId_);
            
            // Close the socket
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        } catch (const std::exception& e) {
            std::cerr << "Error during session cleanup: " << e.what() << std::endl;
        }
    }
}

void GameSession::start() {
    // Start reading from the socket
    connected_ = true;
    startRead();
}

void GameSession::sendMessage(const NetworkMessage& message) {
    if (!connected_ || !socket_.is_open()) {
        return;
    }
    
    try {
        // Serialize the message
        std::vector<uint8_t> serializedMsg = serializeMessage(message);
        
        // Add message header (size)
        MessageHeader header;
        header.size = static_cast<uint32_t>(serializedMsg.size());
        
        // Create buffer with header + message
        std::vector<uint8_t> buffer;
        buffer.resize(sizeof(header) + serializedMsg.size());
        
        // Copy header
        std::memcpy(buffer.data(), &header, sizeof(header));
        
        // Copy message body
        std::memcpy(buffer.data() + sizeof(header), serializedMsg.data(), serializedMsg.size());
        
        // Send asynchronously
        asyncWrite(buffer);
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
        connected_ = false;
    }
}

void GameSession::startRead() {
    if (!socket_.is_open()) {
        connected_ = false;
        return;
    }
    
    // First read the message header to know the size
    boost::asio::async_read(socket_, 
        boost::asio::buffer(&readBuffer_[0], sizeof(MessageHeader)),
        [this, self = shared_from_this()](const boost::system::error_code& error, size_t bytes_transferred) {
            if (!error && bytes_transferred == sizeof(MessageHeader)) {
                // Read the message header
                MessageHeader* header = reinterpret_cast<MessageHeader*>(readBuffer_.data());
                
                // Now read the message body
                if (header->size > 0 && header->size < max_buffer_size) {
                    boost::asio::async_read(socket_,
                        boost::asio::buffer(&readBuffer_[sizeof(MessageHeader)], header->size),
                        [this, self = shared_from_this(), header](const boost::system::error_code& error, size_t bytes_transferred) {
                            if (!error && bytes_transferred == header->size) {
                                // Process the complete message
                                try {
                                    std::vector<uint8_t> messageData(
                                        readBuffer_.begin() + sizeof(MessageHeader),
                                        readBuffer_.begin() + sizeof(MessageHeader) + header->size
                                    );
                                    
                                    // Deserialize and process
                                    NetworkMessage message = deserializeMessage(messageData);
                                    handleMessage(message);
                                } catch (const std::exception& e) {
                                    std::cerr << "Error processing message: " << e.what() << std::endl;
                                }
                            }
                            
                            // Continue reading
                            startRead();
                        });
                } else {
                    // Invalid message size, restart reading
                    startRead();
                }
            } else if (error != boost::asio::error::operation_aborted) {
                // Handle disconnection or error
                if (connected_) {
                    std::cerr << "Read error: " << error.message() << std::endl;
                    connected_ = false;
                    
                    // Remove this session from the server
                    if (!playerId_) {
                        server_.removeSession(playerId_);
                    }
                }
            }
        });
}

void GameSession::handleMessage(const NetworkMessage& message) {
    // Process messages based on type
    std::cout << "[Server Session] Handling message type: " << static_cast<int>(message.type)
              << " from player: " << message.senderId 
              << " with data size: " << message.data.size() << " bytes" << std::endl;
    
    switch (message.type) {
        case MessageType::CONNECT:
            processConnect(message);
            break;
        case MessageType::DISCONNECT:
            processDisconnect(message);
            break;
        case MessageType::PLAYER_POSITION:
            std::cout << "[Server Session] Broadcasting position update from player: " << message.senderId << std::endl;
            server_.broadcastMessageExcept(message, message.senderId);
            break;
        case MessageType::PLAYER_ACTION:
            std::cout << "[Server Session] Broadcasting player action from player: " << message.senderId << std::endl;
            server_.broadcastMessageExcept(message, message.senderId);
            break;
        case MessageType::CHAT_MESSAGE:
            processChat(message);
            break;
        case MessageType::PING:
            // Echo back the ping message
            std::cout << "[Server Session] Echoing ping message to player: " << message.senderId << std::endl;
            sendMessage(message);
            break;
        default:
            std::cerr << "[Server Session] Unknown message type: " << static_cast<int>(message.type) << std::endl;
            break;
    }
}

void GameSession::processConnect(const NetworkMessage& message) {
    // A new player has connected
    playerId_ = message.senderId;
    std::cout << "[Server Session] Player connected with ID: " << playerId_ << std::endl;
    
    // Extract the player name if available
    std::string playerName;
    if(!message.data.empty()) {
        playerName = std::string(message.data.begin(), message.data.end());
        std::cout << "[Server Session] Player name: " << playerName << std::endl;
    }
    
    // Add this session to the server's session map
    server_.addSession(playerId_, shared_from_this());
    
    // Broadcast the new player's connection to all existing players
    server_.broadcastMessageExcept(message, playerId_);
    std::cout << "[Server Session] Processed connect message for player: " << playerId_ << std::endl;
}

void GameSession::processDisconnect(const NetworkMessage& message) {
    // A player is disconnecting
    std::cout << "Player disconnected: " << message.senderId << std::endl;
    
    // Broadcast the disconnection to other players
    server_.broadcastMessageExcept(message, message.senderId);
    
    // Remove this session from the server
    if (message.senderId == playerId_) {
        server_.removeSession(playerId_);
        connected_ = false;
    }
}

void GameSession::processChat(const NetworkMessage& message) {
    // Log and broadcast the chat message
    std::string chatMessage(message.data.begin(), message.data.end());
    std::cout << "Chat from " << message.senderId << ": " << chatMessage << std::endl;
    
    // Broadcast to all clients including the sender for confirmation
    server_.broadcastMessage(message);
}

void GameSession::asyncWrite(const std::vector<uint8_t>& data) {
    boost::asio::async_write(socket_, 
        boost::asio::buffer(data),
        [this, self = shared_from_this()](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
            handleWrite(error, 0);
        });
}

void GameSession::handleWrite(const boost::system::error_code& error, size_t /*bytesTransferred*/) {
    if (error) {
        std::cerr << "Write error: " << error.message() << std::endl;
        connected_ = false;
        
        // Remove this session from the server
        if (!playerId_) {
            server_.removeSession(playerId_);
        }
    }
}

// Simple serialization/deserialization for this example
std::vector<uint8_t> GameSession::serializeMessage(const NetworkMessage& message) {
    std::vector<uint8_t> result;
    
    // 1. Message type - 1 byte
    result.push_back(static_cast<uint8_t>(message.type));
    
    // 2. Sender ID length - 1 byte
    result.push_back(static_cast<uint8_t>(message.senderId));
    
    // 3. Data length - 4 bytes
    uint32_t dataSize = static_cast<uint32_t>(message.data.size());
    result.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(dataSize & 0xFF));
    
    // 5. Data content
    result.insert(result.end(), message.data.begin(), message.data.end());
    
    return result;
}

NetworkMessage GameSession::deserializeMessage(const std::vector<uint8_t>& data) {
    NetworkMessage message;
    size_t offset = 0;
    
    if (data.empty()) return message;
    
    // 1. Message type
    message.type = static_cast<MessageType>(data[offset++]);
    
    if (offset >= data.size()) return message;
    
    // 2. Sender ID length
    uint8_t senderIDLength = data[offset++];

    // 3. Sender ID content
    for (size_t i = 0; i < senderIDLength && offset < data.size(); ++i) {
        message.senderId = data[offset++];
    }
    
    if (offset + 4 > data.size()) return message;
    
    // 4. Data length
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