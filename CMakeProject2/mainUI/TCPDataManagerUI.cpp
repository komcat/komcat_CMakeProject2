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

  // Card styling - make it more card-like with rounded corners and shadow effect
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

  // Card colors based on connection status
  ImVec4 cardBgColor;
  ImVec4 borderColor;
  if (clientInfo.connected) {
    cardBgColor = ImVec4(0.15f, 0.25f, 0.15f, 0.9f);  // Dark green tint
    borderColor = ImVec4(0.0f, 0.8f, 0.0f, 0.6f);     // Green border
  }
  else {
    cardBgColor = ImVec4(0.25f, 0.15f, 0.15f, 0.9f);  // Dark red tint
    borderColor = ImVec4(0.8f, 0.0f, 0.0f, 0.6f);     // Red border
  }

  ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBgColor);
  ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

  // Card dimensions
  ImVec2 cardSize(320, 300); // Fixed size for consistent card layout
  ImGui::BeginChild("ServerCard", cardSize, true, ImGuiWindowFlags_NoScrollbar);

  // === CARD HEADER ===
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::SetWindowFontScale(1.3f);

  // Channel icon + name
  const char* icon = clientInfo.connected ? "📡" : "📴";
  ImGui::Text("%s %s", icon, serverInfo.name.c_str());
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();

  // Connection status badge
  ImGui::SameLine(ImGui::GetWindowWidth() - 90);
  if (clientInfo.connected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Button("ONLINE", ImVec2(80, 0));
    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.0f, 0.0f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Button("OFFLINE", ImVec2(80, 0));
    ImGui::PopStyleColor(3);
  }

  ImGui::Spacing();

  // Separator line
  ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();

  // === CARD BODY ===

  // Description with icon
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
  ImGui::Text("📋 Description:");
  ImGui::PopStyleColor();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
  ImGui::TextWrapped("   %s", serverInfo.description.c_str());
  ImGui::PopStyleColor();

  ImGui::Spacing();

  // Connection details with icon
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
  ImGui::Text("🌐 Address:");
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
  ImGui::Text("%s:%d", serverInfo.host.c_str(), serverInfo.port);
  ImGui::PopStyleColor();

  // Show unit if available
  if (!serverInfo.unit.empty()) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
    ImGui::Text("(%s)", serverInfo.unit.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Spacing();

  // Status indicators row
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
  ImGui::Text("⚙️  Status:");
  ImGui::PopStyleColor();

  ImGui::SameLine();

  // Auto-connect badge
  if (serverInfo.autoConnect) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.8f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::SmallButton("AUTO");
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
  }

  // Data logging badge
  if (serverInfo.logData) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::SmallButton("LOG");
    ImGui::PopStyleColor(2);
  }

  ImGui::Spacing();

  // === CARD FOOTER - Action Buttons ===
  ImGui::Separator();
  ImGui::Spacing();

  // Center the buttons
  float buttonWidth = 100.0f;
  float availWidth = ImGui::GetContentRegionAvail().x;
  float offset = (availWidth - buttonWidth) * 0.5f;
  if (offset > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

  if (clientInfo.connected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Disconnect", ImVec2(buttonWidth, 0))) {
      m_dataClientManager->DisconnectClient(clientIndex);
    }
    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
    if (ImGui::Button("Connect", ImVec2(buttonWidth, 0))) {
      m_dataClientManager->ConnectClient(clientIndex);
    }
    ImGui::PopStyleColor(3);
  }

  ImGui::EndChild();

  // Pop all style modifications
  ImGui::PopStyleColor(2); // ChildBg, Border
  ImGui::PopStyleVar(2);   // Rounding, BorderSize
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

    // Display server cards in a responsive grid layout
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = 320.0f;
    float cardSpacing = ImGui::GetStyle().ItemSpacing.x;
    int columns = (std::max)(1, (int)((availableWidth + cardSpacing) / (cardWidth + cardSpacing)));

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

      // Column layout with proper spacing
      if (i > 0 && i % columns != 0) {
        ImGui::SameLine();
      }

      RenderServerCard(serverInfo, static_cast<int>(i));

      // Add extra spacing between rows
      if ((i + 1) % columns == 0 && i < clientCount - 1) {
        ImGui::Spacing();
      }
    }
  }
  else {
    // No servers message with styling
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("📋 No TCP Data Servers");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Text("• Check that DataServerConfig.json exists and contains server configurations");
    ImGui::Text("• Ensure the DataClientManager is properly initialized");
    ImGui::Text("• Click 'Reload Config' to refresh the configuration");
    ImGui::PopStyleColor();
  }
}