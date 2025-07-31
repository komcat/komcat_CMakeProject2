
// TCPDataManagerUI.cpp
#include "TCPDataManagerUI.h"
#include "imgui.h"
#include <iostream>

TCPDataManagerUI::TCPDataManagerUI() {
    // Constructor - don't initialize here, do it in Initialize()
}

TCPDataManagerUI::~TCPDataManagerUI() {
    // Destructor - no cleanup needed since we don't own the pointer
}

bool TCPDataManagerUI::Initialize(DataClientManager* dataClientManager) {
    if (!dataClientManager) {
        std::cerr << "Failed to initialize TCPDataManagerUI: dataClientManager is nullptr" << std::endl;
        m_isInitialized = false;
        return false;
    }

    m_dataClientManager = dataClientManager;
    m_isInitialized = true;
    std::cout << "TCPDataManagerUI initialized with external DataClientManager" << std::endl;
    return true;
}

void TCPDataManagerUI::Update() {
    if (!m_isInitialized || !m_dataClientManager) {
        return;
    }

    // Update the data client manager
    m_dataClientManager->UpdateClients();
}

void TCPDataManagerUI::Render() {
    if (!m_isInitialized || !m_dataClientManager) {
        // Show error state
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "TCP Data Manager - Not Initialized");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Text("The TCP Data Manager failed to initialize.");
        ImGui::BulletText("DataClientManager pointer is null");
        ImGui::BulletText("Check main() initialization");

        return;
    }

    // Header
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("TCP Data Manager");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("TCP client connections and data streaming from DataServerConfig.json");
    ImGui::Separator();

    // Show client count and status
    size_t clientCount = m_dataClientManager->GetClientCount();
    ImGui::Text("Configured Servers: %zu", clientCount);

    // Quick connection overview
    if (clientCount > 0) {
        int connectedCount = 0;
        for (size_t i = 0; i < clientCount; ++i) {
            auto& clientInfo = m_dataClientManager->GetClientInfo(static_cast<int>(i));
            if (clientInfo.connected) {
                connectedCount++;
            }
        }

        ImGui::SameLine();
        if (connectedCount == clientCount) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(%d/%zu Connected)", connectedCount, clientCount);
        }
        else if (connectedCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(%d/%zu Connected)", connectedCount, clientCount);
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(%d/%zu Connected)", connectedCount, clientCount);
        }
    }

    ImGui::Spacing();

    // Quick action buttons
    if (ImGui::Button("Connect All Auto-Connect Servers")) {
        m_dataClientManager->ConnectAutoClients();
    }

    ImGui::SameLine();
    if (ImGui::Button("Disconnect All")) {
        for (size_t i = 0; i < clientCount; ++i) {
            m_dataClientManager->DisconnectClient(static_cast<int>(i));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // The main data client manager UI
    // Force it to be visible for our sub-page
    bool wasVisible = m_dataClientManager->IsVisible();
    if (!wasVisible) {
        m_dataClientManager->ToggleWindow(); // Make it visible
    }

    // Render the data client manager UI
    m_dataClientManager->RenderUI();

    // If it wasn't visible before, hide it again (but we'll show it next frame anyway)
    // This is a bit of a hack to integrate the existing UI into our sub-page
    // Alternative: We could refactor DataClientManager to have a RenderContent() method
}