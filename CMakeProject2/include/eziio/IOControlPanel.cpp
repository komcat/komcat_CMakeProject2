#include "IOControlPanel.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

// Define the default config file name
const std::string IOControlPanel::DEFAULT_CONFIG_FILE = "io_panel_config.json";

IOControlPanel::IOControlPanel(EziIOManager& manager)
  : m_ioManager(manager)
{
  // Try to load configuration from the default file
  if (!LoadConfiguration(DEFAULT_CONFIG_FILE)) {
    std::cerr << "Failed to load IO Panel configuration, using hardcoded defaults" << std::endl;
    // Fall back to hardcoded initialization
    InitializePins();
  }

  // Initial state refresh
  RefreshPinStates();

  std::cout << "IOControlPanel initialized with " << m_outputPins.size() << " pins" << std::endl;
}

bool IOControlPanel::LoadConfiguration(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Could not open configuration file: " << filename << std::endl;
      return false;
    }

    nlohmann::json config;
    file >> config;

    // Clear existing pins
    m_outputPins.clear();

    // Check if we have panels in the configuration
    if (!config.contains("panels") || !config["panels"].is_array()) {
      std::cerr << "Invalid configuration format: missing 'panels' array" << std::endl;
      return false;
    }

    const auto& panels = config["panels"];
    if (panels.empty()) {
      std::cerr << "No panels defined in configuration" << std::endl;
      return false;
    }

    const auto& panel = panels[0];

    // Load panel name if available
    if (panel.contains("name") && panel["name"].is_string()) {
      m_name = panel["name"].get<std::string>();
    }

    // Load UI settings if available
    if (panel.contains("autoRefresh") && panel["autoRefresh"].is_boolean()) {
      m_autoRefresh = panel["autoRefresh"].get<bool>();
    }
    if (panel.contains("refreshInterval") && panel["refreshInterval"].is_number()) {
      m_refreshInterval = panel["refreshInterval"].get<float>();
    }
    if (panel.contains("compactMode") && panel["compactMode"].is_boolean()) {
      m_compactMode = panel["compactMode"].get<bool>();
    }

    // Load pins
    if (!panel.contains("pins") || !panel["pins"].is_array()) {
      std::cerr << "No pins defined in panel" << std::endl;
      return false;
    }

    for (const auto& pinData : panel["pins"]) {
      PinConfig pin;

      // Validate and load pin configuration
      if (pinData.contains("deviceName") && pinData["deviceName"].is_string()) {
        pin.deviceName = pinData["deviceName"].get<std::string>();
      }
      else {
        std::cerr << "Missing or invalid deviceName in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("deviceId") && pinData["deviceId"].is_number_integer()) {
        pin.deviceId = pinData["deviceId"].get<int>();
      }
      else {
        std::cerr << "Missing or invalid deviceId in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("pinNumber") && pinData["pinNumber"].is_number_integer()) {
        pin.pinNumber = pinData["pinNumber"].get<int>();
      }
      else {
        std::cerr << "Missing or invalid pinNumber in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("label") && pinData["label"].is_string()) {
        pin.label = pinData["label"].get<std::string>();
      }
      else {
        std::cerr << "Missing or invalid label in pin configuration" << std::endl;
        continue;
      }

      // Initialize other fields
      pin.currentState = false;
      pin.lastError = EziIOError::SUCCESS;
      pin.lastToggleTime = std::chrono::steady_clock::now();

      // Add the pin to our list
      m_outputPins.push_back(pin);
    }

    std::cout << "Successfully loaded " << m_outputPins.size() << " pins from " << filename << std::endl;
    return true;

  }
  catch (const nlohmann::json::exception& e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    return false;
  }
  catch (const std::exception& e) {
    std::cerr << "Error loading configuration: " << e.what() << std::endl;
    return false;
  }
}

void IOControlPanel::InitializePins() {
  // Clear existing pins
  m_outputPins.clear();

  // Add the output pins you want to control (hardcoded) - use this as fallback
  // Format: deviceName, deviceId, pinNumber, label

  auto now = std::chrono::steady_clock::now();

  // IOBottom pins (Example from your IOConfig.json)
  m_outputPins.push_back({ "IOBottom", 0, 0, "L_Gripper", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 2, "R_Gripper", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 10, "Vacuum_Base", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 15, "Dispenser_Shot", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 4, "UV_Head", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 5, "Dispenser_Head", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 14, "UV_PLC1", false, EziIOError::SUCCESS, now });
  m_outputPins.push_back({ "IOBottom", 0, 13, "UV_PLC2", false, EziIOError::SUCCESS, now });
}

void IOControlPanel::RenderUI() {
  if (!m_showWindow) return;

  // Update notifications
  UpdateNotifications(ImGui::GetIO().DeltaTime);

  // Auto-refresh timer
  if (m_autoRefresh) {
    m_refreshTimer += ImGui::GetIO().DeltaTime;
    if (m_refreshTimer >= m_refreshInterval) {
      RefreshPinStates();
      m_refreshTimer = 0.0f;
    }
  }

  // Create and configure window
  ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("IO Control Panel", &m_showWindow, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  // Title and status
  ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Output Pin Controls");

  // Show connection status
  ImGui::SameLine();
  int connectedDevices = m_ioManager.getConnectedDeviceCount();
  if (connectedDevices > 0) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%d Connected]", connectedDevices);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[No Devices]");
  }

  ImGui::Separator();

  // Control bar
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

  ImGui::Checkbox("Auto", &m_autoRefresh);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Auto-refresh pin states");
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::DragFloat("##interval", &m_refreshInterval, 0.1f, 0.1f, 5.0f, "%.1fs");

  ImGui::SameLine();
  if (ImGui::Button("Refresh", ImVec2(60, 0))) {
    RefreshPinStates();
    AddNotification("Refreshed", false);
  }

  ImGui::SameLine();
  ImGui::Checkbox("Compact", &m_compactMode);

  ImGui::SameLine();
  ImGui::Checkbox("Debug", &m_showDebugInfo);

  ImGui::PopStyleVar();

  if (m_autoRefresh) {
    float remaining = m_refreshInterval - m_refreshTimer;
    ImGui::Text("Next refresh: %.1fs", remaining);
  }

  ImGui::Separator();

  // Render notifications
  RenderNotifications();

  // Create table for the buttons
  if (m_compactMode) {
    // Compact mode - 2 column layout
    if (ImGui::BeginTable("OutputPinTableCompact", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
      ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      for (auto& pin : m_outputPins) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("%s", pin.label.c_str());

        ImGui::TableNextColumn();
        RenderPinControl(pin);
      }
      ImGui::EndTable();
    }
  }
  else {
    // Full mode - 4 column layout
    if (ImGui::BeginTable("OutputPinTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 40.0f);
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      for (auto& pin : m_outputPins) {
        ImGui::TableNextRow();

        // Pin number column
        ImGui::TableNextColumn();
        ImGui::Text("%d", pin.pinNumber);

        // Label column
        ImGui::TableNextColumn();
        ImGui::Text("%s", pin.label.c_str());

        // Status column
        ImGui::TableNextColumn();
        if (pin.lastError != EziIOError::SUCCESS) {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "ERROR");
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", GetErrorString(pin.lastError).c_str());
          }
        }
        else if (pin.currentState) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
        }
        else {
          ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "OFF");
        }

        // Control column
        ImGui::TableNextColumn();
        RenderPinControl(pin);
      }
      ImGui::EndTable();
    }
  }

  // Statistics and debug info
  if (m_showDebugInfo) {
    ImGui::Separator();
    RenderStatistics();
  }

  // Control buttons at bottom
  ImGui::Separator();

  if (ImGui::Button("All OFF", ImVec2(80, 25))) {
    for (auto& pin : m_outputPins) {
      EziIOError result = m_ioManager.setOutput(pin.deviceId, pin.pinNumber, false);
      if (result == EziIOError::SUCCESS) {
        pin.currentState = false;
        pin.lastToggleTime = std::chrono::steady_clock::now();
      }
      else {
        pin.lastError = result;
        m_totalErrors++;
      }
    }
    AddNotification("All outputs turned OFF", false);
    RefreshPinStates();
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear Errors", ImVec2(80, 25))) {
    for (auto& pin : m_outputPins) {
      pin.lastError = EziIOError::SUCCESS;
    }
    m_totalErrors = 0;
    AddNotification("Errors cleared", false);
  }

  ImGui::SameLine();
  if (ImGui::Button("Reload Config", ImVec2(90, 25))) {
    if (LoadConfiguration(DEFAULT_CONFIG_FILE)) {
      RefreshPinStates();
      AddNotification("Configuration reloaded", false);
    }
    else {
      AddNotification("Failed to reload config", true);
    }
  }

  ImGui::End();
}

void IOControlPanel::RenderPinControl(PinConfig& pin) {
  // Generate unique ID for this pin
  std::string id = "##" + pin.deviceName + "_" + std::to_string(pin.pinNumber);

  // Determine button colors based on state and errors
  ImVec4 buttonColor, hoverColor;

  if (pin.lastError != EziIOError::SUCCESS) {
    // Error state - orange/red
    buttonColor = ImVec4(0.8f, 0.3f, 0.0f, 0.8f);
    hoverColor = ImVec4(1.0f, 0.4f, 0.0f, 0.9f);
  }
  else if (pin.currentState) {
    // ON state - green
    buttonColor = ImVec4(0.0f, 0.7f, 0.0f, 0.8f);
    hoverColor = ImVec4(0.0f, 0.9f, 0.0f, 0.9f);
  }
  else {
    // OFF state - gray
    buttonColor = ImVec4(0.4f, 0.4f, 0.4f, 0.8f);
    hoverColor = ImVec4(0.6f, 0.6f, 0.6f, 0.9f);
  }

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

  // Create toggle button
  std::string buttonLabel = (pin.currentState ? "ON" : "OFF") + id;
  if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 22))) {
    // Toggle the state
    bool newState = !pin.currentState;
    EziIOError result = m_ioManager.setOutput(pin.deviceId, pin.pinNumber, newState);

    if (result == EziIOError::SUCCESS) {
      pin.currentState = newState;
      pin.lastError = EziIOError::SUCCESS;
      pin.lastToggleTime = std::chrono::steady_clock::now();
      m_successfulOperations++;

      std::stringstream msg;
      msg << pin.label << " → " << (newState ? "ON" : "OFF");
      AddNotification(msg.str(), false);

      if (m_showDebugInfo) {
        std::cout << "Toggled " << pin.deviceName << " pin " << pin.pinNumber
          << " (" << pin.label << ") to " << (newState ? "ON" : "OFF") << std::endl;
      }
    }
    else {
      pin.lastError = result;
      m_totalErrors++;

      std::stringstream msg;
      msg << "Failed: " << pin.label << " - " << GetErrorString(result);
      AddNotification(msg.str(), true);
    }
  }

  // Show tooltip with additional info
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Device: %s (ID: %d)", pin.deviceName.c_str(), pin.deviceId);
    ImGui::Text("Pin: %d", pin.pinNumber);
    ImGui::Text("Current: %s", pin.currentState ? "ON" : "OFF");

    if (pin.lastError != EziIOError::SUCCESS) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
        "Error: %s", GetErrorString(pin.lastError).c_str());
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pin.lastToggleTime).count();
    ImGui::Text("Last toggle: %lld sec ago", elapsed);

    ImGui::EndTooltip();
  }

  ImGui::PopStyleColor(3);
}

void IOControlPanel::RefreshPinStates() {
  for (auto& pin : m_outputPins) {
    uint32_t outputs = 0, status = 0;
    EziIOError result = m_ioManager.getLastOutputStatus(pin.deviceId, outputs, status);

    if (result == EziIOError::SUCCESS) {
      uint32_t mask = GetPinMask(pin.deviceId, pin.pinNumber);
      pin.currentState = (outputs & mask) != 0;
      pin.lastError = EziIOError::SUCCESS;
    }
    else {
      pin.lastError = result;
      if (result == EziIOError::DEVICE_DISCONNECTED) {
        pin.currentState = false;  // Assume OFF if disconnected
      }
    }
  }
}

void IOControlPanel::RenderNotifications() {
  if (m_notifications.empty()) return;

  float y_offset = 0;
  for (const auto& notif : m_notifications) {
    ImVec4 color = notif.isError ?
      ImVec4(1.0f, 0.3f, 0.3f, notif.timeRemaining / 3.0f) :
      ImVec4(0.3f, 1.0f, 0.3f, notif.timeRemaining / 3.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s (%.1fs)", notif.message.c_str(), notif.timeRemaining);
    ImGui::PopStyleColor();

    y_offset += 20;
  }
}

void IOControlPanel::RenderStatistics() {
  ImGui::Text("Debug Information:");
  ImGui::Text("Total Pins: %zu", m_outputPins.size());
  ImGui::Text("Successful Ops: %d", m_successfulOperations);
  ImGui::Text("Total Errors: %d", m_totalErrors);
  ImGui::Text("Connected Devices: %d", m_ioManager.getConnectedDeviceCount());

  // Show pin masks
  ImGui::Text("Pin Masks:");
  for (const auto& pin : m_outputPins) {
    uint32_t mask = GetPinMask(pin.deviceId, pin.pinNumber);
    ImGui::Text("  %s: 0x%08X", pin.label.c_str(), mask);
  }
}

void IOControlPanel::AddNotification(const std::string& message, bool isError) {
  Notification notif;
  notif.message = message;
  notif.timeRemaining = 3.0f;  // Show for 3 seconds
  notif.isError = isError;

  m_notifications.push_back(notif);

  // Limit to 5 notifications
  if (m_notifications.size() > 5) {
    m_notifications.erase(m_notifications.begin());
  }
}

void IOControlPanel::UpdateNotifications(float deltaTime) {
  for (auto it = m_notifications.begin(); it != m_notifications.end();) {
    it->timeRemaining -= deltaTime;
    if (it->timeRemaining <= 0) {
      it = m_notifications.erase(it);
    }
    else {
      ++it;
    }
  }
}

uint32_t IOControlPanel::GetPinMask(int deviceId, int pinNumber) const {
  // For IOBottom (deviceId 0), pins use different mask calculation
  if (deviceId == 0) {
    return 0x10000 << pinNumber;  // Specific to IOBottom
  }
  else if (deviceId == 1) {
    return 0x100 << pinNumber;    // Specific to IOTop
  }
  return 1U << pinNumber;  // Default
}

std::string IOControlPanel::GetErrorString(EziIOError error) const {
  return EziIOManager::getErrorString(error);
}