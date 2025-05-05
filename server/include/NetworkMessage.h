#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Enum for different types of network messages - same as client side
enum class MessageType {
    PLAYER_POSITION,
    PLAYER_ACTION,
    GAME_STATE,
    CHAT_MESSAGE,
    CONNECT,
    DISCONNECT,
    PING
};

// Base message structure - same as client side
struct NetworkMessage {
    MessageType type;
    std::string senderId;
    std::vector<uint8_t> data;
};
