// UISMUPanel.cpp - Implementation of embedded SMU panel UI
#include "UISMUPanel.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_client.h"
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm> // For (std::min) and (std::max)

UISMUPanel::UISMUPanel(Keithley2400Manager& smuManager)
  : m_smuManager(smuManager) {
  // Constructor - initialize UI settings
}

UISMUPanel::~UISMUPanel() {
  // Destructor - no cleanup needed
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

  // Output status display
  // Note: Add method to get output status from device if available
  ImGui::Text("Output Status: Active"); // Placeholder

  // Output control buttons
  if (ImGui::Button("Turn ON", ImVec2(80, 30))) {
    device->SetOutput(true);
  }

  ImGui::SameLine();
  if (ImGui::Button("Turn OFF", ImVec2(80, 30))) {
    device->SetOutput(false);
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

void UISMUPanel::RenderVoltageSweepControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Voltage Sweep");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Start voltage with manual input
  if (ImGui::SliderFloat("Start (V)", &m_sourceSettings.sweepStart, -20.0f, 20.0f, "%.2f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepStartInput", &m_sourceSettings.sweepStart, 0.0f, 0.0f, "%.2f")) {
    m_sourceSettings.sweepStart = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStart));
  }
  ImGui::PopItemWidth();

  // Stop voltage with manual input
  if (ImGui::SliderFloat("Stop (V)", &m_sourceSettings.sweepStop, -20.0f, 20.0f, "%.2f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepStopInput", &m_sourceSettings.sweepStop, 0.0f, 0.0f, "%.2f")) {
    m_sourceSettings.sweepStop = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStop));
  }
  ImGui::PopItemWidth();

  // Steps with manual input
  ImGui::SliderInt("Steps", &m_sourceSettings.sweepSteps, 2, 100);
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  ImGui::InputInt("##SweepStepsInput", &m_sourceSettings.sweepSteps, 0, 0);
  m_sourceSettings.sweepSteps = (std::max)(2, (std::min)(100, m_sourceSettings.sweepSteps)); // Clamp
  ImGui::PopItemWidth();

  // Compliance with manual input
  if (ImGui::SliderFloat("Compliance (A)", &m_sourceSettings.sweepCompliance, 0.001f, 1.0f, "%.3f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepComplianceInput", &m_sourceSettings.sweepCompliance, 0.0f, 0.0f, "%.3f")) {
    m_sourceSettings.sweepCompliance = (std::max)(0.001f, (std::min)(1.0f, m_sourceSettings.sweepCompliance));
  }
  ImGui::PopItemWidth();

  // Delay with manual input
  if (ImGui::SliderFloat("Delay (s)", &m_sourceSettings.sweepDelay, 0.01f, 1.0f, "%.3f")) {
    // Value updated by slider
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  if (ImGui::InputFloat("##SweepDelayInput", &m_sourceSettings.sweepDelay, 0.0f, 0.0f, "%.3f")) {
    m_sourceSettings.sweepDelay = (std::max)(0.01f, (std::min)(1.0f, m_sourceSettings.sweepDelay));
  }
  ImGui::PopItemWidth();

  if (ImGui::Button("Perform Voltage Sweep")) {
    std::vector<VoltageSweepResult> sweepResults;
    if (device->VoltageSweep(m_sourceSettings.sweepStart, m_sourceSettings.sweepStop,
      m_sourceSettings.sweepSteps, m_sourceSettings.sweepCompliance,
      m_sourceSettings.sweepDelay, sweepResults)) {
      // Could store results for plotting or analysis
      ImGui::OpenPopup("Sweep Complete");
    }
  }

  // Popup for sweep completion
  if (ImGui::BeginPopupModal("Sweep Complete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Voltage sweep completed successfully!");
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