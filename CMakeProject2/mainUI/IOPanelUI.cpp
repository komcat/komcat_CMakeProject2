// IOPanelUI.cpp - Implementation of embedded IO panel UI with Observer Pattern
#include "IOPanelUI.h"
#include "include/eziio/EziIO_Manager.h"
#include "IOConfigManager.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>

IOPanelUI::IOPanelUI(EziIOManager& ioManager)
  : m_ioManager(ioManager) {
  // Constructor - initialize device states
  RefreshDeviceStates();

  // Initialize statistics
  m_totalConnectedDevices = m_ioManager.getConnectedDeviceCount();

  // Subscribe to IO events by default
  SubscribeToIOEvents();
}

IOPanelUI::~IOPanelUI() {
  // Unsubscribe from events
  UnsubscribeFromIOEvents();
}

void IOPanelUI::SubscribeToIOEvents() {
  if (!m_isSubscribed) {
    // Subscribe this UI as an observer to all devices
    auto self = std::shared_ptr<IEziIOObserver>(this, [](IEziIOObserver*) {});
    m_ioManager.subscribeToAllDevices(self);

    m_isSubscribed = true;
    AddNotification("Subscribed to live IO updates", false);
  }
}

void IOPanelUI::UnsubscribeFromIOEvents() {
  if (m_isSubscribed) {
    // Note: Actual unsubscribe would need to be implemented in EziIOManager
    // For now, we just disable live updates
    m_liveUpdatesEnabled = false;
    m_isSubscribed = false;
    AddNotification("Unsubscribed from live IO updates", false);
  }
}

void IOPanelUI::onPinStateChanged(const PinChangeEvent& event) {
  if (!m_liveUpdatesEnabled) {
    return;
  }

  // Thread-safe update of device state
  UpdateDeviceStateFromEvent(event);

  // Add to history
  AddToPinHistory(event);

  // Update statistics
  m_totalPinChanges++;
  m_deviceChangeCount[event.deviceName]++;

  // Show notification for important changes (optional)
  if (m_showDebugInfo) {
    std::stringstream ss;
    ss << event.deviceName << " "
      << (event.isInput ? "IN" : "OUT") << event.pinNumber
      << " → " << (event.newState ? "ON" : "OFF");
    AddNotification(ss.str(), false);
  }
}

void IOPanelUI::UpdateDeviceStateFromEvent(const PinChangeEvent& event) {
  std::lock_guard<std::mutex> lock(m_deviceStatesMutex);

  // Find the device in our state cache
  auto deviceIt = std::find_if(m_deviceStates.begin(), m_deviceStates.end(),
    [&event](const DeviceState& dev) { return dev.name == event.deviceName; });

  if (deviceIt != m_deviceStates.end()) {
    deviceIt->lastUpdateTime = std::chrono::steady_clock::now();

    if (event.isInput) {
      // Update input pin state
      if (event.newState) {
        deviceIt->inputs |= (1U << event.pinNumber);
      }
      else {
        deviceIt->inputs &= ~(1U << event.pinNumber);
      }

      // Track recently changed pins for highlighting
      deviceIt->recentlyChangedInputPins.push_back(event.pinNumber);
      if (deviceIt->recentlyChangedInputPins.size() > 10) {
        deviceIt->recentlyChangedInputPins.erase(deviceIt->recentlyChangedInputPins.begin());
      }
    }
    else {
      // Update output pin state
      uint32_t mask = GetOutputPinMask(event.deviceName, event.pinNumber);
      if (event.newState) {
        deviceIt->outputs |= mask;
      }
      else {
        deviceIt->outputs &= ~mask;
      }

      // Track recently changed pins
      deviceIt->recentlyChangedOutputPins.push_back(event.pinNumber);
      if (deviceIt->recentlyChangedOutputPins.size() > 10) {
        deviceIt->recentlyChangedOutputPins.erase(deviceIt->recentlyChangedOutputPins.begin());
      }
    }
  }
}

void IOPanelUI::AddToPinHistory(const PinChangeEvent& event) {
  PinChangeHistory history;
  history.deviceName = event.deviceName;
  history.pinNumber = event.pinNumber;
  history.isInput = event.isInput;
  history.newState = event.newState;
  history.timestamp = std::chrono::steady_clock::now();

  m_pinChangeHistory.push_back(history);

  // Limit history size
  if (m_pinChangeHistory.size() > m_maxHistorySize) {
    m_pinChangeHistory.erase(m_pinChangeHistory.begin());
  }
}

void IOPanelUI::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Update notifications
  UpdateNotifications(ImGui::GetIO().DeltaTime);

  // Render error notifications at the top
  RenderErrorNotifications();

  // Render live update indicator
  RenderLiveUpdateIndicator();

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

void IOPanelUI::RenderLiveUpdateIndicator() {
  if (m_liveUpdatesEnabled && m_isSubscribed) {
    // Animated indicator for live updates
    static float pulse = 0.0f;
    pulse += ImGui::GetIO().DeltaTime * 2.0f;
    float alpha = 0.5f + 0.5f * sinf(pulse);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, alpha));
    ImGui::Text(reinterpret_cast<const char*>(u8"● LIVE"));
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Receiving live pin status updates\nTotal changes: %d", m_totalPinChanges);
    }
  }
}

void IOPanelUI::RenderLeftPanel() {
  ImGui::Text("IO Devices");
  ImGui::Separator();

  // Show connection statistics
  ImGui::Text("Connected: %d/%zu", m_totalConnectedDevices, m_deviceStates.size());

  // Show polling status
  if (m_ioManager.isPolling()) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"● Polling Active"));
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"● Polling Inactive"));
  }

  // Live updates toggle
  if (ImGui::Checkbox("Live Updates", &m_liveUpdatesEnabled)) {
    if (m_liveUpdatesEnabled && !m_isSubscribed) {
      SubscribeToIOEvents();
    }
    AddNotification(m_liveUpdatesEnabled ? "Live updates enabled" : "Live updates disabled", false);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enable real-time pin change notifications");
  }

  ImGui::Separator();

  // Auto-refresh controls (disabled when live updates are on)
  ImGui::BeginDisabled(m_liveUpdatesEnabled);
  ImGui::Checkbox("Auto Refresh", &m_autoRefresh);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically refresh device states\n(Disabled when live updates are active)");
  }

  ImGui::SetNextItemWidth(-1);
  ImGui::SliderFloat("##refresh", &m_refreshInterval, 0.1f, 2.0f, "%.1fs");
  ImGui::EndDisabled();

  if (ImGui::Button("Manual Refresh", ImVec2(-1, 25))) {
    RefreshDeviceStates();
    AddNotification("Manual refresh completed", false);
  }

  ImGui::Separator();

  // Debug and error notification toggles
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  ImGui::Checkbox("Debug Info", &m_showDebugInfo);
  ImGui::PopStyleColor();

  ImGui::Checkbox("Show Errors", &m_showErrorNotifications);

  ImGui::Separator();

  // Update timer for auto-refresh (only when live updates are off)
  if (m_autoRefresh && !m_liveUpdatesEnabled) {
    m_refreshTimer += ImGui::GetIO().DeltaTime;
    if (m_refreshTimer >= m_refreshInterval) {
      RefreshDeviceStates();
      m_refreshTimer = 0.0f;
    }

    // Show countdown
    float remainingTime = m_refreshInterval - m_refreshTimer;
    ImGui::Text("Next: %.1fs", remainingTime);
  }
  else if (m_liveUpdatesEnabled) {
    ImGui::Text("Live: %d changes", m_totalPinChanges);
  }

  ImGui::Separator();

  // Render statistics
  RenderStatistics();

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

  // Show pin change history if debug mode is on
  if (m_showDebugInfo && !m_pinChangeHistory.empty()) {
    ImGui::Separator();
    RenderPinChangeHistory();
  }
}

void IOPanelUI::RenderPinChangeHistory() {
  ImGui::Text("Recent Pin Changes (Last %zu)", (std::min)(m_pinChangeHistory.size(), size_t(10)));

  if (ImGui::BeginTable("PinHistory", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableHeadersRow();

    // Show last 10 changes
    size_t start = m_pinChangeHistory.size() > 10 ? m_pinChangeHistory.size() - 10 : 0;
    for (size_t i = start; i < m_pinChangeHistory.size(); i++) {
      const auto& history = m_pinChangeHistory[i];

      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      ImGui::Text("%s", history.deviceName.c_str());

      ImGui::TableNextColumn();
      ImGui::Text("%s", history.isInput ? "IN" : "OUT");

      ImGui::TableNextColumn();
      ImGui::Text("%d", history.pinNumber);

      ImGui::TableNextColumn();
      ImVec4 color = history.newState ?
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
      ImGui::TextColored(color, "%s", history.newState ? "ON" : "OFF");
    }

    ImGui::EndTable();
  }
}

void IOPanelUI::RenderDeviceList() {
  std::lock_guard<std::mutex> lock(m_deviceStatesMutex);

  for (const auto& device : m_deviceStates) {
    ImGui::PushID(device.name.c_str());

    // Determine status color based on connection and errors
    ImVec4 statusColor;
    if (!device.connected) {
      statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red for disconnected
    }
    else if (device.lastInputError != EziIOError::SUCCESS ||
      device.lastOutputError != EziIOError::SUCCESS) {
      statusColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange for errors
    }
    else {
      statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green for OK
    }

    // Status indicator circle
    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
    ImGui::Text(reinterpret_cast<const char*>(u8"●"));
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Show change count if live updates are enabled
    if (m_liveUpdatesEnabled && m_deviceChangeCount[device.name] > 0) {
      ImGui::Text("(%d)", m_deviceChangeCount[device.name]);
      ImGui::SameLine();
    }

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

      if (m_liveUpdatesEnabled) {
        ImGui::Text("Changes: %d", m_deviceChangeCount[device.name]);
      }

      if (device.lastInputError != EziIOError::SUCCESS) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
          "Input Error: %s", GetErrorString(device.lastInputError).c_str());
      }
      if (device.lastOutputError != EziIOError::SUCCESS) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
          "Output Error: %s", GetErrorString(device.lastOutputError).c_str());
      }
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

void IOPanelUI::RenderInputPins(const DeviceState& device) {
  ImGui::Text("Input Pins");

  if (device.inputCount == 0) {
    ImGui::Text("No input pins available");
    return;
  }

  if (!device.connected) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Device must be connected to read inputs");
    return;
  }

  // Create table for inputs
  if (ImGui::BeginTable("InputTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Latch", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int i = 0; i < device.inputCount; i++) {
      ImGui::TableNextRow();

      // Highlight recently changed pins
      HighlightRecentChange(device, i, true);

      ImGui::TableNextColumn();
      ImGui::Text("In %d", i);

      ImGui::TableNextColumn();
      bool pinState = IsPinOn(device.inputs, i);
      ImVec4 stateColor = pinState ?
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

      ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
      ImGui::Text("%s", pinState ? "ON" : "OFF");
      ImGui::PopStyleColor();

      ImGui::TableNextColumn();
      bool latchState = IsPinOn(device.latch, i);
      if (latchState) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "YES");
      }
      else {
        ImGui::Text("NO");
      }

      ImGui::TableNextColumn();
      std::string pinName = GetPinName(device.name, true, i);
      ImGui::Text("%s", pinName.c_str());
    }

    ImGui::EndTable();
  }
}

// IOPanelUI.cpp - Fix for immediate state update after button click

void IOPanelUI::RenderOutputPins(DeviceState& device) {
  ImGui::Text("Output Pins");

  if (device.outputCount == 0) {
    ImGui::Text("No output pins available");
    return;
  }

  if (!device.connected) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Device must be connected to control outputs");
    return;
  }

  // Create table for outputs
  if (ImGui::BeginTable("OutputTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int i = 0; i < device.outputCount; i++) {
      ImGui::TableNextRow();
      ImGui::PushID(i);

      ImGui::TableNextColumn();
      ImGui::Text("Out %d", i);

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

      // ON button
      if (ImGui::Button("ON", ImVec2(50, 20))) {
        EziIOError result = m_ioManager.setOutput(device.id, i, true);
        if (result == EziIOError::SUCCESS) {
          // CRITICAL FIX: Update the UI state immediately
          // The mask calculation needs to match what the hardware expects
          device.outputs |= mask;

          // Force immediate hardware read to get actual state
          RefreshSingleDevice(device);

          if (m_showDebugInfo) {
            std::cout << "Set " << device.name << " output " << i << " ON" << std::endl;
          }
        }
        else {
          device.lastOutputError = result;
          m_totalOperationErrors++;
          AddNotification("Failed to set output: " + GetErrorString(result), true);
        }
      }
      ShowErrorTooltip(device.lastOutputError);

      ImGui::SameLine();

      // OFF button
      if (ImGui::Button("OFF", ImVec2(50, 20))) {
        EziIOError result = m_ioManager.setOutput(device.id, i, false);
        if (result == EziIOError::SUCCESS) {
          // CRITICAL FIX: Update the UI state immediately
          device.outputs &= ~mask;

          // Force immediate hardware read to get actual state
          RefreshSingleDevice(device);

          if (m_showDebugInfo) {
            std::cout << "Set " << device.name << " output " << i << " OFF" << std::endl;
          }
        }
        else {
          device.lastOutputError = result;
          m_totalOperationErrors++;
          AddNotification("Failed to clear output: " + GetErrorString(result), true);
        }
      }
      ShowErrorTooltip(device.lastOutputError);

      ImGui::TableNextColumn();
      std::string pinName = GetPinName(device.name, false, i);
      ImGui::Text("%s", pinName.c_str());

      ImGui::PopID();
    }

    ImGui::EndTable();
  }
}



void IOPanelUI::HighlightRecentChange(const DeviceState& device, int pin, bool isInput) const {
  // Check if this pin was recently changed
  const auto& recentPins = isInput ? device.recentlyChangedInputPins : device.recentlyChangedOutputPins;

  if (std::find(recentPins.begin(), recentPins.end(), pin) != recentPins.end()) {
    // Calculate fade based on time since last update
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - device.lastUpdateTime).count();

    if (elapsed < 1000) {  // Highlight for 1 second
      float alpha = 1.0f - (elapsed / 1000.0f);
      ImVec4 highlightColor(1.0f, 1.0f, 0.0f, alpha * 0.3f);

      // Draw highlight background
      ImVec2 min = ImGui::GetCursorScreenPos();
      ImVec2 max = ImVec2(min.x + ImGui::GetContentRegionAvail().x, min.y + ImGui::GetTextLineHeight());
      ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(highlightColor));
    }
  }
}

void IOPanelUI::RenderStatistics() {
  ImGui::Text("Statistics");
  ImGui::Text("Refreshes: %d", m_refreshCount);
  ImGui::Text("Errors: %d", m_totalOperationErrors);

  if (m_liveUpdatesEnabled) {
    ImGui::Text("Pin Changes: %d", m_totalPinChanges);
  }

  if (m_ioManager.isPolling()) {
    ImGui::Text("Polling: Active");
  }
  else {
    ImGui::Text("Polling: Stopped");
  }
}

// Keep all other existing methods unchanged...
void IOPanelUI::RenderSelectedDeviceUI() {
  // Find the selected device
  std::lock_guard<std::mutex> lock(m_deviceStatesMutex);

  auto deviceIt = std::find_if(m_deviceStates.begin(), m_deviceStates.end(),
    [this](const DeviceState& dev) { return dev.name == m_selectedDeviceName; });

  if (deviceIt == m_deviceStates.end()) {
    ImGui::Text("Selected device not found!");
    m_selectedDeviceName.clear();
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

// Include all other unchanged methods from the original file...
// (RenderDeviceHeader, RenderConnectionControls, RenderUtilityControls, 
//  RenderNoSelectionMessage, RenderErrorNotifications, RefreshDeviceStates,
//  AddNotification, UpdateNotifications, ShowErrorTooltip, GetErrorString,
//  IsPinOn, GetOutputPinMask, GetPinName, ToggleWindow)

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

  // Show error status if any
  if (device.lastInputError != EziIOError::SUCCESS ||
    device.lastOutputError != EziIOError::SUCCESS) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"⚠ Device has errors"));

    if (device.lastInputError != EziIOError::SUCCESS) {
      ImGui::Text("Input Error: %s", GetErrorString(device.lastInputError).c_str());
    }
    if (device.lastOutputError != EziIOError::SUCCESS) {
      ImGui::Text("Output Error: %s", GetErrorString(device.lastOutputError).c_str());
    }
  }

  ImGui::Text("Inputs: %d, Outputs: %d", device.inputCount, device.outputCount);

  if (m_showDebugInfo) {
    ImGui::Text("Raw Input: 0x%08X", device.inputs);
    ImGui::Text("Raw Latch: 0x%08X", device.latch);
    ImGui::Text("Raw Output: 0x%08X", device.outputs);
    ImGui::Text("Output Status: 0x%08X", device.outputStatus);
    ImGui::Text("Total Refreshes: %d", m_refreshCount);
    if (m_liveUpdatesEnabled) {
      ImGui::Text("Device Changes: %d", m_deviceChangeCount[device.name]);
    }
  }
}

void IOPanelUI::RenderConnectionControls(DeviceState& device) {
  ImGui::Text("Connection Controls");

  if (device.connected) {
    if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
      if (m_ioManager.disconnectDevice(device.id)) {
        device.connected = false;
        AddNotification("Disconnected " + device.name, false);
        RefreshDeviceStates();
      }
      else {
        AddNotification("Failed to disconnect " + device.name, true);
      }
    }
  }
  else {
    if (ImGui::Button("Connect", ImVec2(100, 30))) {
      if (m_ioManager.connectDevice(device.id)) {
        device.connected = true;
        AddNotification("Connected " + device.name, false);
        RefreshDeviceStates();
      }
      else {
        AddNotification("Failed to connect " + device.name, true);
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh Status", ImVec2(120, 30))) {
    RefreshDeviceStates();
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear Errors", ImVec2(100, 30))) {
    device.lastInputError = EziIOError::SUCCESS;
    device.lastOutputError = EziIOError::SUCCESS;
    device.lastErrorMessage.clear();
    m_totalOperationErrors = 0;
  }
}

void IOPanelUI::RenderUtilityControls() {
  ImGui::Text("Utility Controls");

  if (ImGui::Button("Turn All Outputs OFF", ImVec2(180, 30))) {
    std::lock_guard<std::mutex> lock(m_deviceStatesMutex);

    auto deviceIt = std::find_if(m_deviceStates.begin(), m_deviceStates.end(),
      [this](const DeviceState& dev) { return dev.name == m_selectedDeviceName; });

    if (deviceIt != m_deviceStates.end()) {
      int successCount = 0;
      int errorCount = 0;

      for (int i = 0; i < deviceIt->outputCount; i++) {
        EziIOError result = m_ioManager.setOutput(deviceIt->id, i, false);
        if (result == EziIOError::SUCCESS) {
          successCount++;
        }
        else {
          errorCount++;
          deviceIt->lastOutputError = result;
        }
      }

      if (errorCount > 0) {
        AddNotification("Failed to turn off " + std::to_string(errorCount) + " outputs", true);
      }
      else {
        AddNotification("All outputs turned OFF", false);
      }

      if (!m_liveUpdatesEnabled) {
        RefreshDeviceStates();
      }
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
  ImGui::Text("Connected devices: %d", m_totalConnectedDevices);

  if (m_liveUpdatesEnabled) {
    ImGui::Text("Live updates: ENABLED");
    ImGui::Text("Total pin changes: %d", m_totalPinChanges);
  }

  if (m_deviceStates.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No IO devices found!");
    ImGui::Text("Check that:");
    ImGui::BulletText("EziIO Manager is initialized");
    ImGui::BulletText("Devices are connected and powered");
    ImGui::BulletText("Network configuration is correct");
  }
}

void IOPanelUI::RenderErrorNotifications() {
  if (!m_showErrorNotifications || m_errorNotifications.empty()) {
    return;
  }

  // Render notifications in the top-right corner
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 work_pos = viewport->WorkPos;
  ImVec2 work_size = viewport->WorkSize;

  float y_offset = 50.0f;

  for (const auto& notif : m_errorNotifications) {
    ImVec2 window_pos = ImVec2(work_pos.x + work_size.x - 350.0f, work_pos.y + y_offset);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    ImVec4 bg_color = notif.isError ?
      ImVec4(0.8f, 0.2f, 0.2f, 0.9f) :
      ImVec4(0.2f, 0.6f, 0.2f, 0.9f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);
    ImGui::Begin(("##notification" + std::to_string(y_offset)).c_str(), nullptr, flags);

    ImGui::Text("%s", notif.message.c_str());
    ImGui::Text("(%.1fs)", notif.timeRemaining);

    ImGui::End();
    ImGui::PopStyleColor();

    y_offset += 60.0f;
  }
}

void IOPanelUI::RefreshDeviceStates() {
  std::lock_guard<std::mutex> lock(m_deviceStatesMutex);

  m_deviceStates.clear();
  m_refreshCount++;

  for (const auto& devicePtr : m_ioManager.getDevices()) {
    DeviceState state;
    state.name = devicePtr->getName();
    state.id = devicePtr->getDeviceId();
    state.inputCount = devicePtr->getInputCount();
    state.outputCount = devicePtr->getOutputCount();
    state.connected = devicePtr->isConnected();
    state.lastUpdateTime = std::chrono::steady_clock::now();

    // Initialize error states
    state.lastInputError = EziIOError::SUCCESS;
    state.lastOutputError = EziIOError::SUCCESS;

    // Get cached input and output states with error handling
    uint32_t inputs = 0, latch = 0;
    uint32_t outputs = 0, outStatus = 0;

    state.lastInputError = m_ioManager.getLastInputStatus(state.id, inputs, latch);
    state.lastOutputError = m_ioManager.getLastOutputStatus(state.id, outputs, outStatus);

    // Log the refresh operation if debug is enabled
    if (m_showDebugInfo) {
      std::cout << "[IOPanelUI] Refreshing device " << state.name << " (ID: " << state.id << ")" << std::endl;
      std::cout << "  Input status: " << GetErrorString(state.lastInputError)
        << " [0x" << std::hex << inputs << ", Latch: 0x" << latch << std::dec << "]" << std::endl;
      std::cout << "  Output status: " << GetErrorString(state.lastOutputError)
        << " [0x" << std::hex << outputs << ", Status: 0x" << outStatus << std::dec << "]" << std::endl;
    }

    state.inputs = inputs;
    state.latch = latch;
    state.outputs = outputs;
    state.outputStatus = outStatus;

    m_deviceStates.push_back(state);
  }

  // Update statistics
  m_totalConnectedDevices = m_ioManager.getConnectedDeviceCount();
}

void IOPanelUI::AddNotification(const std::string& message, bool isError) {
  if (!m_showErrorNotifications) {
    return;
  }

  ErrorNotification notif;
  notif.message = message;
  notif.timeRemaining = 3.0f;  // Show for 3 seconds
  notif.isError = isError;

  m_errorNotifications.push_back(notif);

  // Limit notifications to 5
  if (m_errorNotifications.size() > 5) {
    m_errorNotifications.erase(m_errorNotifications.begin());
  }
}

void IOPanelUI::UpdateNotifications(float deltaTime) {
  for (auto it = m_errorNotifications.begin(); it != m_errorNotifications.end();) {
    it->timeRemaining -= deltaTime;
    if (it->timeRemaining <= 0) {
      it = m_errorNotifications.erase(it);
    }
    else {
      ++it;
    }
  }
}

void IOPanelUI::ShowErrorTooltip(EziIOError error) {
  if (error != EziIOError::SUCCESS && ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    ImGui::Text("Error: %s", GetErrorString(error).c_str());
    ImGui::PopStyleColor();
    ImGui::EndTooltip();
  }
}

std::string IOPanelUI::GetErrorString(EziIOError error) const {
  return EziIOManager::getErrorString(error);
}

bool IOPanelUI::IsPinOn(uint32_t value, int pin) const {
  if (pin < 32) {
    return (value & (1U << pin)) != 0;
  }
  return false;
}

uint32_t IOPanelUI::GetOutputPinMask(const std::string& deviceName, int pin) const {
  if (deviceName == "IOBottom" && pin < 16) {
    return 0x10000 << pin;
  }
  else if (deviceName == "IOTop" && pin < 8) {
    return 0x100 << pin;
  }
  else {
    return 1U << pin;
  }
}

std::string IOPanelUI::GetPinName(const std::string& deviceName, bool isInput, int pin) const {
  if (m_configManager != nullptr) {
    return m_configManager->getPinName(deviceName, isInput, pin);
  }
  return std::string("Pin ") + std::to_string(pin);
}

void IOPanelUI::ToggleWindow() {
  m_showWindow = !m_showWindow;
}


// Add this helper method to refresh a single device immediately
void IOPanelUI::RefreshSingleDevice(DeviceState& device) {
  // Read actual hardware state immediately
  uint32_t outputs = 0, outStatus = 0;
  EziIOError outputError = m_ioManager.getOutputs(device.id, outputs, outStatus);

  if (outputError == EziIOError::SUCCESS) {
    device.outputs = outputs;
    device.outputStatus = outStatus;

    if (m_showDebugInfo) {
      std::cout << "[RefreshSingleDevice] " << device.name
        << " outputs: 0x" << std::hex << outputs << std::dec << std::endl;
    }
  }

  // Also refresh inputs while we're at it
  uint32_t inputs = 0, latch = 0;
  EziIOError inputError = m_ioManager.getLastInputStatus(device.id, inputs, latch);

  if (inputError == EziIOError::SUCCESS) {
    device.inputs = inputs;
    device.latch = latch;
  }
}