#include "include/data/data_client_manager.h"
#include "include/data/global_data_store.h" // Add this line
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

bool DataClientManager::s_instanceExists = false;

// Constructor
DataClientManager::DataClientManager(const std::string& configFilePath)
  : m_configFilePath(configFilePath),
  m_maxLogEntries(1000),
  m_autoSaveData(false),
  m_dataSaveInterval(60),
  m_showDebug(false) // Add this line
{
    if (s_instanceExists) {
        throw std::runtime_error("DataClientManager instance already exists");
    }
    s_instanceExists = true;
  // Load configuration
  if (!LoadConfig()) {
    Logger::GetInstance()->LogError("Failed to load data server configuration: " + configFilePath);
  }
  else {
    Logger::GetInstance()->LogInfo("Data server configuration loaded: " + configFilePath);

    // Connect to auto-connect clients
    ConnectAutoClients();
  }
}

// Destructor
DataClientManager::~DataClientManager() {

    s_instanceExists = false;
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

// Enhanced ConnectClient method with duplicate prevention
bool DataClientManager::ConnectClient(int index) {
    if (index < 0 || index >= static_cast<int>(m_clients.size())) {
        return false;
    }

    DataClientInfo& info = m_clients[index];

    // Skip if already connected
    if (info.connected) {
        return true;
    }

    // NEW: Check for duplicate connections
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

    // Log connection attempt with detailed info
    Logger::GetInstance()->LogInfo("=== CONNECTING CLIENT ===");
    Logger::GetInstance()->LogInfo("Client ID: " + info.config.id);
    Logger::GetInstance()->LogInfo("Target: " + info.config.host + ":" + std::to_string(info.config.port));
    Logger::GetInstance()->LogInfo("Description: " + info.config.description);

    // Connect the client
    info.connected = info.client->Connect(info.config.host, info.config.port);

    // Update status message
    if (info.connected) {
        snprintf(info.statusMessage, sizeof(info.statusMessage),
            "Connected to %s:%d",
            info.config.host.c_str(), info.config.port);
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

  // Skip if not connected
  if (!info.connected) {
    return;
  }

  // Disconnect the client
  info.client->Disconnect();
  info.connected = false;

  // Update status message
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
// OPTION 1: Modify ConnectAutoClients() to add delays between connections
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

// FIXED VERSION: Replace your UpdateClients method with this version:
void DataClientManager::UpdateClients() {
    // Debug counter to limit logging frequency
    static int debugUpdateCounter = 0;
    debugUpdateCounter++;
    bool shouldDebugLog = (debugUpdateCounter % 120 == 0); // Log every 2 seconds at 60fps

    if (m_showDebug && shouldDebugLog) {
        std::cout << "[DEBUG DataClientManager] UpdateClients called" << std::endl;
    }

    // Update all clients
    for (auto& info : m_clients) {
        // Check connection status
        if (info.connected && !info.client->IsConnected()) {
            info.connected = false;
            snprintf(info.statusMessage, sizeof(info.statusMessage),
                "Connection lost to %s:%d",
                info.config.host.c_str(), info.config.port);

            Logger::GetInstance()->LogWarning("Connection lost to data server: " + info.config.id);
        }

        // If connected, update received values
        if (info.connected) {
            // Get all new values since last frame
            std::deque<float> newValues = info.client->GetReceivedValues();

            // Update the circular buffer with new values
            if (!newValues.empty()) {
                if (m_showDebug && shouldDebugLog) {
                    std::cout << "[DEBUG DataClientManager] Received " << newValues.size()
                        << " new values for " << info.config.id << std::endl;
                }

                std::lock_guard<std::mutex> lock(*info.dataMutex);

                for (float val : newValues) {
                    // Create a new data point with the current timestamp
                    DataPoint dataPoint(val);

                    // Store in the circular buffer
                    info.dataPoints[info.dataPointCursor] = dataPoint;
                    info.dataPointCursor = (info.dataPointCursor + 1) % 100;

                    if (info.dataPointCount < 100) {
                        info.dataPointCount++;
                    }

                    // Update latest value
                    info.latestValue = val;

                    // CRITICAL FIX: Update the global data store with the latest value FOR ALL CHANNELS
                    GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
                    if (globalStore) {
                        globalStore->SetValue(info.config.id, val);

                        // ENHANCED DEBUG: Log for ALL channels periodically, not just GPIB-Current
                        if (m_showDebug && shouldDebugLog) {
                            std::cout << "[DEBUG DataClientManager] Updated GlobalDataStore: "
                                << info.config.id << " = " << val << std::endl;

                            // Verify the value was stored
                            float storedValue = globalStore->GetValue(info.config.id, -999.0f);
                            std::cout << "[DEBUG DataClientManager] Verification read for "
                                << info.config.id << ": " << storedValue << std::endl;
                        }
                    }
                    else {
                        if (m_showDebug && shouldDebugLog) {
                            std::cout << "[DEBUG DataClientManager] ERROR: GlobalDataStore is NULL for channel: "
                                << info.config.id << std::endl;
                        }
                    }
                }

                // Log data if configured
                if (info.config.logData && !newValues.empty()) {
                    // Just log the latest value to avoid spamming the log
                    std::stringstream ss;
                    SIValue siValue(info.latestValue, info.config.unit);

                    ss << "Data from " << info.config.id << ": ";

                    if (info.config.displayUnitSuffix) {
                        ss << siValue.GetDisplayString(info.config.unit);
                    }
                    else {
                        ss << siValue.ToString();
                    }

                    //TODO debug verbose
                    //Logger::GetInstance()->LogInfo(ss.str());
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

    // ENHANCED DEBUG: Periodic GlobalDataStore verification for ALL channels
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

// Render the UI for all clients
// Enhanced Card Style RenderUI for DataClientManager
void DataClientManager::RenderUI() {
    if (!m_isVisible) {
        return;
    }

    ImGui::Begin("Data Client Manager", &m_isVisible, ImGuiWindowFlags_NoScrollbar);

    // Header section
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font for header
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

    // Check for duplicates
    std::map<std::string, std::vector<std::string>> duplicateGroups;
    for (const auto& client : m_clients) {
        std::string hostPort = client.config.host + ":" + std::to_string(client.config.port);
        duplicateGroups[hostPort].push_back(client.config.id);
    }

    bool hasDuplicates = false;
    for (const auto& group : duplicateGroups) {
        if (group.second.size() > 1) {
            hasDuplicates = true;
            break;
        }
    }

    if (hasDuplicates) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), " ⚠ Duplicates detected");
    }

    ImGui::Separator();

    // Calculate card layout
    ImGuiStyle& style = ImGui::GetStyle();
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = 320.0f;  // Fixed card width
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
            cardBgColor = IM_COL32(40, 80, 40, 180);    // Dark green background
            borderColor = IM_COL32(80, 160, 80, 255);   // Green border
        }
        else {
            cardBgColor = IM_COL32(60, 60, 60, 180);    // Dark gray background
            borderColor = IM_COL32(100, 100, 100, 255); // Gray border
        }

        // Check for conflicts
        auto conflicts = GetConnectionConflicts(info.config.id);
        if (!conflicts.empty()) {
            cardBgColor = IM_COL32(80, 60, 40, 180);    // Orange background for conflicts
            borderColor = IM_COL32(200, 120, 60, 255);  // Orange border
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

        // Card header - Channel name and status indicator
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

        // Conflict warning
        if (!conflicts.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "⚠");
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

            // Value with larger font
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
            // Not connected - show connection button and status
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
            ImGui::Text("❌ Not connected");
            ImGui::PopStyleColor();

            ImGui::Text(" ");  // Spacing
            ImGui::Text(" ");  // Spacing
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
            // Toggle detailed info or show tooltip
        }

        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("ID: %s", info.config.id.c_str());
            ImGui::Text("Description: %s", info.config.description.c_str());
            ImGui::Text("Unit: %s", info.config.unit.c_str());
            ImGui::Text("Auto-connect: %s", info.config.autoConnect ? "Yes" : "No");
            ImGui::Text("Log data: %s", info.config.logData ? "Yes" : "No");
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

        // Add some spacing between cards
        if ((i + 1) % cardsPerRow == 0) {
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();

    // Footer with global actions
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