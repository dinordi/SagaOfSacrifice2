#pragma once

#include <vector>
#include <cstdint>

// Enum for different types of network messages - same as client side
enum class MessageType {
    PLAYER_POSITION,   // Position update of a player
    PLAYER_ACTION,     // Special action (jump, attack, etc.)
    PLAYER_INPUT,      // Player input state (new type for server-controlled physics)
    GAME_STATE,        // Complete or partial game state update
    GAME_STATE_DELTA,  // Delta game state update (only changed objects)
    GAME_STATE_PART,   // Part of a multi-packet game state update
    CHAT_MESSAGE,      // Text chat
    CONNECT,           // Player connected
    DISCONNECT,        // Player disconnected
    PING,               // Network ping/pong
    PLAYER_JOINED,     // Player joined the game
    PLAYER_LEFT,       // Player left the game
    CHAT,            // Chat message
    ENEMY_STATE_UPDATE, // Update enemy state (e.g., health, dead)
    PLAYER_ASSIGN,     // Assign a player to a client
};

// Base message structure - same as client side
struct NetworkMessage {
    MessageType type;
    uint16_t senderId;
    uint16_t targetId; // Used for messages directed at a specific client
    std::vector<uint8_t> data;
};
