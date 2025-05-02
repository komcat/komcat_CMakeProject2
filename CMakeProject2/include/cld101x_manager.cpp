#include "include/cld101x_manager.h"
#include "include/logger.h"
#include "imgui.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

CLD101xManager::CLD101xManager()
  : m_showWindow(true)
  , m_name("CLD101x Manager")
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("CLD101xManager: Initialized");
}

CLD101xManager::~CLD101xManager() {
  DisconnectAll();
  m_clients.clear();
  m_logger->LogInfo("CLD101xManager: Destroyed");
}

bool CLD101xManager::Initialize(const std::string& configFile) {
  if (configFile.empty()) {
    // No config file provided, use default settings
    AddClient("CLD101x", "127.0.0.88", 65432);
    m_logger->LogInfo("CLD101xManager: Using default settings (no config file)");
    return true;
  }

  try {
    // Open the JSON configuration file
    std::ifstream file(configFile);
    if (!file.is_open()) {
      m_logger->LogError("CLD101xManager: Failed to open config file: " + configFile);
      return false;
    }

    // Parse the JSON
    json config;
    file >> config;

    // Check for the clients array
    if (config.contains("clients") && config["clients"].is_array()) {
      // Clear existing clients first
      m_clients.clear();

      // Process each client in the array
      for (const auto& client : config["clients"]) {
        std::string name = client.value("name", "CLD101x");
        std::string ip = client.value("ip", "127.0.0.88");
        int port = client.value("port", 65432);
        bool enabled = client.value("enabled", true);

        if (enabled) {
          AddClient(name, ip, port);
          m_logger->LogInfo("CLD101xManager: Added client " + name + " at " + ip + ":" + std::to_string(port));
        }
      }
    }
    else {
      m_logger->LogWarning("CLD101xManager: No clients defined in config file, using defaults");
      // Use default values
      AddClient("CLD101x", "127.0.0.88", 65432);
    }

    m_logger->LogInfo("CLD101xManager: Initialized from config file: " + configFile);
    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("CLD101xManager: Error parsing config file: " + std::string(e.what()));
    // Use default values
    AddClient("CLD101x", "127.0.0.88", 65432);
    return false;
  }
}

bool CLD101xManager::AddClient(const std::string& name, const std::string& ip, int port) {
  // Check if a client with this name already exists
  if (m_clients.find(name) != m_clients.end()) {
    m_logger->LogWarning("CLD101xManager: Client with name '" + name + "' already exists");
    return false;
  }

  // Create a new client
  std::unique_ptr<CLD101xClient> client = std::make_unique<CLD101xClient>();
  m_clients[name] = std::move(client);
  m_logger->LogInfo("CLD101xManager: Added client '" + name + "'");
  return true;
}

CLD101xClient* CLD101xManager::GetClient(const std::string& name) {
  auto it = m_clients.find(name);
  if (it != m_clients.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool CLD101xManager::RemoveClient(const std::string& name) {
  auto it = m_clients.find(name);
  if (it != m_clients.end()) {
    // Disconnect first if needed
    if (it->second->IsConnected()) {
      it->second->Disconnect();
    }
    // Remove from map
    m_clients.erase(it);
    m_logger->LogInfo("CLD101xManager: Removed client '" + name + "'");
    return true;
  }
  m_logger->LogWarning("CLD101xManager: Client '" + name + "' not found for removal");
  return false;
}

bool CLD101xManager::ConnectAll() {
  bool allSuccess = true;
  for (auto& [name, client] : m_clients) {
    if (!client->IsConnected()) {
      if (!client->Connect("127.0.0.88", 65432)) {
        m_logger->LogError("CLD101xManager: Failed to connect client '" + name + "'");
        allSuccess = false;
      }
    }
  }
  return allSuccess;
}

void CLD101xManager::DisconnectAll() {
  for (auto& [name, client] : m_clients) {
    if (client->IsConnected()) {
      client->Disconnect();
      m_logger->LogInfo("CLD101xManager: Disconnected client '" + name + "'");
    }
  }
}

std::vector<std::string> CLD101xManager::GetClientNames() const {
  std::vector<std::string> names;
  names.reserve(m_clients.size());
  for (const auto& [name, _] : m_clients) {
    names.push_back(name);
  }
  return names;
}

void CLD101xManager::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_name.c_str(), &m_showWindow)) {
    ImGui::Text("Manage CLD101x Clients");
    ImGui::Separator();

    // Client list and status
    if (m_clients.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No clients available.");

      // Add client section
      static char nameBuffer[64] = "CLD101x";
      static char ipBuffer[64] = "127.0.0.88";
      static int port = 65432;

      ImGui::Separator();
      ImGui::Text("Add New Client:");
      ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
      ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
      ImGui::InputInt("Port", &port);

      if (ImGui::Button("Add Client")) {
        if (AddClient(nameBuffer, ipBuffer, port)) {
          // Reset name for next client, but keep IP and port
          nameBuffer[0] = '\0';
        }
      }
    }
    else {
      if (ImGui::Button("Connect All")) {
        ConnectAll();
      }
      ImGui::SameLine();
      if (ImGui::Button("Disconnect All")) {
        DisconnectAll();
      }

      ImGui::Separator();

      // Display client list
      ImGui::Text("Clients:");
      ImGui::Columns(3, "clientsColumns");
      ImGui::SetColumnWidth(0, 150);
      ImGui::SetColumnWidth(1, 200);
      ImGui::Text("Name"); ImGui::NextColumn();
      ImGui::Text("Status"); ImGui::NextColumn();
      ImGui::Text("Actions"); ImGui::NextColumn();
      ImGui::Separator();

      for (auto& [name, client] : m_clients) {
        ImGui::Text("%s", name.c_str()); ImGui::NextColumn();

        if (client->IsConnected()) {
          ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Connected");
        }
        else {
          ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Disconnected");
        }
        ImGui::NextColumn();

        // Unique ID for buttons
        std::string btnId = "##" + name;

        if (client->IsConnected()) {
          if (ImGui::Button(("Disconnect" + btnId).c_str())) {
            client->Disconnect();
          }
        }
        else {
          if (ImGui::Button(("Connect" + btnId).c_str())) {
            client->Connect("127.0.0.88", 65432);
          }
        }

        ImGui::SameLine();

        if (ImGui::Button(("Open" + btnId).c_str())) {
          client->ToggleWindow();
        }

        ImGui::SameLine();

        if (ImGui::Button(("Remove" + btnId).c_str())) {
          // Mark for removal
          m_logger->LogInfo("CLD101xManager: Removing client '" + name + "'");
          // We can't remove it right now as we're iterating
          // Schedule removal after the loop
          RemoveClient(name);
          // Break the loop as the iterator is now invalid
          break;
        }

        ImGui::NextColumn();
      }

      ImGui::Columns(1);
      ImGui::Separator();

      // Add client section
      static char nameBuffer[64] = "CLD101x";
      static char ipBuffer[64] = "127.0.0.88";
      static int port = 65432;

      ImGui::Text("Add New Client:");
      ImGui::InputText("Name##new", nameBuffer, sizeof(nameBuffer));
      ImGui::InputText("IP Address##new", ipBuffer, sizeof(ipBuffer));
      ImGui::InputInt("Port##new", &port);

      if (ImGui::Button("Add Client")) {
        if (AddClient(nameBuffer, ipBuffer, port)) {
          // Reset name for next client, but keep IP and port
          nameBuffer[0] = '\0';
        }
      }
    }

    // Show device info
    ImGui::Separator();

    // Display latest readings from connected clients
    if (!m_clients.empty()) {
      ImGui::Text("Latest Readings:");
      ImGui::Columns(3, "readingsColumns");
      ImGui::SetColumnWidth(0, 150);
      ImGui::SetColumnWidth(1, 150);
      ImGui::Text("Client"); ImGui::NextColumn();
      ImGui::Text("Temperature (C)"); ImGui::NextColumn();
      ImGui::Text("Current (A)"); ImGui::NextColumn();
      ImGui::Separator();

      for (const auto& [name, client] : m_clients) {
        ImGui::Text("%s", name.c_str()); ImGui::NextColumn();

        if (client->IsConnected()) {
          ImGui::Text("%.2f", client->GetTemperature()); ImGui::NextColumn();
          ImGui::Text("%.3f", client->GetLaserCurrent()); ImGui::NextColumn();
        }
        else {
          ImGui::Text("--"); ImGui::NextColumn();
          ImGui::Text("--"); ImGui::NextColumn();
        }
      }

      ImGui::Columns(1);
    }
  }
  ImGui::End();

  // Render each client's UI window if visible
  for (auto& [name, client] : m_clients) {
    if (client->IsVisible()) {
      client->RenderUI();
    }
  }
}

void CLD101xManager::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

bool CLD101xManager::IsVisible() const {
  return m_showWindow;
}

const std::string& CLD101xManager::GetName() const {
  return m_name;
}