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
        std::cerr << "[Network] Already connected, disconnect first" << std::endl;
        return false;
    }

    server_host_ = host;
    server_port_ = port;

    try {
        io_context_.restart(); // Restart the io_context if it was stopped

        boost::asio::ip::tcp::resolver resolver(io_context_);
        
        // Try to resolve the address with an explicit timeout
        boost::system::error_code ec;
        auto endpoints = resolver.resolve(host, std::to_string(port), ec);
        
        if (ec) {
            std::cerr << "[Network] Failed to resolve host: " << host << ", error: " << ec.message() << std::endl;
            return false;
        }
        
        std::cout << "[Network] Attempting to connect to " << host << ":" << port << std::endl;
        
        // Connect synchronously first to catch immediate failures
        boost::system::error_code connect_ec;
        boost::asio::connect(socket_, endpoints, connect_ec);
        
        if (connect_ec) {
            std::cerr << "[Network] Synchronous connect failed: " << connect_ec.message() << std::endl;
            return false;
        }
        
        // If we got here, connection succeeded
        connected_ = true;
        std::cout << "[Network] Connected to " << host << ":" << port << std::endl;

        // work_guard keeps the io_context running even if there are no handlers
        work_guard_.emplace(boost::asio::make_work_guard(io_context_));

        // Start the ASIO io_context in a separate thread
        io_thread_ = boost::thread([this]() {
            try {
                std::cout << "[Network] IO thread started" << std::endl;
                io_context_.run();
                std::cout << "[Network] IO thread finished" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[Network] Network thread exception: " << e.what() << std::endl;
                connected_ = false;
            }
        });
        
        // Start reading
        startRead();
        
        return connected_;
    } catch (const std::exception& e) {
        std::cerr << "[Network] Connection error: " << e.what() << std::endl;
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

        // Reset the work guard
        if(work_guard_)
        {
            work_guard_->reset();
            work_guard_.reset();
        }

        connected_ = false;
    } catch (const std::exception& e) {
        std::cerr << "Error during disconnect: " << e.what() << std::endl;
    }
}

bool AsioNetworkClient::sendMessage(const NetworkMessage& message) {
    if (!connected_ || !socket_.is_open()) {
        std::cerr << "[AsioNetworkClient] Cannot send message - not connected" << std::endl;
        return false;
    }
    
    try {
        // Create a binary message as shared_ptr to manage message lifetime
        auto buffer_ptr = std::make_shared<std::vector<uint8_t>>();
        auto& buffer = *buffer_ptr;
        
        // Add message type (1 byte)
        buffer.push_back(static_cast<uint8_t>(message.type));
        
        // Add sender ID length (1 byte)
        buffer.push_back(static_cast<uint8_t>(message.senderId.size()));
        
        // Add sender ID content
        buffer.insert(buffer.end(), message.senderId.begin(), message.senderId.end());
        
        // Add data length (4 bytes)
        uint32_t dataSize = static_cast<uint32_t>(message.data.size());
        buffer.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(dataSize & 0xFF));
        
        // Add message data
        buffer.insert(buffer.end(), message.data.begin(), message.data.end());
        
        // Create the message header
        MessageHeader header;
        header.size = static_cast<uint32_t>(buffer.size());
        
        // Create the complete message (header + body)
        auto complete_message_ptr = std::make_shared<std::vector<uint8_t>>();
        auto& completeMessage = *complete_message_ptr;
        completeMessage.resize(sizeof(header) + buffer.size());
        
        // Copy the header
        std::memcpy(completeMessage.data(), &header, sizeof(header));
        
        // Copy the message body
        std::memcpy(completeMessage.data() + sizeof(header), buffer.data(), buffer.size());

        //Store the buffer in collection to keep alive
        {
            std::lock_guard<std::mutex> lock(outgoing_messages_mutex_);
            outgoing_messages_.push_back(buffer_ptr);
        }
        
        // Send the message asynchronously
        boost::asio::async_write(socket_,
            boost::asio::buffer(completeMessage),
            [this, complete_message_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
                // Clean up the buffer after operation completes
                {
                    std::lock_guard<std::mutex> lock(outgoing_messages_mutex_);
                    outgoing_messages_.erase(
                        std::remove(outgoing_messages_.begin(), outgoing_messages_.end(), complete_message_ptr),
                        outgoing_messages_.end());
                }
                
                if (error) {
                    std::cerr << "[AsioNetworkClient] Error sending message: " << error.message() << std::endl;
                    connected_ = false;
                }
            });
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[AsioNetworkClient] Error preparing message: " << e.what() << std::endl;
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
    if (!socket_.is_open()) {
        std::cerr << "[Network] Cannot start read, socket is closed" << std::endl;
        return;
    }

    // First, read the message header to know the size
    boost::asio::async_read(socket_,
        boost::asio::buffer(&read_buffer_[0], sizeof(MessageHeader)),
        [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (!error && bytes_transferred == sizeof(MessageHeader)) {
                // Read the message header
                MessageHeader* header = reinterpret_cast<MessageHeader*>(read_buffer_.data());

                // Log raw header data for debugging
                // std::cout << "[Network] Raw header data: ";
                // for (size_t i = 0; i < sizeof(MessageHeader); ++i) {
                //     std::cout << std::hex << static_cast<int>(read_buffer_[i]) << " ";
                // }
                // std::cout << std::dec << std::endl;

                // Sanity check for message size
                if (header->size <= 0 || header->size >= max_buffer_size) {
                    // std::cerr << "[Network] Invalid message size: " << header->size << std::endl;
                    startRead(); // Restart reading
                    return;
                }

                // Now read the message body
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
                        } else if (error) {
                            std::cerr << "[Network] Error reading message body: " << error.message() << std::endl;
                        }

                        // Continue reading
                        startRead();
                    });
            } else if (error != boost::asio::error::operation_aborted) {
                // Handle error but don't restart if we're intentionally shutting down
                if (connected_) {
                    std::cerr << "[Network] Read error: " << error.message() << std::endl;
                    connected_ = false;
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
    
    if (!received_messages_.empty()) {
        // std::cout << "[Network] Processing " << received_messages_.size() << " received messages" << std::endl;
    }
    
    while (!received_messages_.empty()) {
        if (message_handler_) {
            // std::cout << "[Network] Dispatching message type: " << static_cast<int>(received_messages_.front().type) 
            //           << " from " << received_messages_.front().senderId << std::endl;
            message_handler_(received_messages_.front());
        } else {
            std::cerr << "[Network] Warning: No message handler registered" << std::endl;
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
    
    // // 3. Sender ID content
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