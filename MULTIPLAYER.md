# Saga of Sacrifice 2 - Multiplayer Documentation

This document describes how to set up and use the multiplayer functionality for Saga of Sacrifice 2, implemented using Boost.Asio for network communication.

## Overview

The multiplayer system consists of two main components:

1. **Client** - Integrated directly into the game (SOS directory)
2. **Server** - A standalone application (server directory) that routes messages between clients

## Requirements

- Boost libraries (system, thread)
- C++17 compatible compiler
- CMake 3.10 or newer

## Building the Server

Navigate to the server directory and build using CMake:

```bash
cd server
mkdir build
cd build
cmake ..
make
```

This will create an executable named `SagaServer`.

## Running the Server

Start the server by running:

```bash
./SagaServer [port]
```

- `port` is optional - if not provided, the server will listen on port 8080

To stop the server, press Ctrl+C.

## Building the Client

The multiplayer functionality is already integrated into the main game. Ensure it's built with Boost libraries:

```bash
cd SOS
mkdir build
cd build
cmake ..
make
```

## Running the Game in Multiplayer Mode

Start the game with the following command-line options for multiplayer:

```bash
./SOS -m [-s server_address] [-p port] [-id player_id]
```

Where:
- `-m` or `--multiplayer`: Enables multiplayer mode
- `-s` or `--server`: Specifies the server address (default: localhost)
- `-p` or `--port`: Specifies the server port (default: 8080)
- `-id` or `--playerid`: Sets a specific player ID (default: random ID)

Example:
```bash
./SOS --multiplayer --server 192.168.1.100 --port 8080
```

## Architecture

### Network Protocol

The multiplayer system uses a simple message-based protocol:

1. **Message Header** (4 bytes): Contains the size of the following message
2. **Message Type** (1 byte): Identifies the type of message
3. **Sender ID Length** (1 byte): Length of the sender ID string
4. **Sender ID** (variable): Player identifier
5. **Data Length** (4 bytes): Size of the message payload
6. **Data** (variable): Message payload

### Message Types

- `PLAYER_POSITION`: Updates player position and velocity
- `PLAYER_ACTION`: Indicates special actions (jumping, attacking)
- `GAME_STATE`: Server-to-client game state updates
- `CHAT_MESSAGE`: Text chat messages
- `CONNECT`: Player connection notification
- `DISCONNECT`: Player disconnection notification
- `PING`: Network connectivity check

### Implementation Details

#### Client Side

- `NetworkInterface`: Abstract interface for network communication
- `AsioNetworkClient`: Boost.Asio implementation of NetworkInterface
- `MultiplayerManager`: Coordinates network communication with game state
- `RemotePlayer`: Represents other players in the multiplayer session

#### Server Side

- `GameServer`: Manages client connections and message routing
- `GameSession`: Handles individual client connections
- `NetworkMessage`: Message structure shared between client and server

## Cross-Platform Considerations

The multiplayer implementation is designed to be cross-platform compatible:

- Works on Linux, macOS, and Windows
- Can run on Petalinux for embedded systems
- Supports most network configurations with proper port forwarding

## Known Limitations

- No built-in authentication or encryption
- No prediction or lag compensation
- Simple message serialization (consider using a proper serialization library for production)
- Limited error recovery - connection loss requires manual reconnection

## Future Improvements

- Add authentication and encryption
- Implement lag compensation and movement prediction
- Add lobby system for matchmaking
- Support multiple game rooms
- Add spectator mode