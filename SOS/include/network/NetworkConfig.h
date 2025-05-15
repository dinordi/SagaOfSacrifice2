#pragma once
#include <cstdint>


namespace NetworkConfig 
{
    namespace Client {
        constexpr float InterpolationPeriod = 0.1f; // 100ms

        constexpr uint64_t UpdateInterval = 20;  // 50 updates per second
        
        constexpr float PositionErrorThreshold = 5.0f; // Small threshold to ignore minor pixel differences
        constexpr float ReconciliationBlendFactor = 0.5f; // Blend factor for reconciliation (0-1)
    }

    namespace Server {
        constexpr int TickRate = 60; // 60 ticks per second

        constexpr uint64_t StateUpdateInterval = 20; // 50 updates per second

        // Other settings can be added here like gravity or max velocity
    }

    //Shared settings
    constexpr int MaxPlayers = 4; // Maximum number of players in the game
    constexpr int MaxMessageSize = 1024; // Maximum size of a network message
    constexpr int MaxChatMessageSize = 256; // Maximum size of a chat message
    constexpr int MaxObjectCount = 100; // Maximum number of game objects in the world

    constexpr int DefaultServerPort = 8282;
}