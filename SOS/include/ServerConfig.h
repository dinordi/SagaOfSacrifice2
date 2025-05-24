#pragma once

#include <string>
#include <vector>
#include <memory>

struct ServerInfo {
    std::string name;
    std::string address;
    int port;
    std::string description;
    bool isDefault;
    
    ServerInfo() : name(""), address("localhost"), port(8080), description(""), isDefault(false) {}
    
    ServerInfo(const std::string& name, const std::string& address, int port, 
               const std::string& description = "", bool isDefault = false)
        : name(name), address(address), port(port), description(description), isDefault(isDefault) {}
};

class ServerConfig {
public:
    ServerConfig();
    ~ServerConfig() = default;
    
    // Load server configurations from JSON file
    bool loadFromFile(const std::string& configPath);
    
    // Get list of available servers
    const std::vector<ServerInfo>& getServers() const;
    
    // Get default server (first one marked as default, or first in list)
    const ServerInfo* getDefaultServer() const;
    
    // Get server by index
    const ServerInfo* getServer(size_t index) const;
    
    // Get number of servers
    size_t getServerCount() const;
    
    // Add a server programmatically
    void addServer(const ServerInfo& server);
    
    // Save current configuration to file
    bool saveToFile(const std::string& configPath) const;
    
private:
    std::vector<ServerInfo> servers_;
    void addDefaultServers();
};
