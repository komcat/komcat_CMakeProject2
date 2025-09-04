#include "include/data/data_client_manager.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include "imgui.h"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

bool DataClientManager::s_instanceExists = false;

// Constructor
DataClientManager::DataClientManager(const std::string& configFilePath)
  : m_configFilePath(configFilePath),
  m_maxLogEntries(1000),
  m_autoSaveData(false),
  m_dataSaveInterval(60),
  m_showDebug(false)
{
  if (s_instanceExists) {
    throw std::runtime_error("DataClientManager instance already exists");
  }
  s_instanceExists = true;

  if (!LoadConfig()) {
    Logger::GetInstance()->LogError("Failed to load data server configuration: " + configFilePath);
  }
  else {
    Logger::GetInstance()->LogInfo("Data server configuration loaded: " + configFilePath);
  }
}

// Destructor
DataClientManager::~DataClientManager() {
  s_instanceExists = false;

  // Clear all subscribers before disconnecting
  m_subscribers.clear();
  m_allChannelSubscribers.clear();

  // Disconnect all clients
  for (auto& clientInfo : m_clients) {
    if (clientInfo.connected) {
      clientInfo.client->Disconnect();
    }
  }

  Logger::GetInstance()->LogInfo("DataClientManager shut down");
}

// Load configuration from file
bool DataClientManager::LoadConfig() {
  try {
    // Open the config file
    std::ifstream configFile(m_configFilePath);
    if (!configFile.is_open()) {
      return false;
    }

    // Parse JSON
    configFile >> m_config;

    // Clear existing clients
    m_clients.clear();

    // Load settings
    if (m_config.contains("Settings")) {
      auto& settings = m_config["Settings"];

      if (settings.contains("DefaultServerId")) {
        m_defaultServerId = settings["DefaultServerId"].get<std::string>();
      }

      if (settings.contains("MaxLogEntries")) {
        m_maxLogEntries = settings["MaxLogEntries"].get<int>();
      }

      if (settings.contains("LogDirectory")) {
        m_logDirectory = settings["LogDirectory"].get<std::string>();
      }

      if (settings.contains("AutoSaveData")) {
        m_autoSaveData = settings["AutoSaveData"].get<bool>();
      }

      if (settings.contains("DataSaveInterval")) {
        m_dataSaveInterval = settings["DataSaveInterval"].get<int>();
      }
    }

    // Load servers
    if (m_config.contains("Servers") && m_config["Servers"].is_array()) {
      for (auto& serverConfig : m_config["Servers"]) {
        ServerConfig config;

        config.id = serverConfig["Id"].get<std::string>();
        config.name = serverConfig["Name"].get<std::string>();
        config.host = serverConfig["Host"].get<std::string>();
        config.port = serverConfig["Port"].get<int>();
        config.unit = serverConfig["Unit"].get<std::string>();
        config.displayUnitSuffix = serverConfig["displayUnitSuffix"].get<bool>();
        config.description = serverConfig["Description"].get<std::string>();
        config.autoConnect = serverConfig["AutoConnect"].get<bool>();
        config.logData = serverConfig["LogData"].get<bool>();

        // Create a client info for this server
        m_clients.emplace_back(config);

        Logger::GetInstance()->LogInfo("Added data server: " + config.id + " (" + config.host + ":" + std::to_string(config.port) + ")");
      }
    }

    return true;
  }
  catch (const std::exception& e) {
    Logger::GetInstance()->LogError("Error parsing config file: " + std::string(e.what()));
    return false;
  }
}

// Save configuration to file
bool DataClientManager::SaveConfig() {
  try {
    // Open the config file for writing
    std::ofstream configFile(m_configFilePath);
    if (!configFile.is_open()) {
      return false;
    }

    // Write JSON with indentation
    configFile << std::setw(2) << m_config << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    Logger::GetInstance()->LogError("Error saving config file: " + std::string(e.what()));
    return false;
  }
}

// Get the number of clients
size_t DataClientManager::GetClientCount() const {
  return m_clients.size();
}

// Get a specific client info reference
DataClientInfo& DataClientManager::GetClientInfo(int index) {
  return m_clients.at(index);
}

// Get client info by server ID
DataClientInfo* DataClientManager::GetClientInfoById(const std::string& serverId) {
  for (auto& clientInfo : m_clients) {
    if (clientInfo.config.id == serverId) {
      return &clientInfo;
    }
  }
  return nullptr;
}

// Connect a specific client
bool DataClientManager::ConnectClient(int index) {
  if (index < 0 || index >= static_cast<int>(m_clients.size())) {
    return false;
  }

  DataClientInfo& info = m_clients[index];

  if (info.connected) {
    return true;
  }

  // Check for duplicate connections
  if (IsAlreadyConnectedTo(info.config.host, info.config.port)) {
    auto* existingClient = GetClientConnectedTo(info.config.host, info.config.port);

    std::string warningMsg = "Cannot connect " + info.config.id +
      " - already connected to " + info.config.host + ":" +
      std::to_string(info.config.port);

    if (existingClient) {
      warningMsg += " (existing client: " + existingClient->config.id + ")";
    }

    Logger::GetInstance()->LogWarning(warningMsg);

    snprintf(info.statusMessage, sizeof(info.statusMessage),
      "Duplicate connection blocked - %s already connected",
      existingClient ? existingClient->config.id.c_str() : "another client");

    return false;
  }

  Logger::GetInstance()->LogInfo("=== CONNECTING CLIENT ===");
  Logger::GetInstance()->LogInfo("Client ID: " + info.config.id);
  Logger::GetInstance()->LogInfo("Target: " + info.config.host + ":" + std::to_string(info.config.port));
  Logger::GetInstance()->LogInfo("Description: " + info.config.description);

  // Connect the client
  info.connected = info.client->Connect(info.config.host, info.config.port);

  if (info.connected) {
    snprintf(info.statusMessage, sizeof(info.statusMessage),
      "Connected to %s:%d",
      info.config.host.c_str(), info.config.port);

    // NOTIFY: Connection established
    NotifyConnectionChanged(info.config.id, true);

    Logger::GetInstance()->LogInfo("✅ Connected to: " + info.config.id);
  }
  else {
    snprintf(info.statusMessage, sizeof(info.statusMessage),
      "Failed to connect to %s:%d",
      info.config.host.c_str(), info.config.port);

    Logger::GetInstance()->LogWarning("❌ Failed to connect to: " + info.config.id);
  }

  Logger::GetInstance()->LogInfo("=== END CONNECTION ===");
  return info.connected;
}

// Connect client by server ID
bool DataClientManager::ConnectClientById(const std::string& serverId) {
  for (int i = 0; i < static_cast<int>(m_clients.size()); ++i) {
    if (m_clients[i].config.id == serverId) {
      return ConnectClient(i);
    }
  }
  return false;
}

// Disconnect a specific client
void DataClientManager::DisconnectClient(int index) {
  if (index < 0 || index >= static_cast<int>(m_clients.size())) {
    return;
  }

  DataClientInfo& info = m_clients[index];

  if (!info.connected) {
    return;
  }

  info.client->Disconnect();
  info.connected = false;

  // NOTIFY: Disconnected
  NotifyConnectionChanged(info.config.id, false);

  snprintf(info.statusMessage, sizeof(info.statusMessage),
    "Disconnected from %s:%d",
    info.config.host.c_str(), info.config.port);

  Logger::GetInstance()->LogInfo("Disconnected from data server: " + info.config.id);
}

// Disconnect client by server ID
void DataClientManager::DisconnectClientById(const std::string& serverId) {
  for (int i = 0; i < static_cast<int>(m_clients.size()); ++i) {
    if (m_clients[i].config.id == serverId) {
      DisconnectClient(i);
      return;
    }
  }
}

// Connect all clients with autoConnect set to true
void DataClientManager::ConnectAutoClients() {
  Logger::GetInstance()->LogInfo("Starting auto-connection with delays...");

  for (int i = 0; i < static_cast<int>(m_clients.size()); ++i) {
    if (m_clients[i].config.autoConnect) {
      Logger::GetInstance()->LogInfo("Connecting to: " + m_clients[i].config.id);

      if (ConnectClient(i)) {
        Logger::GetInstance()->LogInfo("✅ Connected to: " + m_clients[i].config.id);
      }
      else {
        Logger::GetInstance()->LogWarning("❌ Failed to connect to: " + m_clients[i].config.id);
      }

      // Add delay between connections (except for the last one)
      if (i < static_cast<int>(m_clients.size()) - 1) {
        Logger::GetInstance()->LogInfo("Waiting 1 second before next connection...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }
  }

  Logger::GetInstance()->LogInfo("Auto-connection completed");
}

// Update all clients
void DataClientManager::UpdateClients() {
  static int debugUpdateCounter = 0;
  debugUpdateCounter++;
  bool shouldDebugLog = (debugUpdateCounter % 120 == 0);

  if (m_showDebug && shouldDebugLog) {
    std::cout << "[DEBUG DataClientManager] UpdateClients called" << std::endl;
  }

  for (auto& info : m_clients) {
    // Check connection status
    if (info.connected && !info.client->IsConnected()) {
      info.connected = false;

      // NOTIFY: Connection lost
      NotifyConnectionChanged(info.config.id, false);

      snprintf(info.statusMessage, sizeof(info.statusMessage),
        "Connection lost to %s:%d",
        info.config.host.c_str(), info.config.port);

      Logger::GetInstance()->LogWarning("Connection lost to data server: " + info.config.id);
    }

    if (info.connected) {
      std::deque<float> newValues = info.client->GetReceivedValues();

      if (!newValues.empty()) {
        if (m_showDebug && shouldDebugLog) {
          std::cout << "[DEBUG DataClientManager] Received " << newValues.size()
            << " new values for " << info.config.id << std::endl;
        }

        std::lock_guard<std::mutex> lock(*info.dataMutex);

        for (float val : newValues) {
          DataPoint dataPoint(val);

          // Store in circular buffer
          info.dataPoints[info.dataPointCursor] = dataPoint;
          info.dataPointCursor = (info.dataPointCursor + 1) % 100;

          if (info.dataPointCount < 100) {
            info.dataPointCount++;
          }

          // Add to timestamp buffer
          info.timestampBuffer.push_back(dataPoint);

          if (info.timestampBuffer.size() > DataClientInfo::MAX_TIMESTAMP_BUFFER_SIZE) {
            info.timestampBuffer.pop_front();
          }

          info.latestValue = val;

          // Update GlobalDataStore
          GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
          if (globalStore) {
            globalStore->SetValue(info.config.id, val);

            if (m_showDebug && shouldDebugLog) {
              std::cout << "[DEBUG DataClientManager] Updated GlobalDataStore: "
                << info.config.id << " = " << val << std::endl;
            }
          }

          // NOTIFY: Data received
          NotifyDataReceived(info.config.id, val, dataPoint);
        }

        // Log data if configured
        if (info.config.logData && !newValues.empty()) {
          std::stringstream ss;
          SIValue siValue(info.latestValue, info.config.unit);

          ss << "Data from " << info.config.id << ": ";

          if (info.config.displayUnitSuffix) {
            ss << siValue.GetDisplayString(info.config.unit);
          }
          else {
            ss << siValue.ToString();
          }
        }
      }
      else {
        if (m_showDebug && shouldDebugLog) {
          std::cout << "[DEBUG DataClientManager] No new values for " << info.config.id << std::endl;
        }
      }
    }
    else {
      if (m_showDebug && shouldDebugLog) {
        std::cout << "[DEBUG DataClientManager] Client " << info.config.id << " not connected" << std::endl;
      }
    }
  }

  // Debug GlobalDataStore verification
  if (m_showDebug && shouldDebugLog) {
    GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
    if (globalStore) {
      auto allChannels = globalStore->GetAvailableChannels();
      std::cout << "[DEBUG DataClientManager] GlobalDataStore currently has "
        << allChannels.size() << " channels:" << std::endl;
      for (const auto& ch : allChannels) {
        float val = globalStore->GetValue(ch);
        std::cout << "[DEBUG DataClientManager]   " << ch << ": " << val << std::endl;
      }
    }
  }
}

// Format timestamp for display
std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
  auto time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm;

#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  localtime_r(&time, &tm);
#endif

  std::stringstream ss;
  ss << std::put_time(&tm, "%H:%M:%S");

  // Add milliseconds
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    tp.time_since_epoch() % std::chrono::seconds(1));
  ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  return ss.str();
}

// Render the UI for all clients (keeping your existing implementation)
void DataClientManager::RenderUI() {
  if (!m_isVisible) {
    return;
  }

  ImGui::Begin("Data Client Manager", &m_isVisible, ImGuiWindowFlags_NoScrollbar);

  // Header section
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::Text("📡 FNW Data Channels");
  ImGui::PopFont();

  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%zu servers configured)", m_clients.size());

  // Quick connection status bar
  int connectedCount = 0;
  for (const auto& client : m_clients) {
    if (client.connected) connectedCount++;
  }

  ImGui::Text("Connected: ");
  ImGui::SameLine();
  if (connectedCount > 0) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d/%zu", connectedCount, m_clients.size());
  }
  else {
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.0f, 1.0f), "%d/%zu", connectedCount, m_clients.size());
  }

  // Add subscriber count info
  size_t totalSubscribers = m_allChannelSubscribers.size();
  for (const auto& [channelId, subs] : m_subscribers) {
    totalSubscribers += subs.size();
  }

  if (totalSubscribers > 0) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.9f, 1.0f), " | %zu active subscribers", totalSubscribers);
  }

  ImGui::Separator();

  // Calculate card layout
  ImGuiStyle& style = ImGui::GetStyle();
  float windowWidth = ImGui::GetContentRegionAvail().x;
  float cardWidth = 320.0f;
  float cardSpacing = style.ItemSpacing.x;
  int cardsPerRow = static_cast<int>((windowWidth + cardSpacing) / (cardWidth + cardSpacing));
  cardsPerRow = (std::max)(1, cardsPerRow);

  // Scrollable area for cards
  ImGui::BeginChild("CardsScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  // Render cards in grid layout
  for (size_t i = 0; i < m_clients.size(); i++) {
    auto& info = m_clients[i];

    // Start new row if needed
    if (i > 0 && (i % cardsPerRow) != 0) {
      ImGui::SameLine();
    }

    // Card container
    ImGui::BeginGroup();

    // Card background
    ImVec2 cardStart = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Determine card color based on connection status
    ImU32 cardBgColor;
    ImU32 borderColor;

    if (info.connected) {
      cardBgColor = IM_COL32(40, 80, 40, 180);
      borderColor = IM_COL32(80, 160, 80, 255);
    }
    else {
      cardBgColor = IM_COL32(60, 60, 60, 180);
      borderColor = IM_COL32(100, 100, 100, 255);
    }

    // Check for conflicts
    auto conflicts = GetConnectionConflicts(info.config.id);
    if (!conflicts.empty()) {
      cardBgColor = IM_COL32(80, 60, 40, 180);
      borderColor = IM_COL32(200, 120, 60, 255);
    }

    // Reserve space for the card
    ImGui::Dummy(ImVec2(cardWidth, 160.0f));
    ImVec2 cardEnd = ImGui::GetItemRectMax();

    // Draw card background and border
    drawList->AddRectFilled(cardStart, cardEnd, cardBgColor, 8.0f);
    drawList->AddRect(cardStart, cardEnd, borderColor, 8.0f, 0, 2.0f);

    // Card content
    ImGui::SetCursorScreenPos(ImVec2(cardStart.x + 10, cardStart.y + 10));
    ImGui::BeginGroup();

    // Card header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

    // Status indicator
    if (info.connected) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "●");
    }
    else {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "●");
    }

    ImGui::SameLine();
    ImGui::Text("%s", info.config.name.c_str());

    // Show subscriber count for this channel
    size_t channelSubCount = GetSubscriberCount(info.config.id);
    if (channelSubCount > 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.9f, 1.0f), "(%zu)", channelSubCount);
    }

    ImGui::PopFont();

    // Server address
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::Text("%s:%d", info.config.host.c_str(), info.config.port);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Real-time value display
    if (info.connected) {
      std::lock_guard<std::mutex> lock(*info.dataMutex);

      // Large value display
      SIValue siValue(info.latestValue, info.config.unit);
      std::string valueText;

      if (info.config.displayUnitSuffix) {
        valueText = siValue.GetDisplayString(info.config.unit, 3);
      }
      else {
        valueText = siValue.ToString(3);
      }

      ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
      ImGui::SetWindowFontScale(1.4f);
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", valueText.c_str());
      ImGui::SetWindowFontScale(1.0f);
      ImGui::PopFont();

      // Data info
      if (info.dataPointCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("📊 %d samples", info.dataPointCount);
        ImGui::PopStyleColor();
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
        ImGui::Text("⏳ Waiting for data...");
        ImGui::PopStyleColor();
      }
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
      ImGui::Text("❌ Not connected");
      ImGui::PopStyleColor();

      ImGui::Text(" ");
      ImGui::Text(" ");
    }

    ImGui::Spacing();

    // Action buttons
    std::string buttonId = "##" + std::to_string(i);

    if (!info.connected) {
      bool canConnect = !IsAlreadyConnectedTo(info.config.host, info.config.port);

      if (!canConnect) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
      }

      if (ImGui::Button(("Connect" + buttonId).c_str(), ImVec2(80, 25)) && canConnect) {
        ConnectClient(static_cast<int>(i));
      }

      ImGui::PopStyleColor(3);

      if (!canConnect && ImGui::IsItemHovered()) {
        auto* existing = GetClientConnectedTo(info.config.host, info.config.port);
        ImGui::SetTooltip("Blocked: %s already connected",
          existing ? existing->config.id.c_str() : "another client");
      }
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.0f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.0f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.0f, 1.0f));

      if (ImGui::Button(("Disconnect" + buttonId).c_str(), ImVec2(80, 25))) {
        DisconnectClient(static_cast<int>(i));
      }

      ImGui::PopStyleColor(3);
    }

    // Info button
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.6f, 1.0f));

    if (ImGui::Button(("ℹ" + buttonId).c_str(), ImVec2(25, 25))) {
      // Toggle detailed info
    }

    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("ID: %s", info.config.id.c_str());
      ImGui::Text("Description: %s", info.config.description.c_str());
      ImGui::Text("Unit: %s", info.config.unit.c_str());
      ImGui::Text("Auto-connect: %s", info.config.autoConnect ? "Yes" : "No");
      ImGui::Text("Log data: %s", info.config.logData ? "Yes" : "No");

      // Show subscribers
      auto channelSubs = GetChannelSubscribers(info.config.id);
      if (!channelSubs.empty()) {
        ImGui::Separator();
        ImGui::Text("Subscribers:");
        for (auto* sub : channelSubs) {
          ImGui::Text("  • %s", sub->GetSubscriberName().c_str());
        }
      }

      if (!conflicts.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Conflicts with:");
        for (const auto& conflict : conflicts) {
          ImGui::Text("  • %s", conflict.c_str());
        }
      }
      ImGui::EndTooltip();
    }

    ImGui::EndGroup();
    ImGui::EndGroup();

    if ((i + 1) % cardsPerRow == 0) {
      ImGui::Spacing();
    }
  }

  ImGui::EndChild();

  // Footer
  ImGui::Separator();

  if (ImGui::Button("🔗 Connect All")) {
    ConnectAutoClients();
  }

  ImGui::SameLine();
  if (ImGui::Button("🔌 Disconnect All")) {
    for (int i = 0; i < static_cast<int>(m_clients.size()); ++i) {
      DisconnectClient(i);
    }
  }

  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
    "Default: %s", m_defaultServerId.c_str());

  ImGui::End();
}

// Check if already connected to same host:port
bool DataClientManager::IsAlreadyConnectedTo(const std::string& host, int port) const {
  for (const auto& clientInfo : m_clients) {
    if (clientInfo.connected &&
      clientInfo.config.host == host &&
      clientInfo.config.port == port) {
      return true;
    }
  }
  return false;
}

// Get client connected to specific host:port
DataClientInfo* DataClientManager::GetClientConnectedTo(const std::string& host, int port) {
  for (auto& clientInfo : m_clients) {
    if (clientInfo.connected &&
      clientInfo.config.host == host &&
      clientInfo.config.port == port) {
      return &clientInfo;
    }
  }
  return nullptr;
}

// Check for duplicate connections
bool DataClientManager::HasDuplicateConnection(const std::string& serverId) const {
  auto* targetClient = const_cast<DataClientManager*>(this)->GetClientInfoById(serverId);
  if (!targetClient) return false;

  return IsAlreadyConnectedTo(targetClient->config.host, targetClient->config.port);
}

// Get connection conflicts
std::vector<std::string> DataClientManager::GetConnectionConflicts(const std::string& serverId) const {
  std::vector<std::string> conflicts;
  auto* targetClient = const_cast<DataClientManager*>(this)->GetClientInfoById(serverId);
  if (!targetClient) return conflicts;

  for (const auto& clientInfo : m_clients) {
    if (clientInfo.config.id != serverId &&
      clientInfo.connected &&
      clientInfo.config.host == targetClient->config.host &&
      clientInfo.config.port == targetClient->config.port) {
      conflicts.push_back(clientInfo.config.id);
    }
  }
  return conflicts;
}

// Subscription management
void DataClientManager::Subscribe(const std::string& channelId, IDataSubscriber* subscriber) {
  if (!subscriber) return;

  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  m_subscribers[channelId].insert(subscriber);

  Logger::GetInstance()->LogInfo(
    "Subscriber '" + subscriber->GetSubscriberName() +
    "' subscribed to channel: " + channelId
  );

  // Send immediate status update
  auto* clientInfo = GetClientInfoById(channelId);
  if (clientInfo) {
    subscriber->OnConnectionChanged(channelId, clientInfo->connected);

    if (clientInfo->connected) {
      std::lock_guard<std::mutex> dataLock(*clientInfo->dataMutex);
      if (clientInfo->dataPointCount > 0) {
        auto lastPoint = clientInfo->dataPoints[(clientInfo->dataPointCursor - 1 + 100) % 100];
        subscriber->OnDataReceived(channelId, clientInfo->latestValue, lastPoint);
      }
    }
  }
}

void DataClientManager::SubscribeToAll(IDataSubscriber* subscriber) {
  if (!subscriber) return;

  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  m_allChannelSubscribers.insert(subscriber);

  Logger::GetInstance()->LogInfo(
    "Subscriber '" + subscriber->GetSubscriberName() +
    "' subscribed to ALL channels"
  );

  for (const auto& client : m_clients) {
    subscriber->OnConnectionChanged(client.config.id, client.connected);

    if (client.connected && client.dataPointCount > 0) {
      std::lock_guard<std::mutex> dataLock(*client.dataMutex);
      auto lastPoint = client.dataPoints[(client.dataPointCursor - 1 + 100) % 100];
      subscriber->OnDataReceived(client.config.id, client.latestValue, lastPoint);
    }
  }
}

void DataClientManager::Unsubscribe(const std::string& channelId, IDataSubscriber* subscriber) {
  if (!subscriber) return;

  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    it->second.erase(subscriber);
    if (it->second.empty()) {
      m_subscribers.erase(it);
    }
  }

  Logger::GetInstance()->LogInfo(
    "Subscriber '" + subscriber->GetSubscriberName() +
    "' unsubscribed from channel: " + channelId
  );
}

void DataClientManager::UnsubscribeFromAll(IDataSubscriber* subscriber) {
  if (!subscriber) return;

  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  m_allChannelSubscribers.erase(subscriber);
}

void DataClientManager::UnsubscribeCompletely(IDataSubscriber* subscriber) {
  if (!subscriber) return;

  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  m_allChannelSubscribers.erase(subscriber);

  for (auto& [channelId, subscribers] : m_subscribers) {
    subscribers.erase(subscriber);
  }

  std::erase_if(m_subscribers, [](const auto& pair) {
    return pair.second.empty();
  });

  Logger::GetInstance()->LogInfo(
    "Subscriber '" + subscriber->GetSubscriberName() +
    "' completely unsubscribed"
  );
}

// Get subscription info
std::vector<std::string> DataClientManager::GetSubscribedChannels(IDataSubscriber* subscriber) const {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  std::vector<std::string> channels;

  for (const auto& [channelId, subscribers] : m_subscribers) {
    if (subscribers.find(subscriber) != subscribers.end()) {
      channels.push_back(channelId);
    }
  }

  return channels;
}

std::set<IDataSubscriber*> DataClientManager::GetChannelSubscribers(const std::string& channelId) const {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    return it->second;
  }

  return std::set<IDataSubscriber*>();
}

size_t DataClientManager::GetSubscriberCount(const std::string& channelId) const {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  size_t count = m_allChannelSubscribers.size();

  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    count += it->second.size();
  }

  return count;
}

// Internal notification methods
void DataClientManager::NotifyDataReceived(const std::string& channelId,
  float value,
  const DataPoint& dataPoint) {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    for (auto* subscriber : it->second) {
      try {
        subscriber->OnDataReceived(channelId, value, dataPoint);
      }
      catch (const std::exception& e) {
        Logger::GetInstance()->LogError(
          "Exception in subscriber '" + subscriber->GetSubscriberName() +
          "': " + e.what()
        );
      }
    }
  }

  for (auto* subscriber : m_allChannelSubscribers) {
    try {
      subscriber->OnDataReceived(channelId, value, dataPoint);
    }
    catch (const std::exception& e) {
      Logger::GetInstance()->LogError(
        "Exception in all-channel subscriber '" +
        subscriber->GetSubscriberName() + "': " + e.what()
      );
    }
  }
}

void DataClientManager::NotifyConnectionChanged(const std::string& channelId, bool connected) {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    for (auto* subscriber : it->second) {
      try {
        subscriber->OnConnectionChanged(channelId, connected);
      }
      catch (const std::exception& e) {
        Logger::GetInstance()->LogError(
          "Exception in subscriber connection change: " + std::string(e.what())
        );
      }
    }
  }

  for (auto* subscriber : m_allChannelSubscribers) {
    try {
      subscriber->OnConnectionChanged(channelId, connected);
    }
    catch (const std::exception& e) {
      Logger::GetInstance()->LogError(
        "Exception in all-channel subscriber connection change: " + std::string(e.what())
      );
    }
  }
}

void DataClientManager::NotifyDataError(const std::string& channelId, const std::string& errorMessage) {
  std::lock_guard<std::mutex> lock(m_subscriberMutex);

  if (auto it = m_subscribers.find(channelId); it != m_subscribers.end()) {
    for (auto* subscriber : it->second) {
      try {
        subscriber->OnDataError(channelId, errorMessage);
      }
      catch (const std::exception& e) {
        Logger::GetInstance()->LogError(
          "Exception in subscriber data error: " + std::string(e.what())
        );
      }
    }
  }

  for (auto* subscriber : m_allChannelSubscribers) {
    try {
      subscriber->OnDataError(channelId, errorMessage);
    }
    catch (const std::exception& e) {
      Logger::GetInstance()->LogError(
        "Exception in all-channel subscriber data error: " + std::string(e.what())
      );
    }
  }
}