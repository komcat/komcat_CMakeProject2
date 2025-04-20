#include "client_manager.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

ClientManager::ClientManager() {
    // Start with one client by default
    AddClient();
}

ClientManager::~ClientManager() {
    // Disconnect all clients
    for (size_t i = 0; i < m_clients.size(); i++) {
        if (m_clients[i].connected) {
            m_clients[i].client->Disconnect();
        }
    }
}

int ClientManager::AddClient() {
    // Create a new client info and add it to the vector
    ClientInfo newClient;
    m_clients.push_back(newClient);

    // Return the index of the new client
    return static_cast<int>(m_clients.size() - 1);
}

bool ClientManager::RemoveClient(int index) {
    // Check if the index is valid
    if (index < 0 || index >= static_cast<int>(m_clients.size())) {
        return false;
    }

    // Disconnect the client if connected
    if (m_clients[index].connected) {
        m_clients[index].client->Disconnect();
    }

    // Remove the client from the vector
    m_clients.erase(m_clients.begin() + index);

    return true;
}

size_t ClientManager::GetClientCount() const {
    return m_clients.size();
}

ClientInfo& ClientManager::GetClientInfo(int index) {
    return m_clients[index];
}

bool ClientManager::ConnectClient(int index, const std::string& ip, int port) {
    // Check if the index is valid
    if (index < 0 || index >= static_cast<int>(m_clients.size())) {
        return false;
    }

    // Update the client info
    ClientInfo& info = m_clients[index];
    info.serverIp = ip;
    info.serverPort = port;

    // Connect the client
    info.connected = info.client->Connect(ip, port);

    // Update status message
    snprintf(info.statusMessage, sizeof(info.statusMessage),
        info.connected ? "Connected to %s:%d" : "Failed to connect to %s:%d",
        ip.c_str(), port);

    return info.connected;
}

void ClientManager::DisconnectClient(int index) {
    // Check if the index is valid
    if (index < 0 || index >= static_cast<int>(m_clients.size())) {
        return;
    }

    // Disconnect the client
    ClientInfo& info = m_clients[index];
    info.client->Disconnect();
    info.connected = false;

    // Update status message
    snprintf(info.statusMessage, sizeof(info.statusMessage),
        "Disconnected from %s:%d", info.serverIp.c_str(), info.serverPort);
}

void ClientManager::UpdateClients() {
    // Update all clients
    for (size_t i = 0; i < m_clients.size(); i++) {
        ClientInfo& info = m_clients[i];

        // Check connection status
        if (info.connected && !info.client->IsConnected()) {
            info.connected = false;
            snprintf(info.statusMessage, sizeof(info.statusMessage),
                "Connection lost to %s:%d", info.serverIp.c_str(), info.serverPort);
        }

        // If connected, update received values
        if (info.connected) {
            // Get all new values since last frame
            std::deque<float> newValues = info.client->GetReceivedValues();

            // Update the circular buffer with new values
            for (float val : newValues) {
                info.receivedValues[info.valuesCursor] = val;
                info.valuesCursor = (info.valuesCursor + 1) % 100;
                if (info.valuesCount < 100)
                    info.valuesCount++;
            }
        }
    }
}

void ClientManager::RenderUI() {
    static bool showAddClientPopup = false;

    // Main window for client management
    ImGui::Begin("TCP Client Manager");

    // Add/Remove client buttons
    if (ImGui::Button("Add Client")) {
        AddClient();
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove Last Client") && m_clients.size() > 1) {
        RemoveClient(static_cast<int>(m_clients.size() - 1));
    }

    ImGui::Separator();

    // Render each client in a collapsible header
    for (size_t i = 0; i < m_clients.size(); i++) {
        ClientInfo& info = m_clients[i];

        // Create header with client index
        char headerLabel[32];
        snprintf(headerLabel, sizeof(headerLabel), "Client %zu", i + 1);

        if (ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen)) {
            // Client-specific identifier for ImGui widgets
            char idPrefix[32];
            snprintf(idPrefix, sizeof(idPrefix), "##Client%zu", i);

            // Connection settings
            char ipBuffer[64];
            strncpy(ipBuffer, info.serverIp.c_str(), sizeof(ipBuffer));

            char ipInputId[64];
            snprintf(ipInputId, sizeof(ipInputId), "Server IP%s", idPrefix);
            if (ImGui::InputText(ipInputId, ipBuffer, sizeof(ipBuffer))) {
                info.serverIp = ipBuffer;
            }

            char portInputId[64];
            snprintf(portInputId, sizeof(portInputId), "Server Port%s", idPrefix);
            ImGui::InputInt(portInputId, &info.serverPort);

            // Connect / Disconnect button
            if (!info.connected) {
                char connectBtnId[64];
                snprintf(connectBtnId, sizeof(connectBtnId), "Connect%s", idPrefix);
                if (ImGui::Button(connectBtnId)) {
                    ConnectClient(static_cast<int>(i), info.serverIp, info.serverPort);
                }
            }
            else {
                char disconnectBtnId[64];
                snprintf(disconnectBtnId, sizeof(disconnectBtnId), "Disconnect%s", idPrefix);
                if (ImGui::Button(disconnectBtnId)) {
                    DisconnectClient(static_cast<int>(i));
                }
            }

            // Display connection status
            ImGui::Text("Status: %s", info.statusMessage);

            // Show received data if connected
            if (info.connected) {
                ImGui::Separator();

                // Display latest received value
                ImGui::Text("Latest received value: %.6f", info.client->GetLatestValue());

                // Debug info
                ImGui::Text("Values in buffer: %d", info.valuesCount);

                // Plot the received values
                ImGui::Separator();
                ImGui::Text("Received Values History:");

                // Calculate min/max for better scaling
                float minValue = 0.0f;
                float maxValue = 1.0f;

                if (info.valuesCount > 0) {
                    // Initialize with first value
                    minValue = info.receivedValues[0];
                    maxValue = info.receivedValues[0];

                    // Find actual min/max
                    for (int j = 0; j < info.valuesCount; j++) {
                        minValue = (std::min)(minValue, info.receivedValues[j]);
                        maxValue = (std::max)(maxValue, info.receivedValues[j]);
                    }

                    // Add margins (10% padding)
                    float range = maxValue - minValue;
                    if (range < 0.001f) range = 0.1f;  // Prevent too small ranges

                    float margin = range * 0.1f;
                    minValue = (std::min)(0.0f, minValue - margin);
                    maxValue = (std::max)(1.0f, maxValue + margin);
                }

                // Create unique ID for the plot
                char plotId[64];
                snprintf(plotId, sizeof(plotId), "##values%s", idPrefix);

                // Plot the values
                ImGui::PlotLines(plotId,
                    info.receivedValues,       // Array
                    info.valuesCount,          // Count
                    info.valuesCursor,         // Offset
                    nullptr,                   // Overlay text
                    minValue,                  // Y-min
                    maxValue,                  // Y-max
                    ImVec2(0, 80),            // Graph size
                    sizeof(float));           // Stride

                ImGui::Text("Min displayed: %.2f, Max displayed: %.2f", minValue, maxValue);
            }

            ImGui::Separator();
        }
    }

    ImGui::End();
}