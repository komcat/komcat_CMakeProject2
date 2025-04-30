#include "include/data/data_client_manager.h"
#include "include/logger.h"
#include "imgui.h"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>

// Constructor
DataClientManager::DataClientManager(const std::string& configFilePath)
  : m_configFilePath(configFilePath),
  m_maxLogEntries(1000),
  m_autoSaveData(false),
  m_dataSaveInterval(60)
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
void DataClientManager::UpdateClients() {
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
        std::lock_guard<std::mutex> lock(*info.dataMutex);  // Changed to use the shared_ptr mutex

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
        }

        // Log data if configured
        if (info.config.logData && !newValues.empty()) {
          // Just log the latest value to avoid spamming the log
          std::stringstream ss;
          ss << "Data from " << info.config.id << ": " << info.latestValue;
          if (info.config.displayUnitSuffix) {
            ss << " " << info.config.unit;
          }
          Logger::GetInstance()->LogInfo(ss.str());  // Changed to LogInfo since LogDebug might not be available
        }
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
        std::lock_guard<std::mutex> lock(*info.dataMutex);  // Changed to use the shared_ptr mutex

        // Display latest received value with unit
        std::string valueLabel = "Latest value: " + std::to_string(info.latestValue);
        if (info.config.displayUnitSuffix) {
          valueLabel += " " + info.config.unit;
        }
        ImGui::Text("%s", valueLabel.c_str());

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

          // Add margins (10% padding)
          float range = maxValue - minValue;
          if (range < 0.001f) range = 0.1f;  // Prevent too small ranges

          float margin = range * 0.1f;
          minValue = (std::min)(0.0f, minValue - margin);
          maxValue = (std::max)(1.0f, maxValue + margin);

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

          ImGui::Text("Min: %.6f, Max: %.6f", minValue, maxValue);

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

              // Value column
              ImGui::TableNextColumn();
              ImGui::Text("%.6f", info.dataPoints[idx].value);
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