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


// Constructor
DataClientManager::DataClientManager(const std::string& configFilePath)
  : m_configFilePath(configFilePath),
  m_maxLogEntries(1000),
  m_autoSaveData(false),
  m_dataSaveInterval(60),
  m_showDebug(false) // Add this line
{
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

  // Skip if already connected
  if (info.connected) {
    return true;
  }

  // Connect the client
  info.connected = info.client->Connect(info.config.host, info.config.port);

  // Update status message
  if (info.connected) {
    snprintf(info.statusMessage, sizeof(info.statusMessage),
      "Connected to %s:%d",
      info.config.host.c_str(), info.config.port);
    Logger::GetInstance()->LogInfo("Connected to data server: " + info.config.id);
  }
  else {
    snprintf(info.statusMessage, sizeof(info.statusMessage),
      "Failed to connect to %s:%d",
      info.config.host.c_str(), info.config.port);
    Logger::GetInstance()->LogWarning("Failed to connect to data server: " + info.config.id);
  }

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
void DataClientManager::ConnectAutoClients() {
  for (int i = 0; i < static_cast<int>(m_clients.size()); ++i) {
    if (m_clients[i].config.autoConnect) {
      ConnectClient(i);
    }
  }
}

// Update all clients

// Replace your UpdateClients method with this version:
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

          // CRITICAL: Update the global data store with the latest value
          GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
          if (globalStore) {
            globalStore->SetValue(info.config.id, val);

            if (m_showDebug && shouldDebugLog && info.config.id == "GPIB-Current") {
              std::cout << "[DEBUG DataClientManager] Updated GlobalDataStore: "
                << info.config.id << " = " << val << std::endl;

              // Verify the value was stored
              float storedValue = globalStore->GetValue(info.config.id, -999.0f);
              std::cout << "[DEBUG DataClientManager] Verification read: " << storedValue << std::endl;

              // Check if the channel is in the available channels list
              auto channels = globalStore->GetAvailableChannels();
              bool found = std::find(channels.begin(), channels.end(), info.config.id) != channels.end();
              std::cout << "[DEBUG DataClientManager] Channel '" << info.config.id
                << "' found in available channels: " << found << std::endl;
            }
          }
          else {
            if (m_showDebug && shouldDebugLog) {
              std::cout << "[DEBUG DataClientManager] ERROR: GlobalDataStore is NULL!" << std::endl;
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
        if (m_showDebug && shouldDebugLog && info.config.id == "GPIB-Current") {
          std::cout << "[DEBUG DataClientManager] No new values for " << info.config.id << std::endl;
        }
      }
    }
    else {
      if (m_showDebug && shouldDebugLog && info.config.id == "GPIB-Current") {
        std::cout << "[DEBUG DataClientManager] Client " << info.config.id << " not connected" << std::endl;
      }
    }
  }

  // Additional debug: Periodic GlobalDataStore verification
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
void DataClientManager::RenderUI() {
  // Skip rendering if not visible
  if (!m_isVisible) {
    return;
  }


  ImGui::Begin("Data Client Manager");

  // Top-level info
  ImGui::Text("Loaded %zu data servers from configuration", m_clients.size());
  ImGui::Text("Default server: %s", m_defaultServerId.c_str());

  ImGui::Separator();

  // Render each client in a collapsible header
  for (size_t i = 0; i < m_clients.size(); i++) {
    auto& info = m_clients[i];

    // Create header with client name and ID
    std::string headerLabel = info.config.name + " (" + info.config.id + ")";

    if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      // Client-specific identifier for ImGui widgets
      std::string idPrefix = "##" + std::to_string(i);

      // Display server info
      ImGui::Text("Server: %s:%d", info.config.host.c_str(), info.config.port);
      ImGui::Text("Description: %s", info.config.description.c_str());
      ImGui::Text("Unit: %s (Display suffix: %s)",
        info.config.unit.c_str(),
        info.config.displayUnitSuffix ? "Yes" : "No");

      ImGui::Separator();

      // Connect / Disconnect button
      if (!info.connected) {
        if (ImGui::Button(("Connect" + idPrefix).c_str())) {
          ConnectClient(static_cast<int>(i));
        }
      }
      else {
        if (ImGui::Button(("Disconnect" + idPrefix).c_str())) {
          DisconnectClient(static_cast<int>(i));
        }
      }

      ImGui::SameLine();

      // Display connection status
      ImGui::Text("Status: %s", info.statusMessage);

      // Show received data if connected
      if (info.connected) {
        ImGui::Separator();

        // Get a lock to safely access the data
        std::lock_guard<std::mutex> lock(*info.dataMutex);

        // Display latest received value with unit and SI prefix
        SIValue siValue(info.latestValue, info.config.unit);
        std::string valueLabel;

        if (info.config.displayUnitSuffix) {
          valueLabel = "Latest value: " + siValue.GetDisplayString(info.config.unit);
        }
        else {
          valueLabel = "Latest value: " + siValue.ToString();
        }

        // Display with larger font
        float originalFontSize = ImGui::GetFontSize();
        ImGui::SetWindowFontScale(1.5f); // Scale up font for this text
        ImGui::Text("%s", valueLabel.c_str());
        ImGui::SetWindowFontScale(1.0f); // Reset font scale

        // Debug info
        ImGui::Text("Data points in buffer: %d", info.dataPointCount);

        if (info.dataPointCount > 0) {
          // Calculate min/max for better scaling
          float minValue = info.dataPoints[0].value;
          float maxValue = info.dataPoints[0].value;

          // Find actual min/max
          for (int j = 0; j < info.dataPointCount; j++) {
            minValue = (std::min)(minValue, info.dataPoints[j].value);
            maxValue = (std::max)(maxValue, info.dataPoints[j].value);
          }

          // Use improved chart scaling
          ChartScale chartScale(minValue, maxValue);
          minValue = chartScale.min;
          maxValue = chartScale.max;

          // Create plot values array - need to copy from circular buffer to linear array
          float plotValues[100];
          for (int j = 0; j < info.dataPointCount; j++) {
            int idx = (info.dataPointCursor - info.dataPointCount + j + 100) % 100;
            plotValues[j] = info.dataPoints[idx].value;
          }

          // Plot the values
          ImGui::PlotLines(("##values" + idPrefix).c_str(),
            plotValues,              // Array
            info.dataPointCount,     // Count
            0,                       // Offset
            nullptr,                 // Overlay text
            minValue,                // Y-min
            maxValue,                // Y-max
            ImVec2(0, 80),           // Graph size
            sizeof(float));          // Stride

          // Display min/max with SI prefixes
          SIValue minSI(minValue, info.config.unit);
          SIValue maxSI(maxValue, info.config.unit);

          if (info.config.displayUnitSuffix) {
            ImGui::Text("Min: %s, Max: %s",
              minSI.GetDisplayString(info.config.unit, 4).c_str(),
              maxSI.GetDisplayString(info.config.unit, 4).c_str());
          }
          else {
            ImGui::Text("Min: %s, Max: %s",
              minSI.ToString(4).c_str(),
              maxSI.ToString(4).c_str());
          }

          // Display latest data points with timestamps in a table
          if (ImGui::TreeNode(("Recent Data Points" + idPrefix).c_str())) {
            ImGui::BeginTable("dataTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);

            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Timestamp");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            // Display the 10 most recent data points (or less if we don't have that many)
            int numToShow = (std::min)(10, info.dataPointCount);

            for (int j = 0; j < numToShow; j++) {
              ImGui::TableNextRow();

              // Calculate the index in the circular buffer
              int idx = (info.dataPointCursor - j - 1 + 100) % 100;

              // Index column
              ImGui::TableNextColumn();
              ImGui::Text("%d", info.dataPointCount - j);

              // Timestamp column
              ImGui::TableNextColumn();
              ImGui::Text("%s", FormatTimestamp(info.dataPoints[idx].timestamp).c_str());

              // Value column with SI prefix
              ImGui::TableNextColumn();
              SIValue rowValue(info.dataPoints[idx].value, info.config.unit);
              if (info.config.displayUnitSuffix) {
                ImGui::Text("%s", rowValue.GetDisplayString(info.config.unit).c_str());
              }
              else {
                ImGui::Text("%s", rowValue.ToString().c_str());
              }
            }

            ImGui::EndTable();
            ImGui::TreePop();
          }
        }
      }

      ImGui::Separator();
    }
  }

  ImGui::End();
}