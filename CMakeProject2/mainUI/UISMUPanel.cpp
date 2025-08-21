// UISMUPanel.cpp - Implementation of embedded SMU panel UI
#include "UISMUPanel.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_client.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <algorithm> // For (std::min) and (std::max)

UISMUPanel::UISMUPanel(Keithley2400Manager& smuManager)
  : m_smuManager(smuManager),
  m_logger(Logger::GetInstance())  
{
  // Constructor - initialize UI settings
  m_logger->LogInfo("UISMUPanel: Initialized");
}

UISMUPanel::~UISMUPanel() {
  // Destructor - no cleanup needed
  m_logger->LogInfo("UISMUPanel: Destroyed");
}

void UISMUPanel::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Calculate content size for 3-column layout: 30% / 35% / 35%
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.30f;
  float middlePanelWidth = contentSize.x * 0.35f;
  float rightPanelWidth = contentSize.x * 0.35f;

  // Left Panel - Device List and Global Controls (30% width)
  ImGui::BeginChild("LeftSMUPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Main Device Interface (35% width)
  ImGui::BeginChild("MiddleSMUPanel", ImVec2(middlePanelWidth, contentSize.y), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Advanced Controls (35% width)
  ImGui::BeginChild("RightSMUPanel", ImVec2(rightPanelWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();
}

void UISMUPanel::RenderLeftPanel() {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "SMU Devices");
  ImGui::Separator();

  // Global controls first
  RenderGlobalControls();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Then device list
  RenderDeviceList();
}

void UISMUPanel::RenderMiddlePanel() {
  if (m_selectedDeviceName.empty()) {
    RenderNoSelectionMessage();
    return;
  }

  Keithley2400Client* device = m_smuManager.GetClient(m_selectedDeviceName);

  if (!device) {
    ImGui::Text("Selected device not found: %s", m_selectedDeviceName.c_str());
    return;
  }

  // Device header
  RenderDeviceHeader(device);

  ImGui::Spacing();

  // Connection controls
  RenderConnectionControls(device);

  ImGui::Spacing();

  // Device status
  RenderDeviceStatus(device);

  ImGui::Spacing();

  // Current readings (prominent display)
  RenderCurrentReadings(device);

  ImGui::Spacing();

  // Source controls
  RenderEnhancedSourceControls(device);

  ImGui::Spacing();

  // Output controls
  RenderOutputControls(device);
}

void UISMUPanel::RenderRightPanel() {
  if (m_selectedDeviceName.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a device for advanced controls");
    return;
  }

  Keithley2400Client* device = m_smuManager.GetClient(m_selectedDeviceName);

  if (!device) {
    ImGui::Text("Device not found");
    return;
  }

  // Advanced controls in right panel

  // Voltage sweep controls
  RenderVoltageSweepControls(device);

  ImGui::Spacing();

  // Measurement history plots
  RenderMeasurementPlots(device);

  ImGui::Spacing();

  // Raw SCPI interface
  RenderSCPIInterface(device);

  ImGui::Spacing();

  // Utility controls
  RenderUtilityControls(device);
}

void UISMUPanel::RenderGlobalControls() {
  ImGui::Text("Global Controls");
  ImGui::Separator();

  // Get aggregated data from the manager
  auto data = m_smuManager.GetAggregatedData();

  // Connection status
  ImGui::Text("Status: %d/%d connected", data.connectedCount, data.totalCount);

  if (data.connectedCount > 0) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● ONLINE");
  }
  else if (data.totalCount > 0) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● OFFLINE");
  }

  ImGui::Spacing();

  // Connection controls
  if (ImGui::Button("Connect All", ImVec2(-1, 25))) {
    if (m_smuManager.ConnectAll()) {
      m_smuManager.StartAllPolling(1000);
    }
  }

  if (ImGui::Button("Disconnect All", ImVec2(-1, 25))) {
    m_smuManager.DisconnectAll();
  }

  if (ImGui::Button("Reset All", ImVec2(-1, 25))) {
    m_smuManager.ResetAllInstruments();
  }

  ImGui::Spacing();

  // Output controls
  ImGui::Text("Output Control:");

  if (ImGui::Button("Turn ON All", ImVec2(-1, 30))) {
    m_smuManager.SetAllOutputs(true);
  }

  if (ImGui::Button("Turn OFF All", ImVec2(-1, 30))) {
    m_smuManager.SetAllOutputs(false);
  }

  ImGui::Spacing();

  // Global set value controls
  ImGui::Text("Global Set Values:");

  // Source mode selection
  ImGui::RadioButton("Voltage", &m_globalSettings.globalSourceMode, 0);
  ImGui::RadioButton("Current", &m_globalSettings.globalSourceMode, 1);

  if (m_globalSettings.globalSourceMode == 0) {
    // Voltage source mode
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##GlobalV", &m_globalSettings.globalVoltage, -20.0f, 20.0f, "%.3f V");
    ImGui::SliderFloat("##GlobalIC", &m_globalSettings.globalCompliance, 0.001f, 1.0f, "%.3f A");
    ImGui::PopItemWidth();

    if (ImGui::Button("Set All V-Source", ImVec2(-1, 25))) {
      auto clientNames = m_smuManager.GetClientNames();
      for (const auto& name : clientNames) {
        auto* client = m_smuManager.GetClient(name);
        if (client && client->IsConnected()) {
          client->SetupVoltageSource(m_globalSettings.globalVoltage, m_globalSettings.globalCompliance);
        }
      }
    }
  }
  else {
    // Current source mode
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##GlobalI", &m_globalSettings.globalCurrent, -1.0f, 1.0f, "%.6f A");
    ImGui::SliderFloat("##GlobalVC", &m_globalSettings.globalCompliance, 1.0f, 200.0f, "%.1f V");
    ImGui::PopItemWidth();

    if (ImGui::Button("Set All I-Source", ImVec2(-1, 25))) {
      auto clientNames = m_smuManager.GetClientNames();
      for (const auto& name : clientNames) {
        auto* client = m_smuManager.GetClient(name);
        if (client && client->IsConnected()) {
          client->SetupCurrentSource(m_globalSettings.globalCurrent, m_globalSettings.globalCompliance);
        }
      }
    }
  }

  // Real-time aggregated measurements
  if (data.connectedCount > 0) {
    ImGui::Spacing();
    ImGui::Text("Total Measurements:");
    ImGui::Text("V: %.3f V", data.totalVoltage);
    ImGui::Text("I: %.6f A", data.totalCurrent);
    ImGui::Text("P: %.6f W", data.totalPower);
  }
}

void UISMUPanel::RenderDeviceList() {
  ImGui::Text("Device List");
  ImGui::Separator();

  // Get all device names from the manager
  auto deviceNames = m_smuManager.GetClientNames();

  if (deviceNames.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No devices configured");
    ImGui::Text("Check smu_config.json");
    return;
  }

  for (const std::string& name : deviceNames) {
    Keithley2400Client* device = m_smuManager.GetClient(name);

    if (device) {  // Only show devices that exist
      ImGui::PushID(name.c_str());

      // Connection status indicator
      bool isConnected = device->IsConnected();
      ImVec4 statusColor = isConnected ?
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

      ImGui::TextColored(statusColor, "●");
      ImGui::SameLine();

      // Device selection button
      bool isSelected = (m_selectedDeviceName == name);
      if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
      }

      if (ImGui::Button(name.c_str(), ImVec2(-1, 25))) {
        m_selectedDeviceName = name;
      }

      if (isSelected) {
        ImGui::PopStyleColor();
      }

      // Quick status info
      if (isConnected) {
        auto reading = device->GetLatestReading();
        ImGui::Text("  %.3fV %.6fA", reading.voltage, reading.current);
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Not connected");
      }

      ImGui::PopID();
    }
  }
}

void UISMUPanel::RenderSelectedDeviceUI() {
  Keithley2400Client* device = m_smuManager.GetClient(m_selectedDeviceName);

  if (!device) {
    ImGui::Text("Selected device not found: %s", m_selectedDeviceName.c_str());
    return;
  }

  // Device header
  RenderDeviceHeader(device);

  ImGui::Spacing();

  // Connection controls
  RenderConnectionControls(device);

  ImGui::Spacing();

  // Device status
  RenderDeviceStatus(device);

  ImGui::Spacing();

  // Current readings (more prominent)
  RenderCurrentReadings(device);

  ImGui::Spacing();

  // Source controls (enhanced)
  RenderEnhancedSourceControls(device);

  ImGui::Spacing();

  // Output controls
  RenderOutputControls(device);

  ImGui::Spacing();

  // Voltage sweep controls
  RenderVoltageSweepControls(device);

  ImGui::Spacing();

  // Measurement history plots
  RenderMeasurementPlots(device);

  ImGui::Spacing();

  // Raw SCPI commands
  RenderSCPIInterface(device);

  ImGui::Spacing();

  // Utility controls
  RenderUtilityControls(device);
}

void UISMUPanel::RenderNoSelectionMessage() {
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a device from the left panel");
  ImGui::Spacing();
  ImGui::Text("Available features:");
  ImGui::BulletText("Individual device control");
  ImGui::BulletText("Real-time measurements");
  ImGui::BulletText("Source configuration");
  ImGui::BulletText("Output control");
  ImGui::BulletText("Device diagnostics");
}

void UISMUPanel::RenderDeviceHeader(Keithley2400Client* device) {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("Device: %s", m_selectedDeviceName.c_str());
  ImGui::SetWindowFontScale(1.0f);

  ImGui::SameLine();

  // Connection status
  bool isConnected = device->IsConnected();
  if (isConnected) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● CONNECTED");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● DISCONNECTED");
  }

  ImGui::Separator();
}

void UISMUPanel::RenderConnectionControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Connection");

  bool isConnected = device->IsConnected();

  if (isConnected) {
    if (ImGui::Button("Disconnect", ImVec2(100, 25))) {
      device->Disconnect();
    }
  }
  else {
    if (ImGui::Button("Connect", ImVec2(100, 25))) {
      // Note: Connection details should come from stored configuration
      // For now, using default. In real implementation, get from manager's stored connections
      device->Connect("127.0.0.1", 8888);
      if (device->IsConnected()) {
        device->StartPolling(1000);
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(100, 25))) {
    device->ResetInstrument();
  }
}

void UISMUPanel::RenderDeviceStatus(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Status");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Get device status information
  ImGui::Text("Communication: OK");
  ImGui::Text("Polling: %s", device->IsPolling() ? "Active" : "Stopped");

  // Show polling status with color indicator
  if (device->IsPolling()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "●");
  }
  else {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "●");
  }
}


void UISMUPanel::RenderOutputControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Output Control");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  if (ImGui::Button("Turn ON", ImVec2(80, 30))) {
    m_logger->LogInfo("UISMUPanel: User requested output ON for device: " + m_selectedDeviceName);

    // Configure instrument first (as we discussed earlier)
    device->ResetInstrument();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (m_sourceSettings.sourceMode == 0) {
      m_logger->LogInfo("UISMUPanel: Configuring voltage source: " +
        std::to_string(m_sourceSettings.voltageSetpoint) + "V, " +
        std::to_string(m_sourceSettings.compliance) + "A compliance");
      device->SetupVoltageSource(m_sourceSettings.voltageSetpoint, m_sourceSettings.compliance);
    }
    else {
      m_logger->LogInfo("UISMUPanel: Configuring current source: " +
        std::to_string(m_sourceSettings.currentSetpoint) + "A, " +
        std::to_string(m_sourceSettings.compliance) + "V compliance");
      device->SetupCurrentSource(m_sourceSettings.currentSetpoint, m_sourceSettings.compliance);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool success = device->SetOutput(true);
    if (success) {
      m_logger->LogInfo("UISMUPanel: Output turned ON successfully");
    }
    else {
      m_logger->LogError("UISMUPanel: Failed to turn output ON: " + device->GetLastError());
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Turn OFF", ImVec2(80, 30))) {
    m_logger->LogInfo("UISMUPanel: User requested output OFF for device: " + m_selectedDeviceName);

    bool success = device->SetOutput(false);
    if (success) {
      m_logger->LogInfo("UISMUPanel: Output turned OFF successfully");
    }
    else {
      m_logger->LogError("UISMUPanel: Failed to turn output OFF: " + device->GetLastError());
    }
  }
}


void UISMUPanel::RenderSourceControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Source Configuration");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Source mode selection
  ImGui::Text("Source Mode:");
  ImGui::RadioButton("Voltage Source", &m_sourceSettings.sourceMode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Current Source", &m_sourceSettings.sourceMode, 1);

  ImGui::Spacing();

  if (m_sourceSettings.sourceMode == 0) {
    // Voltage source mode
    ImGui::Text("Voltage Setpoint:");
    ImGui::SliderFloat("##VoltageSlider", &m_sourceSettings.voltageSetpoint, -20.0f, 20.0f, "%.3f V");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputFloat("##VoltageInput", &m_sourceSettings.voltageSetpoint, 0.0f, 0.0f, "%.3f");
    ImGui::PopItemWidth();

    ImGui::Text("Current Compliance:");
    ImGui::SliderFloat("##CurrentCompliance", &m_sourceSettings.compliance, 0.001f, 1.0f, "%.3f A");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputFloat("##ComplianceInput", &m_sourceSettings.compliance, 0.0f, 0.0f, "%.3f");
    ImGui::PopItemWidth();

    if (ImGui::Button("Setup Voltage Source", ImVec2(200, 30))) {
      device->SetupVoltageSource(m_sourceSettings.voltageSetpoint, m_sourceSettings.compliance);
    }
  }
  else {
    // Current source mode  
    ImGui::Text("Current Setpoint:");
    ImGui::SliderFloat("##CurrentSlider", &m_sourceSettings.currentSetpoint, -1.0f, 1.0f, "%.6f A");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputFloat("##CurrentInput", &m_sourceSettings.currentSetpoint, 0.0f, 0.0f, "%.6f");
    ImGui::PopItemWidth();

    ImGui::Text("Voltage Compliance:");
    ImGui::SliderFloat("##VoltageCompliance", &m_sourceSettings.compliance, 1.0f, 200.0f, "%.1f V");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputFloat("##VComplianceInput", &m_sourceSettings.compliance, 0.0f, 0.0f, "%.1f");
    ImGui::PopItemWidth();

    if (ImGui::Button("Setup Current Source", ImVec2(200, 30))) {
      device->SetupCurrentSource(m_sourceSettings.currentSetpoint, m_sourceSettings.compliance);
    }
  }
}

void UISMUPanel::RenderCurrentReadings(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Current Readings");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  auto reading = device->GetLatestReading();

  // Display readings with more detail like the original
  ImGui::Text("Voltage:    %10.6f V", reading.voltage);
  ImGui::Text("Current:    %10.9f A (%.3f mA)", reading.current, reading.current * 1000.0);
  ImGui::Text("Resistance: %10.2f Ohms", reading.resistance);
  ImGui::Text("Power:      %10.9f W", reading.power);
}

void UISMUPanel::RenderEnhancedSourceControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Source Configuration");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Source mode selection with combo box like original
  ImGui::Text("Source Mode:");
  const char* sourceModes[] = { "Voltage Source", "Current Source" };
  ImGui::Combo("Mode", &m_sourceSettings.sourceMode, sourceModes, IM_ARRAYSIZE(sourceModes));

  ImGui::Spacing();

  if (m_sourceSettings.sourceMode == 0) {
    // Voltage source controls with sliders and input boxes like original
    ImGui::Text("Voltage Source Controls:");

    // Voltage slider and manual input
    if (ImGui::SliderFloat("Voltage (V)", &m_sourceSettings.voltageSetpoint, -20.0f, 20.0f, "%.3f")) {
      // Value updated by slider
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    if (ImGui::InputFloat("##VoltageInput", &m_sourceSettings.voltageSetpoint, 0.0f, 0.0f, "%.3f")) {
      // Clamp to valid range
      m_sourceSettings.voltageSetpoint = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.voltageSetpoint));
    }
    ImGui::PopItemWidth();

    // Current compliance slider and manual input
    if (ImGui::SliderFloat("Current Compliance (A)", &m_sourceSettings.compliance, 0.001f, 1.0f, "%.3f")) {
      // Value updated by slider
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    if (ImGui::InputFloat("##ComplianceInput", &m_sourceSettings.compliance, 0.0f, 0.0f, "%.3f")) {
      // Clamp to valid range
      m_sourceSettings.compliance = (std::max)(0.001f, (std::min)(1.0f, m_sourceSettings.compliance));
    }
    ImGui::PopItemWidth();

    if (ImGui::Button("Setup Voltage Source")) {
      device->SetupVoltageSource(m_sourceSettings.voltageSetpoint, m_sourceSettings.compliance);
    }
  }
  else {
    // Current source controls
    ImGui::Text("Current Source Controls:");

    // Current slider and manual input
    if (ImGui::SliderFloat("Current (A)", &m_sourceSettings.currentSetpoint, 0.0f, 1.0f, "%.6f")) {
      // Value updated by slider
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    if (ImGui::InputFloat("##CurrentInput", &m_sourceSettings.currentSetpoint, 0.0f, 0.0f, "%.6f")) {
      // Clamp to valid range
      m_sourceSettings.currentSetpoint = (std::max)(0.0f, (std::min)(1.0f, m_sourceSettings.currentSetpoint));
    }
    ImGui::PopItemWidth();

    // Voltage compliance slider and manual input
    if (ImGui::SliderFloat("Voltage Compliance (V)", &m_sourceSettings.compliance, 1.0f, 200.0f, "%.1f")) {
      // Value updated by slider
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    if (ImGui::InputFloat("##VComplianceInput", &m_sourceSettings.compliance, 0.0f, 0.0f, "%.1f")) {
      // Clamp to valid range
      m_sourceSettings.compliance = (std::max)(1.0f, (std::min)(200.0f, m_sourceSettings.compliance));
    }
    ImGui::PopItemWidth();

    if (ImGui::Button("Setup Current Source")) {
      device->SetupCurrentSource(m_sourceSettings.currentSetpoint, m_sourceSettings.compliance);
    }
  }
}


// Then in UISMUPanel.cpp, replace RenderVoltageSweepControls with this enhanced version:

void UISMUPanel::RenderVoltageSweepControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Voltage Sweep");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Sweep parameter controls (same as before)
  if (ImGui::SliderFloat("Start (V)", &m_sourceSettings.sweepStart, -20.0f, 20.0f, "%.2f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepStartInput", &m_sourceSettings.sweepStart, 0.0f, 0.0f, "%.2f")) {
    m_sourceSettings.sweepStart = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStart));
  }
  ImGui::PopItemWidth();

  if (ImGui::SliderFloat("Stop (V)", &m_sourceSettings.sweepStop, -20.0f, 20.0f, "%.2f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepStopInput", &m_sourceSettings.sweepStop, 0.0f, 0.0f, "%.2f")) {
    m_sourceSettings.sweepStop = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStop));
  }
  ImGui::PopItemWidth();

  ImGui::SliderInt("Steps", &m_sourceSettings.sweepSteps, 2, 100);
  ImGui::SliderFloat("Compliance (A)", &m_sourceSettings.sweepCompliance, 0.001f, 1.0f, "%.3f");
  ImGui::SliderFloat("Delay (s)", &m_sourceSettings.sweepDelay, 0.01f, 1.0f, "%.3f");

  ImGui::Spacing();

  // ENHANCED: Real-time sweep status display
  if (m_sweepState.inProgress) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "🔄 SWEEP IN PROGRESS");

    // Calculate elapsed time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_sweepState.startTime).count();
    ImGui::Text("Elapsed: %.1f seconds", elapsed / 1000.0f);

    // Estimated progress (rough estimate)
    float estimatedTotal = m_sourceSettings.sweepSteps * m_sourceSettings.sweepDelay * 1000.0f;
    float progress = (std::min)(elapsed / estimatedTotal, 1.0f);
    ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Parameters: %.1fV → %.1fV, %d steps",
      m_sourceSettings.sweepStart, m_sourceSettings.sweepStop, m_sourceSettings.sweepSteps);

    if (ImGui::Button("⚠️ This will complete automatically", ImVec2(-1, 30))) {
      // Sweep is running on server side, can't really cancel easily
      ImGui::OpenPopup("Cannot Cancel");
    }
  }
  else {
    // ENHANCED: Sweep initiation with immediate feedback
    if (ImGui::Button("🚀 Perform Voltage Sweep", ImVec2(-1, 40))) {
      // Clear previous results
      m_sweepState.lastResult = "";
      m_sweepState.lastSweepData.clear();
      m_sweepState.inProgress = true;
      m_sweepState.startTime = std::chrono::steady_clock::now();

      // Log the sweep attempt
      Logger::GetInstance()->LogInfo("UI: Starting voltage sweep from " +
        std::to_string(m_sourceSettings.sweepStart) + "V to " +
        std::to_string(m_sourceSettings.sweepStop) + "V");

      // Perform sweep (this will block briefly while sending command)
      std::vector<VoltageSweepResult> sweepResults;
      bool success = device->VoltageSweep(
        m_sourceSettings.sweepStart,
        m_sourceSettings.sweepStop,
        m_sourceSettings.sweepSteps,
        m_sourceSettings.sweepCompliance,
        m_sourceSettings.sweepDelay,
        sweepResults
      );

      // Update state immediately after command completes
      m_sweepState.inProgress = false;

      if (success) {
        m_sweepState.lastResult = "✅ Sweep completed successfully!";
        m_sweepState.lastSweepData = sweepResults;

        Logger::GetInstance()->LogInfo("UI: Sweep completed with " + std::to_string(sweepResults.size()) + " points");

        // ADD THIS: Print detailed data to console
        PrintSweepDataToConsole(m_sweepState.lastSweepData);

        // Show success popup
        ImGui::OpenPopup("Sweep Complete");
      }
      else {
        m_sweepState.lastResult = "❌ Sweep failed: " + device->GetLastError();
        Logger::GetInstance()->LogError("UI: Sweep failed - " + device->GetLastError());

        // Show error popup
        ImGui::OpenPopup("Sweep Failed");
      }
    }
  }

  // ENHANCED: Results display
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Last Sweep Results:");

  if (!m_sweepState.lastResult.empty()) {
    if (m_sweepState.lastResult.find("✅") != std::string::npos) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", m_sweepState.lastResult.c_str());
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_sweepState.lastResult.c_str());
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No sweep performed yet");
  }

  // ENHANCED: Data table for sweep results
  if (!m_sweepState.lastSweepData.empty()) {
    ImGui::Text("Data points: %zu", m_sweepState.lastSweepData.size());

    if (ImGui::BeginTable("SweepResults", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Set V", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Measured V", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Current (mA)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableHeadersRow();

      // Show first few and last few points
      size_t maxDisplay = 6;
      size_t dataSize = m_sweepState.lastSweepData.size();

      for (size_t i = 0; i < (std::min)(dataSize, maxDisplay / 2); ++i) {
        const auto& point = m_sweepState.lastSweepData[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%.2f", point.setVoltage);
        ImGui::TableNextColumn(); ImGui::Text("%.4f", point.measuredVoltage);
        ImGui::TableNextColumn(); ImGui::Text("%.6f", point.measuredCurrent * 1000.0);
      }

      if (dataSize > maxDisplay) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("...");
        ImGui::TableNextColumn(); ImGui::Text("...");
        ImGui::TableNextColumn(); ImGui::Text("...");

        for (size_t i = dataSize - maxDisplay / 2; i < dataSize; ++i) {
          const auto& point = m_sweepState.lastSweepData[i];
          ImGui::TableNextRow();
          ImGui::TableNextColumn(); ImGui::Text("%.2f", point.setVoltage);
          ImGui::TableNextColumn(); ImGui::Text("%.4f", point.measuredVoltage);
          ImGui::TableNextColumn(); ImGui::Text("%.6f", point.measuredCurrent * 1000.0);
        }
      }

      ImGui::EndTable();
    }

    // Add a button to re-print data to console
    if (ImGui::Button("🖨️ Print to Console")) {
      PrintSweepDataToConsole(m_sweepState.lastSweepData);
    }
    ImGui::SameLine();
    if (ImGui::Button("📊 Export Data")) {
      ExportSweepDataToFile(m_sweepState.lastSweepData);
    }
  }

  // Popups for feedback
  if (ImGui::BeginPopupModal("Sweep Complete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("✅ Voltage sweep completed successfully!");
    ImGui::Text("Data points collected: %zu", m_sweepState.lastSweepData.size());
    ImGui::Separator();
    if (ImGui::Button("Great!", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Sweep Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Voltage sweep failed!");
    ImGui::Text("Error: %s", device->GetLastError().c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Cannot Cancel", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Sweep is running on the instrument.");
    ImGui::Text("It will complete automatically.");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Export Info", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Data export feature coming soon!");
    ImGui::Text("For now, check the console logs for data.");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void UISMUPanel::RenderMeasurementPlots(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Measurement History");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Display current values and measurement info
  auto reading = device->GetLatestReading();

  ImGui::Text("Real-time measurements:");
  ImGui::Text("  V: %.6f V", reading.voltage);
  ImGui::Text("  I: %.6f mA", reading.current * 1000.0);
  ImGui::Text("  P: %.6f mW", reading.power * 1000.0);

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Historical plotting available when polling");
}

void UISMUPanel::RenderSCPIInterface(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Raw SCPI Commands");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  ImGui::InputText("Command", m_scpiState.command, sizeof(m_scpiState.command));

  if (ImGui::Button("Send Write Command")) {
    if (strlen(m_scpiState.command) > 0) {
      device->SendWriteCommand(m_scpiState.command);
      m_scpiState.lastResponse = "Write command sent";
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Send Query Command")) {
    if (strlen(m_scpiState.command) > 0) {
      std::string response;
      if (device->SendQueryCommand(m_scpiState.command, response)) {
        m_scpiState.lastResponse = "Response: " + response;
      }
      else {
        m_scpiState.lastResponse = "Query failed: " + device->GetLastError();
      }
    }
  }

  if (!m_scpiState.lastResponse.empty()) {
    ImGui::Text("Last Response:");
    ImGui::TextWrapped("%s", m_scpiState.lastResponse.c_str());
  }
}

void UISMUPanel::RenderUtilityControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Utilities");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Polling controls with rate selection
  if (device->IsPolling()) {
    if (ImGui::Button("Stop Polling", ImVec2(120, 25))) {
      device->StopPolling();
    }
  }
  else {
    ImGui::Text("Start with rate:");
    if (ImGui::Button("100ms", ImVec2(60, 25))) {
      device->StartPolling(100);
    }
    ImGui::SameLine();
    if (ImGui::Button("200ms", ImVec2(60, 25))) {
      device->StartPolling(200);
    }
    ImGui::SameLine();
    if (ImGui::Button("500ms", ImVec2(60, 25))) {
      device->StartPolling(500);
    }
  }

 
}
void UISMUPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;
}



void UISMUPanel::PrintSweepDataToConsole(const std::vector<VoltageSweepResult>& data) {
  if (data.empty()) {
    std::cout << "No sweep data to print.\n" << std::endl;
    return;
  }

  // Print header
  std::cout << "\n" << std::string(70, '=') << std::endl;
  std::cout << "           VOLTAGE SWEEP RESULTS" << std::endl;
  std::cout << std::string(70, '=') << std::endl;
  std::cout << "Device: " << m_selectedDeviceName << std::endl;
  std::cout << "Points: " << data.size() << std::endl;
  std::cout << "Range:  " << data.front().setVoltage << "V to " << data.back().setVoltage << "V" << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  // Print column headers
  std::cout << std::left
    << std::setw(6) << "Step"
    << std::setw(12) << "Set V"
    << std::setw(15) << "Measured V"
    << std::setw(15) << "Current (A)"
    << std::setw(15) << "Current (mA)"
    << "Power (mW)" << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  // Print data rows
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& point = data[i];
    double powerMW = point.measuredVoltage * point.measuredCurrent * 1000.0; // mW

    std::cout << std::left
      << std::setw(6) << (i + 1)
      << std::setw(12) << std::fixed << std::setprecision(3) << point.setVoltage
      << std::setw(15) << std::fixed << std::setprecision(6) << point.measuredVoltage
      << std::setw(15) << std::scientific << std::setprecision(3) << point.measuredCurrent
      << std::setw(15) << std::fixed << std::setprecision(6) << (point.measuredCurrent * 1000.0)
      << std::fixed << std::setprecision(6) << powerMW << std::endl;
  }

  std::cout << std::string(70, '-') << std::endl;

  // Print summary statistics
  double maxCurrent = 0.0, minCurrent = 0.0;
  double maxVoltage = data[0].measuredVoltage, minVoltage = data[0].measuredVoltage;

  for (const auto& point : data) {
    maxCurrent = (std::max)(maxCurrent, std::abs(point.measuredCurrent));
    minCurrent = (std::min)(minCurrent, std::abs(point.measuredCurrent));
    maxVoltage = (std::max)(maxVoltage, point.measuredVoltage);
    minVoltage = (std::min)(minVoltage, point.measuredVoltage);
  }

  std::cout << "SUMMARY:" << std::endl;
  std::cout << "  Voltage range: " << std::fixed << std::setprecision(3)
    << minVoltage << "V to " << maxVoltage << "V" << std::endl;
  std::cout << "  Current range: " << std::scientific << std::setprecision(3)
    << minCurrent << "A to " << maxCurrent << "A" << std::endl;
  std::cout << "  Max current:   " << std::fixed << std::setprecision(6)
    << (maxCurrent * 1000.0) << " mA" << std::endl;

  // Calculate resistance if current is non-zero
  if (maxCurrent > 1e-12) {
    double resistance = maxVoltage / maxCurrent;
    std::cout << "  Est. resistance: " << std::scientific << std::setprecision(3)
      << resistance << " Ohms" << std::endl;
  }

  std::cout << std::string(70, '=') << std::endl;
  std::cout << "Data printed at: " << GetCurrentTimeString() << std::endl;
  std::cout << std::string(70, '=') << "\n" << std::endl;

  // Also log to the logger system
  m_logger->LogInfo("UISMUPanel: Sweep data printed to console (" +
    std::to_string(data.size()) + " points)");
}

void UISMUPanel::ExportSweepDataToFile(const std::vector<VoltageSweepResult>& data) {
  if (data.empty()) {
    m_logger->LogWarning("UISMUPanel: No data to export");
    return;
  }

  // Generate filename with timestamp
  std::string filename = "sweep_data_" + GetTimestampString() + ".csv";

  std::ofstream file(filename);
  if (!file.is_open()) {
    m_logger->LogError("UISMUPanel: Failed to create export file: " + filename);
    return;
  }

  // Write CSV header
  file << "Step,Set_Voltage_V,Measured_Voltage_V,Current_A,Current_mA,Power_mW\n";

  // Write data
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& point = data[i];
    double currentMA = point.measuredCurrent * 1000.0;
    double powerMW = point.measuredVoltage * point.measuredCurrent * 1000.0;

    file << (i + 1) << ","
      << std::fixed << std::setprecision(6) << point.setVoltage << ","
      << std::fixed << std::setprecision(6) << point.measuredVoltage << ","
      << std::scientific << std::setprecision(6) << point.measuredCurrent << ","
      << std::fixed << std::setprecision(6) << currentMA << ","
      << std::fixed << std::setprecision(6) << powerMW << "\n";
  }

  file.close();

  std::cout << "\nData exported to: " << filename << std::endl;
  m_logger->LogInfo("UISMUPanel: Data exported to " + filename);
}

// Helper methods - add these to UISMUPanel class:
std::string UISMUPanel::GetCurrentTimeString() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::string UISMUPanel::GetTimestampString() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  return ss.str();
}