#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_client.h"
#include "include/logger.h"
#include "imgui.h"
#include <algorithm>
// Add this include at the top of keithley2400_manager.cpp
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
Keithley2400Manager::Keithley2400Manager()
  : m_logger(Logger::GetInstance())
  , m_showWindow(true)
  , m_name("Keithley 2400 Manager")
{
  m_logger->LogInfo("Keithley2400Manager: Initialized");
}

Keithley2400Manager::~Keithley2400Manager() {
  DisconnectAll();
  m_logger->LogInfo("Keithley2400Manager: Destroyed");
}

bool Keithley2400Manager::Initialize(const std::string& configFile) {
  if (!configFile.empty()) {
    return LoadConfiguration(configFile);
  }
  else {
    LoadDefaultConfiguration();
    return true;
  }
}

// Updated AddClient method to store connection info
bool Keithley2400Manager::AddClient(const std::string& name, const std::string& ip, int port) {
  if (m_clients.find(name) != m_clients.end()) {
    m_logger->LogWarning("Keithley2400Manager: Client " + name + " already exists");
    return false;
  }

  auto client = std::make_unique<Keithley2400Client>();
  client->SetName(name + " (" + ip + ":" + std::to_string(port) + ")");

  // Store connection info
  m_clientConnections[name] = { ip, port };

  m_clients[name] = std::move(client);
  m_logger->LogInfo("Keithley2400Manager: Added client " + name + " for " + ip + ":" + std::to_string(port));

  return true;
}

Keithley2400Client* Keithley2400Manager::GetClient(const std::string& name) {
  auto it = m_clients.find(name);
  if (it != m_clients.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool Keithley2400Manager::RemoveClient(const std::string& name) {
  auto it = m_clients.find(name);
  if (it != m_clients.end()) {
    if (it->second->IsConnected()) {
      it->second->Disconnect();
    }
    m_clients.erase(it);
    m_logger->LogInfo("Keithley2400Manager: Removed client " + name);
    return true;
  }
  return false;
}

// Updated ConnectAll method to use stored connection info
bool Keithley2400Manager::ConnectAll() {
  bool allConnected = true;

  for (auto& [name, client] : m_clients) {
    // Use stored connection info
    auto connIt = m_clientConnections.find(name);
    if (connIt != m_clientConnections.end()) {
      const auto& [ip, port] = connIt->second;
      if (!client->Connect(ip, port)) {
        m_logger->LogWarning("Keithley2400Manager: Failed to connect client " + name + " to " + ip + ":" + std::to_string(port));
        allConnected = false;
      }
    }
    else {
      m_logger->LogWarning("Keithley2400Manager: No connection info for client " + name);
      allConnected = false;
    }
  }

  return allConnected;
}

void Keithley2400Manager::DisconnectAll() {
  for (auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      client->Disconnect();
    }
  }
}

std::vector<std::string> Keithley2400Manager::GetClientNames() const {
  std::vector<std::string> names;
  for (const auto& [name, client] : m_clients) {
    names.push_back(name);
  }
  return names;
}

bool Keithley2400Manager::SetAllOutputs(bool enable) {
  bool allSuccess = true;

  for (auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      if (!client->SetOutput(enable)) {
        allSuccess = false;
        m_logger->LogWarning("Keithley2400Manager: Failed to set output for " + name);
      }
    }
  }

  return allSuccess;
}

bool Keithley2400Manager::ResetAllInstruments() {
  bool allSuccess = true;

  for (auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      if (!client->ResetInstrument()) {
        allSuccess = false;
        m_logger->LogWarning("Keithley2400Manager: Failed to reset " + name);
      }
    }
  }

  return allSuccess;
}

void Keithley2400Manager::StartAllPolling(int intervalMs) {
  for (auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      client->StartPolling(intervalMs);
    }
  }
}

void Keithley2400Manager::StopAllPolling() {
  for (auto& [name, client] : m_clients) {
    client->StopPolling();
  }
}

Keithley2400Manager::AggregatedData Keithley2400Manager::GetAggregatedData() const {
  AggregatedData data;
  data.connectedCount = 0;
  data.totalCount = m_clients.size();
  data.totalVoltage = 0.0;
  data.totalCurrent = 0.0;
  data.totalPower = 0.0;

  for (const auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      data.connectedCount++;
      auto reading = client->GetLatestReading();
      data.totalVoltage += reading.voltage;
      data.totalCurrent += reading.current;
      data.totalPower += reading.power;
      data.readings.push_back({ name, reading });
    }
  }

  return data;
}

void Keithley2400Manager::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_name.c_str(), &m_showWindow)) {

    // Header info
    auto data = GetAggregatedData();
    ImGui::Text("Keithley 2400 Manager");
    ImGui::Text("Clients: %d connected / %d total", data.connectedCount, data.totalCount);

    ImGui::Separator();

    // Bulk operations
    ImGui::Text("Bulk Operations:");

    if (ImGui::Button("Connect All")) {
      ConnectAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disconnect All")) {
      DisconnectAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset All")) {
      ResetAllInstruments();
    }

    // Output control
    if (ImGui::Button("All Outputs ON")) {
      SetAllOutputs(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("All Outputs OFF")) {
      SetAllOutputs(false);
    }

    ImGui::Separator();

    // Client management
    ImGui::Text("Add New Client:");
    static char newClientName[64] = "";
    static char newClientIP[64] = "localhost";
    static int newClientPort = 8888;

    ImGui::InputText("Name", newClientName, sizeof(newClientName));
    ImGui::InputText("IP", newClientIP, sizeof(newClientIP));
    ImGui::InputInt("Port", &newClientPort);

    if (ImGui::Button("Add Client")) {
      if (strlen(newClientName) > 0) {
        AddClient(newClientName, newClientIP, newClientPort);
        // Clear the input fields
        strcpy_s(newClientName, sizeof(newClientName), "");
      }
    }

    ImGui::Separator();

    // Client list
    ImGui::Text("Clients:");

    for (auto& [name, client] : m_clients) {
      ImGui::PushID(name.c_str());

      // Status indicator
      bool connected = client->IsConnected();
      ImGui::TextColored(connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
        connected ? "●" : "●");
      ImGui::SameLine();

      // Client name and controls
      ImGui::Text("%s", name.c_str());
      ImGui::SameLine();

      if (connected) {
        if (ImGui::Button("Disconnect")) {
          client->Disconnect();
        }
        ImGui::SameLine();
        if (ImGui::Button("Show UI")) {
          if (!client->IsVisible()) {
            client->ToggleWindow();
          }
        }
      }
      else {
        if (ImGui::Button("Connect")) {
          client->Connect("localhost", 8888);  // You might want to store IP/port
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Remove")) {
        RemoveClient(name);
        ImGui::PopID();
        break;  // Break to avoid iterator invalidation
      }

      // Show latest reading if connected
      if (connected) {
        auto reading = client->GetLatestReading();
        ImGui::Text("    V: %.3fV, I: %.6fA, P: %.6fW",
          reading.voltage, reading.current, reading.power);
      }

      ImGui::PopID();
    }

    ImGui::Separator();

    // Aggregated data display
    if (data.connectedCount > 0) {
      ImGui::Text("Aggregated Data:");
      ImGui::Text("Total Voltage: %.6f V", data.totalVoltage);
      ImGui::Text("Total Current: %.6f A", data.totalCurrent);
      ImGui::Text("Total Power: %.6f W", data.totalPower);
    }
  }
  ImGui::End();
}

void Keithley2400Manager::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

bool Keithley2400Manager::IsVisible() const {
  return m_showWindow;
}

const std::string& Keithley2400Manager::GetName() const {
  return m_name;
}


void Keithley2400Manager::LoadDefaultConfiguration() {
  // Update default clients to use 127.0.0.101
  AddClient("Keithley-Main", "127.0.0.101", 8888);
  //AddClient("Keithley-Secondary", "127.0.0.101", 8889);  // Different port for secondary

  m_logger->LogInfo("Keithley2400Manager: Loaded default configuration with 2 clients");
}

bool Keithley2400Manager::SaveConfiguration(const std::string& filename) {
  try {
    json config;

    // Save manager settings
    config["manager_settings"] = {
      {"auto_connect", m_uiState.autoConnect},
      {"default_polling_interval", m_uiState.defaultPollingInterval},
      {"bulk_voltage", m_uiState.bulkVoltage},
      {"bulk_current", m_uiState.bulkCurrent},
      {"bulk_compliance", m_uiState.bulkCompliance},
      {"bulk_source_mode", m_uiState.bulkSourceMode}
    };

    // Save client configurations
    json clients_array = json::array();

    for (const auto& [name, client] : m_clients) {
      json client_config;
      client_config["name"] = name;
      client_config["display_name"] = client->GetName();

      // Extract IP and port from stored connection (you'll need to store these)
      // For now, we'll use a simple approach
      if (name == "Keithley-Main") {
        client_config["ip"] = "192.168.1.100";
        client_config["port"] = 8888;
      }
      else if (name == "Keithley-Secondary") {
        client_config["ip"] = "192.168.1.101";
        client_config["port"] = 8888;
      }
      else {
        client_config["ip"] = "localhost";
        client_config["port"] = 8888;
      }

      client_config["auto_connect"] = true;
      client_config["enabled"] = true;
      client_config["polling_interval"] = 1000;

      clients_array.push_back(client_config);
    }

    config["clients"] = clients_array;

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
      m_logger->LogError("Keithley2400Manager: Cannot open file for writing: " + filename);
      return false;
    }

    file << std::setw(2) << config << std::endl;
    file.close();

    m_logger->LogInfo("Keithley2400Manager: Configuration saved to " + filename);
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Keithley2400Manager: Error saving configuration: " + std::string(e.what()));
    return false;
  }
}

bool Keithley2400Manager::LoadConfiguration(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      m_logger->LogWarning("Keithley2400Manager: Cannot open config file: " + filename + ", using defaults");
      LoadDefaultConfiguration();
      return false;
    }

    json config;
    file >> config;
    file.close();

    // Clear existing clients
    m_clients.clear();

    // Load manager settings
    if (config.contains("manager_settings")) {
      auto& settings = config["manager_settings"];
      m_uiState.autoConnect = settings.value("auto_connect", true);
      m_uiState.defaultPollingInterval = settings.value("default_polling_interval", 1000);
      m_uiState.bulkVoltage = settings.value("bulk_voltage", 0.0);
      m_uiState.bulkCurrent = settings.value("bulk_current", 0.001);
      m_uiState.bulkCompliance = settings.value("bulk_compliance", 0.1);
      m_uiState.bulkSourceMode = settings.value("bulk_source_mode", 0);
    }

    // Load client configurations
    // In LoadConfiguration method, around line where you create clients:
    // In LoadConfiguration method:
   // In keithley2400_manager.cpp, modify the LoadConfiguration method:
    for (const auto& client_config : config["clients"]) {
      std::string name = client_config.value("name", "Unknown");
      std::string display_name = client_config.value("display_name", name);
      std::string ip = client_config.value("ip", "localhost");
      int port = client_config.value("port", 8888);
      bool enabled = client_config.value("enabled", true);
      int polling_interval = client_config.value("polling_interval", m_uiState.defaultPollingInterval);

      if (enabled) {
        if (AddClient(name, ip, port)) {
          Keithley2400Client* client = GetClient(name);
          if (client) {
            // Set the name to the config name (SMU1), not the display_name
            client->SetName(name);  // <-- This will set m_name = "SMU1"

            // Store the polling interval for this client
            m_clientPollingIntervals[name] = polling_interval;
          }

          m_clientConnections[name] = { ip, port };
          m_logger->LogInfo("Keithley2400Manager: Loaded client " + name + " with polling interval " + std::to_string(polling_interval) + "ms");
        }
      }
    }

    m_logger->LogInfo("Keithley2400Manager: Configuration loaded from " + filename + " with " +
      std::to_string(m_clients.size()) + " clients");
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Keithley2400Manager: Error loading configuration: " + std::string(e.what()));
    LoadDefaultConfiguration();
    return false;
  }
}