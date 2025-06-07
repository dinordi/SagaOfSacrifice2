# System Overview

```mermaid
flowchart TD
    SERVER -->|TCP/UDP| WRAPPER_CLIENT
    SERVER -->|TCP/UDP| ZYBO_CLIENT
    SERVER -->|Game Locic API| GAME_CORE
    WRAPPER_CLIENT -->|Game Logic API| GAME_CORE
    ZYBO_CLIENT -->|Game Logic API| GAME_CORE

    SERVER[Server Standalone C++]
    WRAPPER_CLIENT[wrapper_client SDL3 Wrapper]
    ZYBO_CLIENT[client/ Zybo Z7 10 FPGA]
    GAME_CORE[SOS/ Game Logic Core]
```

- **Server**: Standalone C++ application handling multiplayer for all clients.
- **wrapper_client**: SDL3-based client for desktop (Linux/Windows/macOS).
- **client/**: Zybo Z7 10 FPGA client, uses custom graphics accelerated by the FPGA.
- **SOS/**: Shared core game logic, used by both types of clients.


---
