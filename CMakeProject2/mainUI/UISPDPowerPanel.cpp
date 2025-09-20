// UISPDPowerPanel.cpp
#include "UISPDPowerPanel.h"
#include "include/PowerSupply/SPDPowerSupplyManager.h"
#include "imgui.h"
#include <iostream>
#include <chrono>

// Forward declaration in UISPDPowerPanel.h
//class SPDPowerSupplyManager;

UISPDPowerPanel::UISPDPowerPanel(SPDPowerSupplyManager& spdManager)
  : m_spdManager(spdManager) {
  std::cout << "[INFO] UISPDPowerPanel created" << std::endl;
}

void UISPDPowerPanel::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(1300, 750), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("SPD Power Supply Control", &m_showWindow, ImGuiWindowFlags_MenuBar)) {

    // Menu bar
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Discover All Devices")) {
          m_spdManager.AddDiscoveredDevices(false);
        }
        if (ImGui::MenuItem("Connect All Devices")) {
          m_spdManager.ConnectAll();
        }
        if (ImGui::MenuItem("Emergency Stop")) {
          m_spdManager.EmergencyStop();
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    // Main content area
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    // Calculate panel widths (3 columns)
    float leftPanelWidth = contentSize.x * 0.28f;   // 28% for device list
    float middlePanelWidth = contentSize.x * 0.36f; // 36% for CV/CC settings
    float rightPanelWidth = contentSize.x * 0.36f;  // 36% for monitoring & advanced

    // LEFT PANEL - Connected Devices
    ImGui::BeginChild("DeviceListPanel", ImVec2(leftPanelWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // MIDDLE PANEL - CV/CC Settings
    ImGui::BeginChild("CVCCSettingsPanel", ImVec2(middlePanelWidth, contentSize.y), true);
    RenderMiddlePanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // RIGHT PANEL - Monitoring & Advanced Settings
    ImGui::BeginChild("MonitoringPanel", ImVec2(rightPanelWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();
  }
  ImGui::End();
}

void UISPDPowerPanel::RenderLeftPanel() {
  ImGui::Text("SPD Power Supplies");
  ImGui::Separator();

  // Device list
  RenderDeviceList();

  ImGui::Separator();

  // Global controls
  RenderGlobalControls();
}

void UISPDPowerPanel::RenderDeviceList() {
  auto deviceNames = m_spdManager.GetDeviceNames();

  ImGui::Text("Devices: %zu", deviceNames.size());
  ImGui::Text("Connected: %d", m_spdManager.GetConnectedCount());
  ImGui::Spacing();

  if (deviceNames.empty()) {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No devices found");
    ImGui::Text("Click 'Discover Devices' below");
    return;
  }

  for (const auto& deviceName : deviceNames) {
    auto* device = m_spdManager.GetDevice(deviceName);
    bool isConnected = device && device->isConnected();
    bool isSelected = (m_selectedDeviceId == deviceName);

    // Color coding for device status
    if (isConnected) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Green
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.2f, 1.0f)); // Red
    }

    if (ImGui::Selectable(deviceName.c_str(), isSelected)) {
      m_selectedDeviceId = deviceName;
      std::cout << "[INFO] Selected device: " << deviceName << std::endl;
    }

    ImGui::PopStyleColor();

    // Show connection status
    ImGui::SameLine();
    if (isConnected) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[CONN]");
    }
    else {
      ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.2f, 1.0f), "[DISC]");
    }

    // Show output status if connected - USE CACHED STATUS
    if (isConnected) {
      ImGui::SameLine();
      const auto& status = GetCachedStatus(deviceName);  // ← Use cache instead of direct call
      if (status.isValid && status.outputEnabled) {
        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "[OUT]");
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[OFF]");
      }
    }
  }
}


void UISPDPowerPanel::RenderGlobalControls() {
  ImGui::Text("System Controls");
  ImGui::Spacing();

  // Emergency stop button (red)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button("EMERGENCY STOP", ImVec2(-1, 30))) {
    m_spdManager.EmergencyStop();
  }
  ImGui::PopStyleColor(2);

  ImGui::Spacing();

  // Global enable/disable
  static bool globalOutputEnabled = false;
  if (ImGui::Checkbox("Enable All Outputs", &globalOutputEnabled)) {
    m_spdManager.SetAllOutputs(globalOutputEnabled);
  }

  ImGui::Spacing();

  // Connection controls
  if (ImGui::Button("Connect All", ImVec2(-1, 0))) {
    m_spdManager.ConnectAll();
  }

  if (ImGui::Button("Disconnect All", ImVec2(-1, 0))) {
    m_spdManager.DisconnectAll();
  }

  if (ImGui::Button("Discover Devices", ImVec2(-1, 0))) {
    m_spdManager.AddDiscoveredDevices(false);
  }

  ImGui::Spacing();

  // Polling status
  ImGui::Text("Polling: %s",
    m_spdManager.IsPollingActive() ? "Active" : "Stopped");
}

void UISPDPowerPanel::RenderMiddlePanel() {
  ImGui::Text("Device Settings");
  ImGui::Separator();

  if (m_selectedDeviceId.empty()) {
    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("No Device Selected");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Text("Select a device from the list on the left");
    ImGui::Text("to configure its settings.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("SPD Power Supply Features:");
    ImGui::BulletText("Constant Voltage (CV) mode");
    ImGui::BulletText("Constant Current (CC) mode");
    ImGui::BulletText("Output enable/disable control");
    ImGui::BulletText("Voltage range: 0-30V");
    ImGui::BulletText("Current range: 0-5A");
    ImGui::BulletText("Real-time monitoring");
    return;
  }

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device) {
    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.2f, 1.0f), "Device Not Found");
    return;
  }

  if (!device->isConnected()) {
    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.2f, 1.0f), "Device Not Connected");
    ImGui::Text("Device: %s", m_selectedDeviceId.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Connect This Device", ImVec2(-1, 0))) {
      device->connect();
    }
    return;
  }

  // Mode selection
  RenderModeSelection();

  ImGui::Separator();

  // Mode-specific controls
  if (m_currentMode == OperatingMode::CONSTANT_VOLTAGE) {
    RenderConstantVoltageControls();
  }
  else {
    RenderConstantCurrentControls();
  }

  ImGui::Separator();

  // Output control
  RenderOutputControls();

  ImGui::Separator();

  // Quick presets
  RenderQuickPresets();
}

void UISPDPowerPanel::RenderModeSelection() {
  ImGui::Text("Operating Mode");
  ImGui::Spacing();

  // Mode selection buttons
  ImVec2 buttonSize = ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 30);

  // CV Mode button
  if (m_currentMode == OperatingMode::CONSTANT_VOLTAGE) {
    ImGui::PushStyleColor(ImGuiCol_Button, GetModeColor(OperatingMode::CONSTANT_VOLTAGE));
  }
  if (ImGui::Button("Constant Voltage", buttonSize)) {
    m_currentMode = OperatingMode::CONSTANT_VOLTAGE;
  }
  if (m_currentMode == OperatingMode::CONSTANT_VOLTAGE) {
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();

  // CC Mode button
  if (m_currentMode == OperatingMode::CONSTANT_CURRENT) {
    ImGui::PushStyleColor(ImGuiCol_Button, GetModeColor(OperatingMode::CONSTANT_CURRENT));
  }
  if (ImGui::Button("Constant Current", buttonSize)) {
    m_currentMode = OperatingMode::CONSTANT_CURRENT;
  }
  if (m_currentMode == OperatingMode::CONSTANT_CURRENT) {
    ImGui::PopStyleColor();
  }

  ImGui::Spacing();
  ImGui::Text("Mode: %s", GetModeString(m_currentMode));
}

void UISPDPowerPanel::RenderConstantVoltageControls() {
  ImGui::Text("Constant Voltage Settings");
  ImGui::Spacing();

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device) return;

  // Voltage setting (primary control in CV mode)
  static float voltage = 0.0f;
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderFloat("##voltage", &voltage, 0.0f, 30.0f, "%.2f V")) {
    device->setVoltage(1, voltage); // Channel 1
  }
  ImGui::PopItemWidth();
  ImGui::Text("Output Voltage: %.2f V", voltage);

  ImGui::Spacing();

  // Current limit (secondary control in CV mode)
  static float currentLimit = 1.0f;
  ImGui::Text("Current Limit");
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderFloat("##currentlimit", &currentLimit, 0.0f, 5.0f, "%.3f A")) {
    device->setCurrent(1, currentLimit); // Channel 1
  }
  ImGui::PopItemWidth();
  ImGui::Text("Max Current: %.3f A", currentLimit);
}

void UISPDPowerPanel::RenderConstantCurrentControls() {
  ImGui::Text("Constant Current Settings");
  ImGui::Spacing();

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device) return;

  // Current setting (primary control in CC mode)
  static float current = 0.0f;
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderFloat("##current", &current, 0.0f, 5.0f, "%.3f A")) {
    device->setCurrent(1, current); // Channel 1
  }
  ImGui::PopItemWidth();
  ImGui::Text("Output Current: %.3f A", current);

  ImGui::Spacing();

  // Voltage limit (secondary control in CC mode)
  static float voltageLimit = 30.0f;
  ImGui::Text("Voltage Limit");
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderFloat("##voltagelimit", &voltageLimit, 0.0f, 30.0f, "%.2f V")) {
    device->setVoltage(1, voltageLimit); // Channel 1
  }
  ImGui::PopItemWidth();
  ImGui::Text("Max Voltage: %.2f V", voltageLimit);
}

void UISPDPowerPanel::RenderOutputControls() {
  ImGui::Text("Output Control");
  ImGui::Spacing();

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device) return;

  auto outputState = device->getOutputState(1); // Channel 1
  bool outputEnabled = outputState.has_value() ? outputState.value() : false;

  // Large output enable/disable button
  ImVec2 buttonSize = ImVec2(-1, 40);
  if (outputEnabled) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
    if (ImGui::Button("OUTPUT ENABLED", buttonSize)) {
      device->setOutput(1, false); // Channel 1
    }
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    if (ImGui::Button("OUTPUT DISABLED", buttonSize)) {
      device->setOutput(1, true); // Channel 1
    }
  }
  ImGui::PopStyleColor(2);
}

void UISPDPowerPanel::RenderQuickPresets() {
  ImGui::Text("Quick Presets");
  ImGui::Spacing();

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device) return;

  ImVec2 buttonSize = ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 25);

  if (ImGui::Button("5V / 1A", buttonSize)) {
    device->setVoltage(1, 5.0);  // Channel 1
    device->setCurrent(1, 1.0);  // Channel 1
    m_currentMode = OperatingMode::CONSTANT_VOLTAGE;
  }
  ImGui::SameLine();
  if (ImGui::Button("12V / 0.5A", buttonSize)) {
    device->setVoltage(1, 12.0); // Channel 1
    device->setCurrent(1, 0.5);  // Channel 1
    m_currentMode = OperatingMode::CONSTANT_VOLTAGE;
  }

  if (ImGui::Button("24V / 0.2A", buttonSize)) {
    device->setVoltage(1, 24.0); // Channel 1
    device->setCurrent(1, 0.2);  // Channel 1
    m_currentMode = OperatingMode::CONSTANT_VOLTAGE;
  }
  ImGui::SameLine();
  if (ImGui::Button("3.3V / 2A", buttonSize)) {
    device->setVoltage(1, 3.3);  // Channel 1
    device->setCurrent(1, 2.0);  // Channel 1
    m_currentMode = OperatingMode::CONSTANT_VOLTAGE;
  }
}

void UISPDPowerPanel::RenderRightPanel() {
  ImGui::Text("Monitoring & Advanced");
  ImGui::Separator();

  if (m_selectedDeviceId.empty()) {
    ImGui::Text("Select a device for monitoring");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderAdvancedSettings();
    return;
  }

  // Device monitoring
  RenderDeviceMonitoring();

  ImGui::Separator();

  // Advanced settings
  RenderAdvancedSettings();
}

void UISPDPowerPanel::RenderDeviceMonitoring() {
  ImGui::Text("Device Status");
  ImGui::Spacing();

  ImGui::Text("Device: %s", m_selectedDeviceId.c_str());

  auto* device = m_spdManager.GetDevice(m_selectedDeviceId);
  if (!device || !device->isConnected()) {
    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.2f, 1.0f), "Status: Disconnected");
    return;
  }

  ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Status: Connected");

  // Get device status from the all statuses map
  auto statuses = m_spdManager.GetAllStatuses();
  if (statuses.find(m_selectedDeviceId) != statuses.end()) {
    ImGui::Text("Info: %s", statuses[m_selectedDeviceId].c_str());
  }

  // Output status
  auto outputState = device->getOutputState(1); // Channel 1
  bool outputEnabled = outputState.has_value() ? outputState.value() : false;
  ImGui::Text("Output: %s", outputEnabled ? "ENABLED" : "DISABLED");

  // Mode display
  ImGui::Text("Mode: %s", GetModeString(m_currentMode));

  ImGui::Spacing();
  ImGui::Text("Real-time Values:");

  // Get actual measurements
  auto voltage = device->getVoltage(1);   // Channel 1
  auto current = device->getCurrent(1);   // Channel 1

  if (voltage.has_value()) {
    ImGui::BulletText("Voltage: %.3f V (measured)", voltage.value());
  }
  else {
    ImGui::BulletText("Voltage: -- V (read error)");
  }

  if (current.has_value()) {
    ImGui::BulletText("Current: %.3f A (measured)", current.value());
  }
  else {
    ImGui::BulletText("Current: -- A (read error)");
  }

  // Calculate power if both values available
  if (voltage.has_value() && current.has_value()) {
    double power = voltage.value() * current.value();
    ImGui::BulletText("Power: %.3f W (calculated)", power);
  }
  else {
    ImGui::BulletText("Power: -- W (calculation error)");
  }

  ImGui::Spacing();
  ImGui::Text("Device Info:");

  // Get instrument ID if available
  try {
    std::string instrumentID = device->getInstrumentID();
    if (!instrumentID.empty()) {
      ImGui::BulletText("ID: %s", instrumentID.c_str());
    }
  }
  catch (...) {
    ImGui::BulletText("ID: Read error");
  }
}

void UISPDPowerPanel::RenderAdvancedSettings() {
  ImGui::Text("Global Settings");
  ImGui::Spacing();

  // Global voltage slider
  static float globalVoltage = 0.0f;
  if (ImGui::SliderFloat("Global Voltage", &globalVoltage, 0.0f, 30.0f, "%.2f V")) {
    m_spdManager.SetAllVoltages(globalVoltage);
  }

  // Global current slider
  static float globalCurrent = 0.5f;
  if (ImGui::SliderFloat("Global Current", &globalCurrent, 0.0f, 5.0f, "%.3f A")) {
    m_spdManager.SetAllCurrentLimits(globalCurrent);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("System Monitoring");

  // Polling controls
  static int pollingInterval = 1000;
  ImGui::SliderInt("Poll Rate (ms)", &pollingInterval, 100, 5000);

  if (!m_spdManager.IsPollingActive()) {
    if (ImGui::Button("Start Monitoring", ImVec2(-1, 0))) {
      m_spdManager.StartAllPolling(pollingInterval);
    }
  }
  else {
    if (ImGui::Button("Stop Monitoring", ImVec2(-1, 0))) {
      m_spdManager.StopAllPolling();
    }
  }

  ImGui::Spacing();

  // System information
  ImGui::Text("System Info:");
  ImGui::BulletText("Total Devices: %zu", m_spdManager.GetDeviceNames().size());
  ImGui::BulletText("Connected: %d", m_spdManager.GetConnectedCount());
  ImGui::BulletText("Polling: %s", m_spdManager.IsPollingActive() ? "Active" : "Stopped");
}

void UISPDPowerPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

const char* UISPDPowerPanel::GetModeString(OperatingMode mode) const {
  switch (mode) {
  case OperatingMode::CONSTANT_VOLTAGE: return "Constant Voltage (CV)";
  case OperatingMode::CONSTANT_CURRENT: return "Constant Current (CC)";
  default: return "Unknown";
  }
}

ImVec4 UISPDPowerPanel::GetModeColor(OperatingMode mode) const {
  switch (mode) {
  case OperatingMode::CONSTANT_VOLTAGE: return ImVec4(0.2f, 0.6f, 1.0f, 1.0f);  // Blue
  case OperatingMode::CONSTANT_CURRENT: return ImVec4(1.0f, 0.6f, 0.2f, 1.0f);  // Orange
  default: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
  }
}

// In UISPDPowerPanel.cpp - Add this method:
const UISPDPowerPanel::CachedDeviceStatus& UISPDPowerPanel::GetCachedStatus(const std::string& deviceId) {
  // Check if entry exists, create if not
  auto it = m_deviceStatusCache.find(deviceId);
  if (it == m_deviceStatusCache.end()) {
    // Create new entry with default values
    m_deviceStatusCache[deviceId] = CachedDeviceStatus();
  }

  // Now get reference to existing entry
  CachedDeviceStatus& cached = m_deviceStatusCache[deviceId];

  if (cached.needsUpdate()) {
    auto* device = m_spdManager.GetDevice(deviceId);
    if (device && device->isConnected()) {
      try {
        auto outputState = device->getOutputState(1);
        auto voltage = device->getVoltage(1);
        auto current = device->getCurrent(1);

        if (outputState.has_value() && voltage.has_value() && current.has_value()) {
          cached.outputEnabled = outputState.value();
          cached.voltage = voltage.value();
          cached.current = current.value();
          cached.isValid = true;
        }
        else {
          cached.isValid = false;
        }
      }
      catch (...) {
        cached.isValid = false;
      }
    }
    else {
      cached.isValid = false;
    }

    cached.lastUpdate = std::chrono::steady_clock::now();
  }

  return cached;
}