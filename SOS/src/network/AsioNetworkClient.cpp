#include "network/AsioNetworkClient.h"
#include <iostream>
#include <functional>

AsioNetworkClient::AsioNetworkClient() 
    : socket_(io_context_), 
      connected_(false), 
      server_port_(0) {
    read_buffer_.resize(max_buffer_size);
}

AsioNetworkClient::~AsioNetworkClient() {
    disconnect();
}

bool AsioNetworkClient::connect(const std::string& host, int port) {
    if (connected_) {
        std::cerr << "Already connected, disconnect first" << std::endl;
        return false;
    }

    server_host_ = host;
    server_port_ = port;

    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));

        // Ensure the handler signature matches one of the expected overloads.
        // Common signature takes error_code and the endpoint type.
        boost::asio::async_connect(socket_, endpoints,
            [this](const boost::system::error_code& error,
                   const boost::asio::ip::tcp::endpoint& /* endpoint */) { // Using const& is common
                handleConnect(error);
            });
        

        // Start the ASIO io_context in a separate thread
        io_thread_ = boost::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                std::cerr << "Network thread exception: " << e.what() << std::endl;
                connected_ = false;
            }
        });

        // Wait briefly to ensure connection has time to be established
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        return connected_;
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        return false;
    }
}

void AsioNetworkClient::disconnect() {
    if (!connected_) return;

    try {
        // Close the socket
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close();
        }

        // Stop the io_context and join the thread
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }

        connected_ = false;
    } catch (const std::exception& e) {
        std::cerr << "Error during disconnect: " << e.what() << std::endl;
    }
}

bool AsioNetworkClient::sendMessage(const NetworkMessage& message) {
    if (!connected_ || !socket_.is_open()) {
        return false;
    }

    try {
        // Serialize the message
        std::vector<uint8_t> serializedMsg = serializeMessage(message);
        
        // Prefix with message size
        MessageHeader header;
        header.size = static_cast<uint32_t>(serializedMsg.size());
        
        // Create combined buffer with header + message
        std::vector<uint8_t> buffer;
        buffer.resize(sizeof(header) + serializedMsg.size());
        
        // Copy header
        std::memcpy(buffer.data(), &header, sizeof(header));
        
        // Copy message body
        std::memcpy(buffer.data() + sizeof(header), serializedMsg.data(), serializedMsg.size());
        
        // Send the message asynchronously
        boost::asio::async_write(socket_, 
            boost::asio::buffer(buffer, buffer.size()),
            [this](const boost::system::error_code& error, std::size_t) {
                handleWrite(error);
            });
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
        return false;
    }
}

void AsioNetworkClient::setMessageHandler(std::function<void(const NetworkMessage&)> handler) {
    message_handler_ = handler;
}

void AsioNetworkClient::update() {
    processMessageQueue();
}

bool AsioNetworkClient::isConnected() const {
    return connected_ && socket_.is_open();
}

void AsioNetworkClient::handleConnect(const boost::system::error_code& error) {
    if (!error) {
        connected_ = true;
        std::cout << "Connected to server: " << server_host_ << ":" << server_port_ << std::endl;
        startRead();
    } else {
        std::cerr << "Connection failed: " << error.message() << std::endl;
        connected_ = false;
    }
}

void AsioNetworkClient::startRead() {
    if (!socket_.is_open()) return;
    
    // First, read the message header to know the size
    boost::asio::async_read(socket_,
        boost::asio::buffer(&read_buffer_[0], sizeof(MessageHeader)),
        [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (!error && bytes_transferred == sizeof(MessageHeader)) {
                // Read the message header
                MessageHeader* header = reinterpret_cast<MessageHeader*>(read_buffer_.data());
                
                // Now read the message body
                if (header->size > 0 && header->size < max_buffer_size) {
                    boost::asio::async_read(socket_,
                        boost::asio::buffer(&read_buffer_[sizeof(MessageHeader)], header->size),
                        [this, header](const boost::system::error_code& error, std::size_t bytes_transferred) {
                            if (!error && bytes_transferred == header->size) {
                                // Process the complete message
                                std::vector<uint8_t> messageData(
                                    read_buffer_.begin() + sizeof(MessageHeader),
                                    read_buffer_.begin() + sizeof(MessageHeader) + header->size
                                );
                                
                                // Deserialize and add to queue
                                NetworkMessage message = deserializeMessage(messageData);
                                std::lock_guard<std::mutex> lock(message_mutex_);
                                received_messages_.push(message);
                            }
                            
                            // Continue reading
                            startRead();
                        });
                } else {
                    // Invalid message size, restart reading
                    startRead();
                }
            } else if (error != boost::asio::error::operation_aborted) {
                // Handle error but don't restart if we're intentionally shutting down
                if (connected_) {
                    std::cerr << "Read error: " << error.message() << std::endl;
                    // Try to reconnect or handle disconnection
                    connected_ = false;
                    // Could implement reconnection logic here
                }
            }
        });
}

void AsioNetworkClient::handleRead(const boost::system::error_code& error, size_t bytesTransferred) {
    if (!error) {
        // If we successfully read data, process it
        if (bytesTransferred > 0) {
            // Deserialize the message
            std::vector<uint8_t> messageData(read_buffer_.begin(), read_buffer_.begin() + bytesTransferred);
            NetworkMessage message = deserializeMessage(messageData);
            
            // Add to the received message queue
            std::lock_guard<std::mutex> lock(message_mutex_);
            received_messages_.push(message);
        }
        
        // Start another read operation
        startRead();
    } else if (error != boost::asio::error::operation_aborted) {
        // Error occurred
        std::cerr << "HandleRead Read error: " << error.message() << std::endl;
        connected_ = false;
    }
}

void AsioNetworkClient::handleWrite(const boost::system::error_code& error) {
    if (error) {
        std::cerr << "Write error: " << error.message() << std::endl;
        connected_ = false;
    }
}

void AsioNetworkClient::processMessageQueue() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    
    while (!received_messages_.empty()) {
        if (message_handler_) {
            message_handler_(received_messages_.front());
        }
        received_messages_.pop();
    }
}

// Simple serialization/deserialization for this example
// In a real application, consider using a proper serialization library
std::vector<uint8_t> AsioNetworkClient::serializeMessage(const NetworkMessage& message) {
    std::vector<uint8_t> result;
    
    // 1. Message type - 1 byte
    result.push_back(static_cast<uint8_t>(message.type));
    
    // 2. Sender ID length - 1 byte
    result.push_back(static_cast<uint8_t>(message.senderId.size()));
    
    // 3. Sender ID content
    result.insert(result.end(), message.senderId.begin(), message.senderId.end());
    
    // 4. Data length - 4 bytes
    uint32_t dataSize = static_cast<uint32_t>(message.data.size());
    result.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(dataSize & 0xFF));
    
    // 5. Data content
    result.insert(result.end(), message.data.begin(), message.data.end());
    
    return result;
}

NetworkMessage AsioNetworkClient::deserializeMessage(const std::vector<uint8_t>& data) {
    NetworkMessage message;
    size_t offset = 0;
    
    if (data.empty()) return message;
    
    // 1. Message type
    message.type = static_cast<MessageType>(data[offset++]);
    
    if (offset >= data.size()) return message;
    
    // 2. Sender ID length
    uint8_t senderIdLength = data[offset++];
    
    // 3. Sender ID content
    if (offset + senderIdLength <= data.size()) {
        message.senderId.assign(data.begin() + offset, data.begin() + offset + senderIdLength);
        offset += senderIdLength;
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