#pragma once

#include "NetworkInterface.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <queue>
#include <mutex>
#include <optional>

#include <boost/bind/bind.hpp>
using namespace boost::placeholders;

class AsioNetworkClient : public NetworkInterface {
public:
    AsioNetworkClient();
    ~AsioNetworkClient() override;

    // NetworkInterface implementation
    bool connect(const std::string& host, int port) override;
    void disconnect() override;
    bool sendMessage(const NetworkMessage& message) override;
    void setMessageHandler(std::function<void(const NetworkMessage&)> handler) override;
    void update() override;
    bool isConnected() const override;
    void setClientId(const uint16_t clientId) override;

private:
    // Asio-specific implementation details
    void handleConnect(const boost::system::error_code& error);
    void startRead();
    void handleRead(const boost::system::error_code& error, size_t bytesTransferred);
    void handleWrite(const boost::system::error_code& error);
    void processMessageQueue();

    // Network message serialization/deserialization
    std::vector<uint8_t> serializeMessage(const NetworkMessage& message);
    NetworkMessage deserializeMessage(const std::vector<uint8_t>& data);

    
private:
    std::mutex outgoing_messages_mutex_;
    std::vector<std::shared_ptr<std::vector<uint8_t>>> outgoing_messages_;
    
    // Asio io_context and socket
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::thread io_thread_;

    // Work guard to keep io_context running
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    
    // Connection state
    bool connected_;
    std::string server_host_;
    int server_port_;
    uint16_t client_id_; // Client ID for message sending
    
    // Message handling
    std::function<void(const NetworkMessage&)> message_handler_;
    std::queue<NetworkMessage> received_messages_;
    std::mutex message_mutex_;
    
    // Message read buffer
    enum { max_buffer_size = 8192 };
    std::vector<uint8_t> read_buffer_;
    
    // Message header contains the size of the following message
    struct MessageHeader {
        uint32_t size;
    };
};