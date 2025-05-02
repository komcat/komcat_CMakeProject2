#pragma once

#include "include/tcp_client.h"
#include <vector>
#include <string>
#include <memory>

// Structure to hold client information
struct ClientInfo {
    std::shared_ptr<TcpClient> client;
    std::string serverIp;
    int serverPort;
    bool connected;
    char statusMessage[128];
    float receivedValues[100]; // Array to store last 100 values for visualization
    int valuesCount;
    int valuesCursor;

    // Constructor with defaults
    ClientInfo() :
        client(std::make_shared<TcpClient>()),
        serverIp("127.0.0.1"),
        serverPort(8888),
        connected(false),
        valuesCount(0),
        valuesCursor(0) {
        strcpy_s(statusMessage, sizeof(statusMessage), "Not connected");

        // Initialize the received values array
        for (int i = 0; i < 100; i++) {
            receivedValues[i] = 0.0f;
        }
    }
};

// Class to manage multiple TCP clients
class ClientManager {
private:
    std::vector<ClientInfo> m_clients;

public:
    ClientManager();
    ~ClientManager();

    // Add a new client with default settings
    int AddClient();

    // Remove a client by index
    bool RemoveClient(int index);

    // Get the number of clients
    size_t GetClientCount() const;

    // Get a specific client info reference
    ClientInfo& GetClientInfo(int index);

    // Connect a specific client
    bool ConnectClient(int index, const std::string& ip, int port);

    // Disconnect a specific client
    void DisconnectClient(int index);

    // Update all clients (call this every frame)
    void UpdateClients();

    // Render the UI for all clients
    void RenderUI();
};