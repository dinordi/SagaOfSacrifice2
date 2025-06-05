#include "ServerConfig.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

ServerConfig::ServerConfig() {
    addDefaultServers();
}

bool ServerConfig::loadFromFile(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "[ServerConfig] Could not open server config file: " << configPath << std::endl;
            std::cerr << "[ServerConfig] Using default server configuration" << std::endl;
            return false;
        }
        
        json configJson;
        file >> configJson;
        file.close();
        
        servers_.clear();
        
        if (configJson.contains("servers") && configJson["servers"].is_array()) {
            for (const auto& serverJson : configJson["servers"]) {
                ServerInfo server;
                
                if (serverJson.contains("name") && serverJson["name"].is_string()) {
                    server.name = serverJson["name"];
                } else {
                    std::cerr << "[ServerConfig] Server entry missing required 'name' field" << std::endl;
                    continue;
                }
                
                if (serverJson.contains("address") && serverJson["address"].is_string()) {
                    server.address = serverJson["address"];
                } else {
                    std::cerr << "[ServerConfig] Server '" << server.name << "' missing required 'address' field" << std::endl;
                    continue;
                }
                
                if (serverJson.contains("port") && serverJson["port"].is_number_integer()) {
                    server.port = serverJson["port"];
                } else {
                    std::cerr << "[ServerConfig] Server '" << server.name << "' missing required 'port' field" << std::endl;
                    continue;
                }
                
                if (serverJson.contains("description") && serverJson["description"].is_string()) {
                    server.description = serverJson["description"];
                }
                
                if (serverJson.contains("default") && serverJson["default"].is_boolean()) {
                    server.isDefault = serverJson["default"];
                }
                
                servers_.push_back(server);
                std::cout << "[ServerConfig] Loaded server: " << server.name 
                          << " (" << server.address << ":" << server.port << ")" << std::endl;
            }
        }
        
        // If no servers were loaded, add defaults
        if (servers_.empty()) {
            std::cerr << "[ServerConfig] No valid servers found in config file, using defaults" << std::endl;
            addDefaultServers();
            return false;
        }
        
        std::cout << "[ServerConfig] Successfully loaded " << servers_.size() << " server(s) from " << configPath << std::endl;
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "[ServerConfig] JSON parsing error: " << e.what() << std::endl;
        std::cerr << "[ServerConfig] Using default server configuration" << std::endl;
        servers_.clear();
        addDefaultServers();
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[ServerConfig] Error loading server config: " << e.what() << std::endl;
        std::cerr << "[ServerConfig] Using default server configuration" << std::endl;
        servers_.clear();
        addDefaultServers();
        return false;
    }
}

const std::vector<ServerInfo>& ServerConfig::getServers() const {
    return servers_;
}

const ServerInfo* ServerConfig::getDefaultServer() const {
    // First, look for a server marked as default
    for (const auto& server : servers_) {
        if (server.isDefault) {
            return &server;
        }
    }
    
    // If no default found, return first server
    if (!servers_.empty()) {
        return &servers_[0];
    }
    
    return nullptr;
}

const ServerInfo* ServerConfig::getServer(size_t index) const {
    if (index < servers_.size()) {
        return &servers_[index];
    }
    return nullptr;
}

size_t ServerConfig::getServerCount() const {
    return servers_.size();
}

void ServerConfig::addServer(const ServerInfo& server) {
    servers_.push_back(server);
}

bool ServerConfig::saveToFile(const std::string& configPath) const {
    try {
        json configJson;
        json serversArray = json::array();
        
        for (const auto& server : servers_) {
            json serverJson;
            serverJson["name"] = server.name;
            serverJson["address"] = server.address;
            serverJson["port"] = server.port;
            serverJson["description"] = server.description;
            serverJson["default"] = server.isDefault;
            serversArray.push_back(serverJson);
        }
        
        configJson["servers"] = serversArray;
        
        std::ofstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "[ServerConfig] Could not open file for writing: " << configPath << std::endl;
            return false;
        }
        
        file << configJson.dump(4); // Pretty print with 4 spaces
        file.close();
        
        std::cout << "[ServerConfig] Successfully saved server configuration to " << configPath << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ServerConfig] Error saving server config: " << e.what() << std::endl;
        return false;
    }
}

void ServerConfig::addDefaultServers() {
    servers_.clear();
    
    // Add localhost server (default for testing)
    servers_.emplace_back("Local Server", "localhost", 8080, "Local development server", true);
    
    // Add some example servers
    servers_.emplace_back("Official Server", "game.sagaofsacrifice.com", 8080, "Official game server", false);
    servers_.emplace_back("EU Server", "eu.sagaofsacrifice.com", 8080, "European server", false);
    servers_.emplace_back("US Server", "us.sagaofsacrifice.com", 8080, "United States server", false);
    
    std::cout << "[ServerConfig] Added default server configurations" << std::endl;
}
