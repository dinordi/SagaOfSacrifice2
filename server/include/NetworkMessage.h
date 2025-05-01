#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Enum for different types of network messages - same as client side
enum class MessageType {
    PLAYER_POSITION,   // Position update of a player
    PLAYER_ACTION,     // Special action (jump, attack, etc.)
    PLAYER_INPUT,      // Player input state (new type for server-controlled physics)
    GAME_STATE,        // Complete or partial game state update
    CHAT_MESSAGE,      // Text chat
    CONNECT,           // Player connected
    DISCONNECT,        // Player disconnected
    PING               // Network ping/pong
};

// Base message structure - same as client side
struct NetworkMessage {
    MessageType type;
    std::string senderId;
    std::vector<uint8_t> data;
};
