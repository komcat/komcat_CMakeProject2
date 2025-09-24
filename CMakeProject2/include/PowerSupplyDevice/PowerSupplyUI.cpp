// PowerSupplyUI.cpp
#include "PowerSupplyUI.h"
#include "logger.h"
#include <chrono>

PowerSupplyUI::PowerSupplyUI() {
  // Don't create manager here - will be set externally
  Logger::GetInstance()->LogInfo("PowerSupplyUI created");
}

PowerSupplyUI::~PowerSupplyUI() {
  // Don't cleanup manager since we don't own it
  // MachineOperations will handle cleanup
}

void PowerSupplyUI::SetPowerSupplyManager(PowerSupplyManager* manager) {
  m_manager = manager;

  if (m_manager) {
    RefreshDeviceList();
    Logger::GetInstance()->LogInfo("PowerSupplyUI connected to PowerSupplyManager with " +
      std::to_string(m_deviceIds.size()) + " devices");
  }
}

bool PowerSupplyUI::Initialize(const std::string& configFile) {
  if (!m_manager) {
    Logger::GetInstance()->LogError("PowerSupplyUI: No manager set before initialization");
    return false;
  }

  // Initialize manager with config
  bool result = m_manager->Initialize(configFile);

  if (result) {
    RefreshDeviceList();
    Logger::GetInstance()->LogInfo("PowerSupplyUI initialized with " +
      std::to_string(m_deviceIds.size()) + " devices");
  }

  return result;
}

void PowerSupplyUI::Render() {
  if (!m_showWindow) return;

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(m_windowName.c_str(), &m_showWindow)) {
    // Check if manager is available
    if (!m_manager) {
      ImGui::Text("Power Supply Manager not connected");
      ImGui::Text("Waiting for initialization...");
      ImGui::Separator();
      ImGui::Text("This typically means:");
      ImGui::BulletText("MachineOperations hasn't initialized PowerSupplyManager");
      ImGui::BulletText("Configuration file hasn't been loaded");

      if (ImGui::Button("Retry Connection")) {
        RefreshDeviceList();
      }
    }
    else {
      // Header with quick controls
      RenderQuickControls();
      ImGui::Separator();

      // Main content in two columns
      ImGui::Columns(2, "MainColumns", true);

      // Left column - Device List
      ImGui::Text("Devices");
      ImGui::Separator();
      RenderDeviceList();

      ImGui::NextColumn();

      // Right column - Device Control
      ImGui::Text("Control Panel");
      ImGui::Separator();
      if (!m_selectedDevice.empty()) {
        RenderDeviceControl(m_selectedDevice);
      }
      else {
        ImGui::Text("Select a device to control");
      }

      ImGui::Columns(1);

      // Auto-update status
      m_updateTimer += ImGui::GetIO().DeltaTime;
      if (m_updateTimer >= m_updateInterval) {
        UpdateDeviceStatus();
        m_updateTimer = 0.0f;
      }
    }
  }
  ImGui::End();
}

void PowerSupplyUI::RenderQuickControls() {
  if (!m_manager) return;

  if (ImGui::Button("Refresh")) {
    RefreshDeviceList();
    UpdateDeviceStatus();
  }

  ImGui::SameLine();
  if (ImGui::Button("Connect All")) {
    auto result = m_manager->ConnectAllDevices();
    Logger::GetInstance()->LogInfo("Connected " + std::to_string(result.successCount) +
      "/" + std::to_string(m_deviceIds.size()) + " devices");
    UpdateDeviceStatus();
  }

  ImGui::SameLine();
  if (ImGui::Button("All OFF")) {
    m_manager->TurnOffAll();
    for (auto& [id, control] : m_deviceControls) {
      control.outputOn = false;
    }
  }

  ImGui::SameLine();
  ImGui::Text("| Connected: %d/%zu",
    [this]() {
    int count = 0;
    for (const auto& id : m_deviceIds) {
      if (m_manager->IsDeviceConnected(id)) count++;
    }
    return count;
  }(),
    m_deviceIds.size());
}

void PowerSupplyUI::RenderDeviceList() {
  if (!m_manager) return;

  ImGui::BeginChild("DeviceList", ImVec2(0, 0), true);

  for (const auto& deviceId : m_deviceIds) {
    auto status = m_manager->GetDeviceStatus(deviceId);
    auto device = m_manager->GetDevice(deviceId);

    ImGui::PushID(deviceId.c_str());

    // Device header
    bool isSelected = (m_selectedDevice == deviceId);
    if (ImGui::Selectable(deviceId.c_str(), isSelected)) {
      m_selectedDevice = deviceId;
    }

    // Status indicators
    ImGui::Indent();

    // Connection status
    ImGui::Text("Status: %s", status.connected ? "Connected" : "Disconnected");

    // Show device info if available
    if (device && status.connected) {
      auto info = device->GetDeviceInfo();
      ImGui::Text("Model: %s", info.model.c_str());

      // Current measurements
      ImGui::Text("Output: %.3f V, %.3f A",
        status.lastMeasurement.voltage,
        status.lastMeasurement.current);

      // Output state for channels
      for (const auto& [ch, on] : status.channelOutputOn) {
        ImGui::SameLine();
        if (on) {
          ImGui::TextColored(ImVec4(0, 1, 0, 1), "CH%d: ON", ch);
        }
        else {
          ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "CH%d: OFF", ch);
        }
      }
    }

    ImGui::Unindent();
    ImGui::PopID();

    ImGui::Separator();
  }

  ImGui::EndChild();
}

void PowerSupplyUI::RenderDeviceControl(const std::string& deviceId) {
  if (!m_manager || !m_manager->HasDevice(deviceId)) {
    ImGui::Text("Device not found");
    return;
  }

  auto status = m_manager->GetDeviceStatus(deviceId);
  auto device = m_manager->GetDevice(deviceId);

  // Device Information Section
  ImGui::Text("Device Information");
  ImGui::Separator();
  ImGui::Text("Device ID: %s", deviceId.c_str());

  // Show device model and status
  if (device && status.connected) {
    auto info = device->GetDeviceInfo();
    ImGui::Text("Model: %s", info.model.c_str());
    ImGui::Text("Serial: %s", info.serialNumber.c_str());
  }

  ImGui::Text("Status: %s", status.connected ? "Connected" : "Disconnected");

  // Show channel states
  if (!status.channelOutputOn.empty()) {
    ImGui::Text("Channels: ");
    ImGui::SameLine();
    for (const auto& [ch, on] : status.channelOutputOn) {
      if (on) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "CH%d:ON ", ch);
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "CH%d:OFF ", ch);
      }
      ImGui::SameLine();
    }
    ImGui::NewLine();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Connection control
  if (!status.connected) {
    if (ImGui::Button("Connect", ImVec2(100, 30))) {
      if (m_manager->ConnectDevice(deviceId)) {
        Logger::GetInstance()->LogInfo("Connected to " + deviceId);
        UpdateDeviceStatus();
      }
    }
    ImGui::Text("Device is disconnected");
    return;
  }
  else {
    if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
      m_manager->DisconnectDevice(deviceId);
      m_deviceControls[deviceId].outputOn = false;
      UpdateDeviceStatus();
    }
  }

  ImGui::Separator();

  // Get or create control structure
  auto& control = m_deviceControls[deviceId];

  // Channel selection
  ImGui::Text("Channel Control");
  ImGui::InputInt("Channel", &control.channel);
  control.channel = (std::max)(1, control.channel);

  ImGui::Separator();

  // Voltage control
  ImGui::Text("Voltage Setting");
  ImGui::PushID("VoltageControl");  // Add unique ID scope
  ImGui::DragFloat("Voltage (V)", &control.voltage, 0.01f, 0.0f, 30.0f, "%.3f");
  ImGui::SameLine();
  if (ImGui::Button("Set V")) {
    if (m_manager->SetVoltage(deviceId, control.voltage, control.channel)) {
      Logger::GetInstance()->LogInfo("Set voltage to " + std::to_string(control.voltage) + "V");
    }
  }
  ImGui::PopID();  // End ID scope

  // Current control
  ImGui::Text("Current Limit");
  ImGui::PushID("CurrentControl");  // Add unique ID scope
  ImGui::DragFloat("Current (A)", &control.current, 0.001f, 0.0f, 5.0f, "%.3f");
  ImGui::SameLine();
  if (ImGui::Button("Set I")) {
    if (m_manager->SetCurrent(deviceId, control.current, control.channel)) {
      Logger::GetInstance()->LogInfo("Set current to " + std::to_string(control.current) + "A");
    }
  }
  ImGui::PopID();  // End ID scope

  ImGui::Separator();

  // Output control
  ImGui::Text("Output Control");

  bool outputOn = status.channelOutputOn[control.channel];

  if (outputOn) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Turn OFF", ImVec2(100, 30))) {
      m_manager->TurnOff(deviceId, control.channel);
      control.outputOn = false;
    }
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    if (ImGui::Button("Turn ON", ImVec2(100, 30))) {
      // Set voltage and current before turning on
      m_manager->SetVoltage(deviceId, control.voltage, control.channel);
      m_manager->SetCurrent(deviceId, control.current, control.channel);
      m_manager->TurnOn(deviceId, control.channel);
      control.outputOn = true;
    }
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  ImGui::Text("Output is %s", outputOn ? "ON" : "OFF");

  ImGui::Separator();

  // Current readings
  ImGui::Text("Live Measurements:");

  // Display cached values - don't read from hardware in render loop!
  ImGui::Text("Voltage: %.3f V", status.lastMeasurement.voltage);
  ImGui::Text("Current: %.4f A", status.lastMeasurement.current);
  ImGui::Text("Power: %.3f W", status.lastMeasurement.voltage * status.lastMeasurement.current);

  // Last update time
  auto now = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
    now - status.lastUpdated).count();
  ImGui::Text("Updated: %lld seconds ago", elapsed);

  // Manual update button
  if (ImGui::Button("Update Now")) {
    // This is OK - only reads when button is clicked
    auto measurement = m_manager->ReadMeasurement(deviceId, control.channel);

    Logger::GetInstance()->LogInfo("Manual update - " + deviceId +
      " Ch" + std::to_string(control.channel) +
      ": V=" + std::to_string(measurement.voltage) +
      "V, A=" + std::to_string(measurement.current) + "A");

    // Update the cached status
    m_manager->UpdateDeviceStatus(deviceId);
  }
}

void PowerSupplyUI::RefreshDeviceList() {
  if (!m_manager) return;

  m_deviceIds = m_manager->GetDeviceIds();

  // Initialize controls for new devices
  for (const auto& id : m_deviceIds) {
    if (m_deviceControls.find(id) == m_deviceControls.end()) {
      m_deviceControls[id] = DeviceControl();
    }
  }

  // Select first device if none selected
  if (m_selectedDevice.empty() && !m_deviceIds.empty()) {
    m_selectedDevice = m_deviceIds[0];
  }
}

void PowerSupplyUI::UpdateDeviceStatus() {
  if (!m_manager) return;

  for (const auto& deviceId : m_deviceIds) {
    if (m_manager->IsDeviceConnected(deviceId)) {
      m_manager->UpdateDeviceStatus(deviceId);

      // Update control state
      auto status = m_manager->GetDeviceStatus(deviceId);
      if (!status.channelOutputOn.empty()) {
        m_deviceControls[deviceId].outputOn = status.channelOutputOn[1];
      }
    }
  }
}