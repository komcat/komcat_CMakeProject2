// TCPDataManagerUI.cpp
#include "TCPDataManagerUI.h"
#include "imgui.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

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

  // Load the DataServerConfig.json to get proper names and descriptions
  LoadServerConfig();

  std::cout << "TCPDataManagerUI initialized with external DataClientManager" << std::endl;
  return true;
}

void TCPDataManagerUI::LoadServerConfig() {
  try {
    std::ifstream file("DataServerConfig.json");
    if (!file.is_open()) {
      std::cerr << "TCPDataManagerUI: Could not open DataServerConfig.json" << std::endl;
      return;
    }

    nlohmann::json config;
    file >> config;

    m_serverConfigs.clear();

    if (config.contains("Servers") && config["Servers"].is_array()) {
      for (const auto& server : config["Servers"]) {
        ServerDisplayInfo info;
        info.id = server.value("Id", "");
        info.name = server.value("Name", "Unknown Server");
        info.description = server.value("Description", "No description available");
        info.host = server.value("Host", "");
        info.port = server.value("Port", 0);
        info.autoConnect = server.value("AutoConnect", false);
        info.unit = server.value("Unit", "");
        info.displayUnitSuffix = server.value("displayUnitSuffix", false);
        info.logData = server.value("LogData", false);

        m_serverConfigs[info.id] = info;
      }
    }

    std::cout << "TCPDataManagerUI: Loaded " << m_serverConfigs.size() << " server configurations" << std::endl;
  }
  catch (const std::exception& e) {
    std::cerr << "TCPDataManagerUI: Error loading DataServerConfig.json: " << e.what() << std::endl;
  }
}

void TCPDataManagerUI::Update() {
  if (!m_isInitialized || !m_dataClientManager) {
    return;
  }

  // Update the data client manager
  m_dataClientManager->UpdateClients();
}

void TCPDataManagerUI::RenderServerCard(const ServerDisplayInfo& serverInfo, int clientIndex) {
  // Get the actual client info from DataClientManager
  auto& clientInfo = m_dataClientManager->GetClientInfo(clientIndex);

  // Create a styled card for each server
  ImGui::PushID(clientIndex);

  // Card background
  ImVec2 cardSize(0, 120); // Auto width, fixed height
  ImGui::BeginChild("ServerCard", cardSize, true, ImGuiWindowFlags_NoScrollbar);

  // Header with server name
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("%s", serverInfo.name.c_str());
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();

  // Connection status indicator
  ImGui::SameLine();
  if (clientInfo.connected) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● Connected");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● Disconnected");
  }

  ImGui::Separator();

  // Description
  ImGui::TextWrapped("%s", serverInfo.description.c_str());

  ImGui::Spacing();

  // Connection details
  ImGui::Text("Address: %s:%d", serverInfo.host.c_str(), serverInfo.port);

  // Show unit if available
  if (!serverInfo.unit.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "(%s)", serverInfo.unit.c_str());
  }

  ImGui::Spacing();

  // Action buttons
  if (clientInfo.connected) {
    if (ImGui::Button("Disconnect")) {
      m_dataClientManager->DisconnectClient(clientIndex);
    }
  }
  else {
    if (ImGui::Button("Connect")) {
      m_dataClientManager->ConnectClient(clientIndex);
    }
  }

  // Show auto-connect status
  if (serverInfo.autoConnect) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "[Auto]");
  }

  // Show data logging status
  if (serverInfo.logData) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Log]");
  }

  ImGui::EndChild();
  ImGui::PopID();
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

  ImGui::SameLine();
  if (ImGui::Button("Reload Config")) {
    LoadServerConfig();
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Custom server cards display
  if (clientCount > 0) {
    ImGui::Text("Server Channels:");
    ImGui::Spacing();

    // Display server cards in a grid layout (2 columns)
    int columns = 2;
    float cardWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (columns - 1)) / columns;

    // Convert map to vector for index-based access
    std::vector<ServerDisplayInfo> serverConfigs;
    for (const auto& [id, config] : m_serverConfigs) {
      serverConfigs.push_back(config);
    }

    for (size_t i = 0; i < clientCount; ++i) {
      auto& clientInfo = m_dataClientManager->GetClientInfo(static_cast<int>(i));

      // Get server config by index position (assumes same order as JSON)
      ServerDisplayInfo serverInfo;
      if (i < serverConfigs.size()) {
        serverInfo = serverConfigs[i];
      }
      else {
        // Fallback if no config found for this index
        serverInfo.id = "server_" + std::to_string(i);
        serverInfo.name = "Server " + std::to_string(i + 1);
        serverInfo.description = "Configuration not found in DataServerConfig.json";
        serverInfo.host = "Unknown";
        serverInfo.port = 0;
        serverInfo.autoConnect = false;
        serverInfo.logData = false;
      }

      // Column layout
      if (i > 0 && i % columns != 0) {
        ImGui::SameLine();
      }

      ImGui::BeginGroup();
      ImGui::PushItemWidth(cardWidth);

      RenderServerCard(serverInfo, static_cast<int>(i));

      ImGui::PopItemWidth();
      ImGui::EndGroup();
    }
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No servers configured in DataServerConfig.json");
  }
}