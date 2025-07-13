// IOPanelUI.cpp - Implementation of embedded IO panel UI
#include "IOPanelUI.h"
#include "include/eziio/EziIO_Manager.h"
#include "IOConfigManager.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <iomanip>

IOPanelUI::IOPanelUI(EziIOManager& ioManager)
  : m_ioManager(ioManager) {
  // Constructor - initialize device states
  RefreshDeviceStates();
}

IOPanelUI::~IOPanelUI() {
  // Destructor - no cleanup needed
}

void IOPanelUI::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Calculate content size for left/right panel layout
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.25f;
  float rightPanelWidth = contentSize.x * 0.75f;

  // Left Panel - IO Device List (25% width)
  ImGui::BeginChild("LeftIOPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Selected Device Interface (75% width)
  ImGui::BeginChild("RightIOPanel", ImVec2(rightPanelWidth, contentSize.y), false);
  RenderRightPanel();
  ImGui::EndChild();
}

void IOPanelUI::RenderLeftPanel() {
  ImGui::Text("IO Devices");
  ImGui::Separator();

  // Auto-refresh controls
  ImGui::Checkbox("Auto Refresh", &m_autoRefresh);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically refresh device states");
  }

  ImGui::SetNextItemWidth(-1);
  ImGui::SliderFloat("##refresh", &m_refreshInterval, 0.1f, 2.0f, "%.1fs");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Refresh interval");
  }

  if (ImGui::Button("Refresh Now", ImVec2(-1, 25))) {
    RefreshDeviceStates();
  }

  ImGui::Separator();

  // Debug mode toggle
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  ImGui::Checkbox("Debug Info", &m_showDebugInfo);
  ImGui::PopStyleColor();

  ImGui::Separator();

  // Update timer for auto-refresh
  if (m_autoRefresh) {
    m_refreshTimer += ImGui::GetIO().DeltaTime;
    if (m_refreshTimer >= m_refreshInterval) {
      RefreshDeviceStates();
      m_refreshTimer = 0.0f;
    }

    // Show countdown
    float remainingTime = m_refreshInterval - m_refreshTimer;
    ImGui::Text("Next: %.1fs", remainingTime);
  }

  ImGui::Separator();

  // Render device list
  RenderDeviceList();
}

void IOPanelUI::RenderRightPanel() {
  if (m_selectedDeviceName.empty()) {
    RenderNoSelectionMessage();
  }
  else {
    RenderSelectedDeviceUI();
  }
}

void IOPanelUI::RenderDeviceList() {
  for (const auto& device : m_deviceStates) {
    ImGui::PushID(device.name.c_str());

    // Connection status indicator
    ImVec4 statusColor = device.connected ?
      ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :  // Green for connected
      ImVec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red for disconnected

    // Status indicator circle
    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
    ImGui::Text("●");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Device selection button
    bool isSelected = (m_selectedDeviceName == device.name);
    if (isSelected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 0.8f));
    }

    std::string buttonLabel = device.name + "##select";
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 30))) {
      m_selectedDeviceName = device.name;
    }

    if (isSelected) {
      ImGui::PopStyleColor();
    }

    // Device info tooltip
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Device: %s", device.name.c_str());
      ImGui::Text("ID: %d", device.id);
      ImGui::Text("Inputs: %d", device.inputCount);
      ImGui::Text("Outputs: %d", device.outputCount);
      ImGui::Text("Status: %s", device.connected ? "Connected" : "Disconnected");
      ImGui::EndTooltip();
    }

    // Show connection status text
    ImGui::SetWindowFontScale(0.8f);
    ImGui::Text("ID:%d I:%d O:%d", device.id, device.inputCount, device.outputCount);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::PopID();
    ImGui::Spacing();
  }
}

void IOPanelUI::RenderSelectedDeviceUI() {
  // Find the selected device
  auto deviceIt = std::find_if(m_deviceStates.begin(), m_deviceStates.end(),
    [this](const DeviceState& dev) { return dev.name == m_selectedDeviceName; });

  if (deviceIt == m_deviceStates.end()) {
    ImGui::Text("Selected device not found!");
    return;
  }

  DeviceState& device = *deviceIt;

  // Render device interface
  RenderDeviceHeader(device);
  ImGui::Separator();

  RenderConnectionControls(device);
  ImGui::Separator();

  RenderInputPins(device);
  ImGui::Separator();

  RenderOutputPins(device);
  ImGui::Separator();

  RenderUtilityControls();
}

void IOPanelUI::RenderDeviceHeader(const DeviceState& device) {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("%s", device.name.c_str());
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Text("Device ID: %d", device.id);

  // Connection status with color
  ImVec4 statusColor = device.connected ?
    ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
    ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

  ImGui::Text("Status: ");
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
  ImGui::Text("%s", device.connected ? "Connected" : "Disconnected");
  ImGui::PopStyleColor();

  ImGui::Text("Inputs: %d, Outputs: %d", device.inputCount, device.outputCount);

  if (m_showDebugInfo) {
    ImGui::Text("Raw Input: 0x%08X", device.inputs);
    ImGui::Text("Raw Latch: 0x%08X", device.latch);
    ImGui::Text("Raw Output: 0x%08X", device.outputs);
    ImGui::Text("Output Status: 0x%08X", device.outputStatus);
  }
}

void IOPanelUI::RenderConnectionControls(const DeviceState& device) {
  ImGui::Text("Connection Controls");

  if (device.connected) {
    if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
      // Note: EziIOManager might not have individual disconnect methods
      // This would need to be implemented
      std::cout << "Disconnect device " << device.name << std::endl;
    }
  }
  else {
    if (ImGui::Button("Connect", ImVec2(100, 30))) {
      // Note: EziIOManager might not have individual connect methods
      // This would need to be implemented
      std::cout << "Connect device " << device.name << std::endl;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh Status", ImVec2(120, 30))) {
    RefreshDeviceStates();
  }
}

void IOPanelUI::RenderInputPins(const DeviceState& device) {
  ImGui::Text("Input Pins");

  if (device.inputCount == 0) {
    ImGui::Text("No input pins available");
    return;
  }

  // Create table for inputs
  if (ImGui::BeginTable("InputTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int i = 0; i < device.inputCount; i++) {
      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      ImGui::Text("Input %d", i);

      ImGui::TableNextColumn();
      bool pinState = IsPinOn(device.inputs, i);
      ImVec4 stateColor = pinState ?
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

      ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
      ImGui::Text("%s", pinState ? "ON" : "OFF");
      ImGui::PopStyleColor();

      ImGui::TableNextColumn();
      std::string pinName = GetPinName(device.name, true, i);
      ImGui::Text("%s", pinName.c_str());
    }

    ImGui::EndTable();
  }
}

void IOPanelUI::RenderOutputPins(DeviceState& device) {
  ImGui::Text("Output Pins");

  if (device.outputCount == 0) {
    ImGui::Text("No output pins available");
    return;
  }

  // Create table for outputs
  if (ImGui::BeginTable("OutputTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int i = 0; i < device.outputCount; i++) {
      ImGui::TableNextRow();
      ImGui::PushID(i);

      ImGui::TableNextColumn();
      ImGui::Text("Output %d", i);

      ImGui::TableNextColumn();
      uint32_t mask = GetOutputPinMask(device.name, i);
      bool pinState = (device.outputs & mask) != 0;

      ImVec4 stateColor = pinState ?
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

      ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
      ImGui::Text("%s", pinState ? "ON" : "OFF");
      ImGui::PopStyleColor();

      ImGui::TableNextColumn();

      // ON/OFF buttons
      if (ImGui::Button("ON", ImVec2(25, 20))) {
        if (m_ioManager.setOutput(device.id, i, true)) {
          std::cout << "Set " << device.name << " output " << i << " ON" << std::endl;
          // Update local state immediately
          device.outputs |= mask;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("OFF", ImVec2(25, 20))) {
        if (m_ioManager.setOutput(device.id, i, false)) {
          std::cout << "Set " << device.name << " output " << i << " OFF" << std::endl;
          // Update local state immediately
          device.outputs &= ~mask;
        }
      }

      ImGui::TableNextColumn();
      std::string pinName = GetPinName(device.name, false, i);
      ImGui::Text("%s", pinName.c_str());

      ImGui::PopID();
    }

    ImGui::EndTable();
  }
}

void IOPanelUI::RenderUtilityControls() {
  ImGui::Text("Utility Controls");

  if (ImGui::Button("Turn All Outputs OFF", ImVec2(180, 30))) {
    // Find the selected device and turn off all outputs
    auto deviceIt = std::find_if(m_deviceStates.begin(), m_deviceStates.end(),
      [this](const DeviceState& dev) { return dev.name == m_selectedDeviceName; });

    if (deviceIt != m_deviceStates.end()) {
      for (int i = 0; i < deviceIt->outputCount; i++) {
        m_ioManager.setOutput(deviceIt->id, i, false);
      }
      std::cout << "Turned off all outputs for " << m_selectedDeviceName << std::endl;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh Device", ImVec2(120, 30))) {
    RefreshDeviceStates();
  }
}

void IOPanelUI::RenderNoSelectionMessage() {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("IO Device Management");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select an IO device from the list on the left to view");
  ImGui::Text("its input/output pins and control interface.");

  ImGui::Spacing();
  ImGui::Text("Available devices: %zu", m_deviceStates.size());

  if (m_deviceStates.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No IO devices found!");
    ImGui::Text("Check that:");
    ImGui::BulletText("EziIO Manager is initialized");
    ImGui::BulletText("Devices are connected and powered");
    ImGui::BulletText("Network configuration is correct");
  }
}

void IOPanelUI::RefreshDeviceStates() {
  // Clear previous states
  m_deviceStates.clear();

  // Get all devices from the manager
  for (const auto& devicePtr : m_ioManager.getDevices()) {
    DeviceState state;
    state.name = devicePtr->getName();
    state.id = devicePtr->getDeviceId();
    state.inputCount = devicePtr->getInputCount();
    state.outputCount = devicePtr->getOutputCount();
    state.connected = devicePtr->isConnected();

    // Get cached input and output states
    uint32_t inputs = 0, latch = 0;
    uint32_t outputs = 0, outStatus = 0;

    bool inputSuccess = m_ioManager.getLastInputStatus(state.id, inputs, latch);
    bool outputSuccess = m_ioManager.getLastOutputStatus(state.id, outputs, outStatus);

    // Log the refresh operation if debug is enabled
    if (m_showDebugInfo) {
      std::cout << "[IOPanelUI] Refreshing device " << state.name << " (ID: " << state.id << ")" << std::endl;
      std::cout << "  Input status: " << (inputSuccess ? "Success" : "Failed")
        << " [0x" << std::hex << inputs << ", Latch: 0x" << latch << std::dec << "]" << std::endl;
      std::cout << "  Output status: " << (outputSuccess ? "Success" : "Failed")
        << " [0x" << std::hex << outputs << ", Status: 0x" << outStatus << std::dec << "]" << std::endl;
    }

    state.inputs = inputs;
    state.latch = latch;
    state.outputs = outputs;
    state.outputStatus = outStatus;

    // Add to our list
    m_deviceStates.push_back(state);
  }
}

bool IOPanelUI::IsPinOn(uint32_t value, int pin) const {
  if (pin < 32) {
    return (value & (1U << pin)) != 0;
  }
  return false;
}

uint32_t IOPanelUI::GetOutputPinMask(const std::string& deviceName, int pin) const {
  if (deviceName == "IOBottom" && pin < 16) {
    // 16-output module uses specific pin masks (starting from bit 16)
    return 0x10000 << pin;
  }
  else if (deviceName == "IOTop" && pin < 8) {
    // 8-output module uses different masks (starting from bit 8)
    return 0x100 << pin;
  }
  else {
    // Fallback to standard bit pattern
    return 1U << pin;
  }
}

std::string IOPanelUI::GetPinName(const std::string& deviceName, bool isInput, int pin) const {
  if (m_configManager != nullptr) {
    return m_configManager->getPinName(deviceName, isInput, pin);
  }

  // Default behavior if no config manager is set
  return std::string("Pin ") + std::to_string(pin);
}

void IOPanelUI::ToggleWindow() {
  m_showWindow = !m_showWindow;
}