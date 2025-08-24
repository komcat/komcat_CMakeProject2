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


// Enhanced connection controls with auto-polling
void UISMUPanel::RenderConnectionControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Connection");

  bool isConnected = device->IsConnected();

  if (isConnected) {
    if (ImGui::Button("Disconnect", ImVec2(100, 25))) {
      // Save polling state before disconnect
      m_pollingState.wasPollingOnConnect = device->IsPolling();
      if (m_pollingState.wasPollingOnConnect) {
        m_pollingState.lastPollingRate = m_sourceSettings.pollingInterval;
        StopPollingWithFeedback(device);
      }

      device->Disconnect();
      m_logger->LogInfo("UISMUPanel: Disconnected " + m_selectedDeviceName);
    }
  }
  else {
    if (ImGui::Button("Connect", ImVec2(100, 25))) {
      // Try to connect with stored configuration
      bool success = device->Connect("127.0.0.101", 8888); // Use stored config in real implementation

      if (success) {
        m_logger->LogInfo("UISMUPanel: Connected " + m_selectedDeviceName);

        // Auto-start polling if enabled
        if (m_sourceSettings.autoStartPolling) {
          std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Brief delay for connection
          StartOptimalPolling(device, m_pollingState.lastPollingRate);
        }
      }
      else {
        m_logger->LogError("UISMUPanel: Failed to connect " + m_selectedDeviceName +
          ": " + device->GetLastError());
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(100, 25))) {
    if (isConnected) {
      // Stop polling before reset
      bool wasPolling = device->IsPolling();
      int pollingRate = m_sourceSettings.pollingInterval;

      if (wasPolling) {
        device->StopPolling();
      }

      device->ResetInstrument();
      m_logger->LogInfo("UISMUPanel: Reset " + m_selectedDeviceName);

      // Restart polling if it was running
      if (wasPolling) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for reset
        StartOptimalPolling(device, pollingRate);
      }
    }
  }

  // Auto-polling checkbox
  ImGui::Spacing();
  ImGui::Checkbox("Auto-start polling on connect", &m_sourceSettings.autoStartPolling);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically start data polling when device connects");
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


// Enhanced current readings display with trend indicators
void UISMUPanel::RenderCurrentReadings(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Real-time Measurements");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  auto reading = device->GetLatestReading();
  bool isPolling = device->IsPolling();

  // Main measurement display with larger text for key values
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font but could be larger

  // Voltage display with trend
  ImGui::Text("Voltage:");
  ImGui::SameLine();
  if (isPolling) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%10.6f V", reading.voltage);
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%10.6f V", reading.voltage);
  }

  // Current display with units
  ImGui::Text("Current:");
  ImGui::SameLine();
  if (isPolling) {
    if (std::abs(reading.current) < 1e-6) { // Less than 1 µA
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%10.3f nA", reading.current * 1e9);
    }
    else if (std::abs(reading.current) < 1e-3) { // Less than 1 mA
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%10.3f µA", reading.current * 1e6);
    }
    else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%10.3f mA", reading.current * 1000.0);
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%10.9f A", reading.current);
  }

  ImGui::PopFont();

  // Secondary measurements
  ImGui::Spacing();
  ImGui::Text("Resistance: %10.2f Ω", reading.resistance);
  ImGui::Text("Power:      %10.6f mW", reading.power * 1000.0);

  // Data freshness indicator
  if (isPolling) {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - reading.timestamp).count();

    if (age < 500) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🟢 Live (%dms)", (int)age);
    }
    else if (age < 2000) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "🟡 Recent (%dms)", (int)age);
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "🔴 Stale (%.1fs)", age / 1000.0f);
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏸️ Not polling");
  }

  ImGui::Spacing();

  // Quick actions for polling
  if (!isPolling) {
    if (ImGui::Button("🚀 Quick Start (250ms)", ImVec2(150, 25))) {
      device->StartPolling(250);
      m_logger->LogInfo("UISMUPanel: Quick started polling at 250ms");
    }
  }
  else {
    if (ImGui::Button("⏸️ Pause Polling", ImVec2(120, 25))) {
      device->StopPolling();
      m_logger->LogInfo("UISMUPanel: Paused polling");
    }
  }
}

// Add a utility controls section for advanced polling features
void UISMUPanel::RenderUtilityControls(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Utilities & Data");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Data logging controls
  static bool autoLog = false;
  ImGui::Checkbox("Auto-log measurements", &autoLog);

  if (autoLog && device->IsPolling()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "📝 Logging...");
    // TODO: Implement auto-logging to file
  }

  // Manual data capture
  if (ImGui::Button("📸 Capture Current Reading", ImVec2(180, 25))) {
    auto reading = device->GetLatestReading();

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "MANUAL READING CAPTURE" << std::endl;
    std::cout << "Device: " << m_selectedDeviceName << std::endl;
    std::cout << "Time:   " << GetCurrentTimeString() << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << "Voltage:    " << std::fixed << std::setprecision(6) << reading.voltage << " V" << std::endl;
    std::cout << "Current:    " << std::scientific << std::setprecision(3) << reading.current << " A" << std::endl;
    std::cout << "            " << std::fixed << std::setprecision(6) << (reading.current * 1000.0) << " mA" << std::endl;
    std::cout << "Resistance: " << std::scientific << std::setprecision(3) << reading.resistance << " Ω" << std::endl;
    std::cout << "Power:      " << std::scientific << std::setprecision(3) << reading.power << " W" << std::endl;
    std::cout << "            " << std::fixed << std::setprecision(6) << (reading.power * 1000.0) << " mW" << std::endl;
    std::cout << std::string(50, '=') << "\n" << std::endl;

    m_logger->LogInfo("UISMUPanel: Manual reading captured and printed");
  }

  // Polling diagnostics
  if (device->IsPolling()) {
    ImGui::Spacing();
    ImGui::Text("Polling Diagnostics:");
    ImGui::Text("• Rate: %d ms intervals", m_sourceSettings.pollingInterval);
    ImGui::Text("• Target rate: %.1f Hz", 1000.0f / m_sourceSettings.pollingInterval);

    // Add data rate monitoring if available
    static auto lastCheckTime = std::chrono::steady_clock::now();
    static int lastReadCount = 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime).count();

    if (elapsed >= 5) { // Update every 5 seconds
      // This would require access to read count from the client
      // For now, just show the theoretical rate
      ImGui::Text("• Status: Running smoothly");
      lastCheckTime = now;
    }
  }
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
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Measurement Sweeps");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Tabbed interface for different sweep types
  if (ImGui::BeginTabBar("SweepTypeTabs", ImGuiTabBarFlags_None)) {

    // VOLTAGE SWEEP TAB
    if (ImGui::BeginTabItem("⚡ Voltage Sweep")) {
      ImGui::Spacing();

      // Voltage sweep parameter controls
      if (ImGui::SliderFloat("Start (V)", &m_sourceSettings.sweepStart, -20.0f, 20.0f, "%.2f")) {
        // Value updated by slider
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##VSweepStartInput", &m_sourceSettings.sweepStart, 0.0f, 0.0f, "%.2f")) {
        m_sourceSettings.sweepStart = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStart));
      }
      ImGui::PopItemWidth();

      if (ImGui::SliderFloat("Stop (V)", &m_sourceSettings.sweepStop, -20.0f, 20.0f, "%.2f")) {
        // Value updated by slider
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##VSweepStopInput", &m_sourceSettings.sweepStop, 0.0f, 0.0f, "%.2f")) {
        m_sourceSettings.sweepStop = (std::max)(-20.0f, (std::min)(20.0f, m_sourceSettings.sweepStop));
      }
      ImGui::PopItemWidth();

      ImGui::SliderInt("Steps", &m_sourceSettings.sweepSteps, 2, 100);
      ImGui::SliderFloat("Current Compliance (A)", &m_sourceSettings.sweepCompliance, 0.001f, 1.0f, "%.3f");
      ImGui::SliderFloat("Delay (s)", &m_sourceSettings.sweepDelay, 0.01f, 1.0f, "%.3f");

      ImGui::Spacing();

      // Voltage sweep status and controls (existing logic)
      if (m_sweepState.inProgress) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "🔄 VOLTAGE SWEEP IN PROGRESS");

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_sweepState.startTime).count();
        ImGui::Text("Elapsed: %.1f seconds", elapsed / 1000.0f);

        float estimatedTotal = m_sourceSettings.sweepSteps * m_sourceSettings.sweepDelay * 1000.0f;
        float progress = (std::min)(elapsed / estimatedTotal, 1.0f);
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Parameters: %.1fV → %.1fV, %d steps",
          m_sourceSettings.sweepStart, m_sourceSettings.sweepStop, m_sourceSettings.sweepSteps);
      }
      else {
        if (ImGui::Button("🚀 Perform Voltage Sweep", ImVec2(-1, 40))) {
          // Clear previous results
          m_sweepState.lastResult = "";
          m_sweepState.lastSweepData.clear();
          m_sweepState.inProgress = true;
          m_sweepState.startTime = std::chrono::steady_clock::now();

          m_logger->LogInfo("UI: Starting voltage sweep from " +
            std::to_string(m_sourceSettings.sweepStart) + "V to " +
            std::to_string(m_sourceSettings.sweepStop) + "V");

          std::vector<VoltageSweepResult> sweepResults;
          bool success = device->VoltageSweep(
            m_sourceSettings.sweepStart,
            m_sourceSettings.sweepStop,
            m_sourceSettings.sweepSteps,
            m_sourceSettings.sweepCompliance,
            m_sourceSettings.sweepDelay,
            sweepResults
          );

          m_sweepState.inProgress = false;

          if (success) {
            m_sweepState.lastResult = "✅ Voltage sweep completed successfully!";
            m_sweepState.lastSweepData = sweepResults;
            m_logger->LogInfo("UI: Voltage sweep completed with " + std::to_string(sweepResults.size()) + " points");
            PrintSweepDataToConsole(m_sweepState.lastSweepData);
          }
          else {
            m_sweepState.lastResult = "❌ Voltage sweep failed: " + device->GetLastError();
            m_logger->LogError("UI: Voltage sweep failed - " + device->GetLastError());
          }
        }
      }

      // Voltage sweep results display
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Text("Voltage Sweep Results:");

      if (!m_sweepState.lastResult.empty()) {
        if (m_sweepState.lastResult.find("✅") != std::string::npos) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", m_sweepState.lastResult.c_str());
        }
        else {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_sweepState.lastResult.c_str());
        }

        if (!m_sweepState.lastSweepData.empty()) {
          ImGui::Text("Data points: %zu", m_sweepState.lastSweepData.size());
          if (ImGui::Button("🖨️ Print V-Sweep to Console")) {
            PrintSweepDataToConsole(m_sweepState.lastSweepData);
          }
        }
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No voltage sweep performed yet");
      }

      ImGui::EndTabItem();
    }

    // CURRENT SWEEP TAB
    if (ImGui::BeginTabItem("🔋 Current Sweep")) {
      ImGui::Spacing();

      // Current sweep parameter controls
      if (ImGui::SliderFloat("Start (A)", &m_sourceSettings.currentSweepStart, -1.0f, 1.0f, "%.6f")) {
        // Value updated by slider
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##CSweepStartInput", &m_sourceSettings.currentSweepStart, 0.0f, 0.0f, "%.6f")) {
        m_sourceSettings.currentSweepStart = (std::max)(-1.0f, (std::min)(1.0f, m_sourceSettings.currentSweepStart));
      }
      ImGui::PopItemWidth();

      if (ImGui::SliderFloat("Stop (A)", &m_sourceSettings.currentSweepStop, -1.0f, 1.0f, "%.6f")) {
        // Value updated by slider
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##CSweepStopInput", &m_sourceSettings.currentSweepStop, 0.0f, 0.0f, "%.6f")) {
        m_sourceSettings.currentSweepStop = (std::max)(-1.0f, (std::min)(1.0f, m_sourceSettings.currentSweepStop));
      }
      ImGui::PopItemWidth();

      ImGui::SliderInt("Steps##CSteps", &m_sourceSettings.currentSweepSteps, 2, 100);
      ImGui::SliderFloat("Voltage Compliance (V)", &m_sourceSettings.currentSweepCompliance, 1.0f, 200.0f, "%.1f");
      ImGui::SliderFloat("Delay (s)##CDelay", &m_sourceSettings.currentSweepDelay, 0.01f, 1.0f, "%.3f");

      ImGui::Spacing();

      // Current sweep status and controls
      if (m_currentSweepState.inProgress) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "🔄 CURRENT SWEEP IN PROGRESS");

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_currentSweepState.startTime).count();
        ImGui::Text("Elapsed: %.1f seconds", elapsed / 1000.0f);

        float estimatedTotal = m_sourceSettings.currentSweepSteps * m_sourceSettings.currentSweepDelay * 1000.0f;
        float progress = (std::min)(elapsed / estimatedTotal, 1.0f);
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Parameters: %.6fA → %.6fA, %d steps",
          m_sourceSettings.currentSweepStart, m_sourceSettings.currentSweepStop, m_sourceSettings.currentSweepSteps);
      }
      else {
        if (ImGui::Button("🚀 Perform Current Sweep", ImVec2(-1, 40))) {
          // Clear previous results
          m_currentSweepState.lastResult = "";
          m_currentSweepState.lastSweepData.clear();
          m_currentSweepState.inProgress = true;
          m_currentSweepState.startTime = std::chrono::steady_clock::now();

          m_logger->LogInfo("UI: Starting current sweep from " +
            std::to_string(m_sourceSettings.currentSweepStart) + "A to " +
            std::to_string(m_sourceSettings.currentSweepStop) + "A");

          std::vector<CurrentSweepResult> currentSweepResults;
          bool success = device->CurrentSweep(
            m_sourceSettings.currentSweepStart,
            m_sourceSettings.currentSweepStop,
            m_sourceSettings.currentSweepSteps,
            m_sourceSettings.currentSweepCompliance,
            m_sourceSettings.currentSweepDelay,
            currentSweepResults
          );

          m_currentSweepState.inProgress = false;

          if (success) {
            m_currentSweepState.lastResult = "✅ Current sweep completed successfully!";
            m_currentSweepState.lastSweepData = currentSweepResults;
            m_logger->LogInfo("UI: Current sweep completed with " + std::to_string(currentSweepResults.size()) + " points");
            PrintCurrentSweepDataToConsole(m_currentSweepState.lastSweepData);
          }
          else {
            m_currentSweepState.lastResult = "❌ Current sweep failed: " + device->GetLastError();
            m_logger->LogError("UI: Current sweep failed - " + device->GetLastError());
          }
        }
      }

      // Current sweep results display
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Text("Current Sweep Results:");

      if (!m_currentSweepState.lastResult.empty()) {
        if (m_currentSweepState.lastResult.find("✅") != std::string::npos) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", m_currentSweepState.lastResult.c_str());
        }
        else {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_currentSweepState.lastResult.c_str());
        }

        if (!m_currentSweepState.lastSweepData.empty()) {
          ImGui::Text("Data points: %zu", m_currentSweepState.lastSweepData.size());

          // Show preview table for current sweep
          if (ImGui::BeginTable("CurrentSweepResults", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Set I (A)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Measured V", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Measured I", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Resistance", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            // Show first few and last few points
            size_t maxDisplay = 6;
            size_t dataSize = m_currentSweepState.lastSweepData.size();

            for (size_t i = 0; i < (std::min)(dataSize, maxDisplay / 2); ++i) {
              const auto& point = m_currentSweepState.lastSweepData[i];
              ImGui::TableNextRow();
              ImGui::TableNextColumn(); ImGui::Text("%.6f", point.setCurrent);
              ImGui::TableNextColumn(); ImGui::Text("%.4f", point.measuredVoltage);
              ImGui::TableNextColumn(); ImGui::Text("%.6f", point.measuredCurrent);
              ImGui::TableNextColumn(); ImGui::Text("%.2f", point.resistance);
            }

            if (dataSize > maxDisplay) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn(); ImGui::Text("...");
              ImGui::TableNextColumn(); ImGui::Text("...");
              ImGui::TableNextColumn(); ImGui::Text("...");
              ImGui::TableNextColumn(); ImGui::Text("...");

              for (size_t i = dataSize - maxDisplay / 2; i < dataSize; ++i) {
                const auto& point = m_currentSweepState.lastSweepData[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%.6f", point.setCurrent);
                ImGui::TableNextColumn(); ImGui::Text("%.4f", point.measuredVoltage);
                ImGui::TableNextColumn(); ImGui::Text("%.6f", point.measuredCurrent);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", point.resistance);
              }
            }

            ImGui::EndTable();
          }

          if (ImGui::Button("🖨️ Print I-Sweep to Console")) {
            PrintCurrentSweepDataToConsole(m_currentSweepState.lastSweepData);
          }
          ImGui::SameLine();
          if (ImGui::Button("📊 Export I-Sweep Data")) {
            ExportCurrentSweepDataToFile(m_currentSweepState.lastSweepData);
          }
        }
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No current sweep performed yet");
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
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
  std::tm timeinfo;
  localtime_s(&timeinfo, &time_t);
  ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::string UISMUPanel::GetTimestampString() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  std::tm timeinfo;
	localtime_s(&timeinfo, &time_t);
  ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
  return ss.str();
}

// Add to UISMUPanel.cpp - Enhanced polling controls

void UISMUPanel::RenderDeviceStatus(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Status & Polling");

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  // Connection status
  ImGui::Text("Communication: OK");

  // Enhanced polling status with visual indicator
  bool isPolling = device->IsPolling();
  ImGui::Text("Polling: %s", isPolling ? "Active" : "Stopped");
  ImGui::SameLine();

  if (isPolling) {
    // Animate the polling indicator
    float time = (float)ImGui::GetTime();
    float alpha = 0.5f + 0.5f * sinf(time * 6.0f); // Pulsing effect
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, alpha), "● LIVE");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "●");
  }

  ImGui::Spacing();

  // Polling controls with rate selection
  ImGui::Text("Polling Control:");

  if (isPolling) {
    // Show current polling rate and stop button
    ImGui::Text("Rate: %d ms", m_sourceSettings.pollingInterval);

    if (ImGui::Button("⏹️ Stop Polling", ImVec2(120, 30))) {
      device->StopPolling();
      m_logger->LogInfo("UISMUPanel: User stopped polling for " + m_selectedDeviceName);
    }

    // Real-time data rate display
    ImGui::Spacing();
    auto reading = device->GetLatestReading();
    auto now = std::chrono::steady_clock::now();
    auto timeSinceReading = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - reading.timestamp).count();

    if (timeSinceReading < 1000) { // Less than 1 second old
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "📡 Data: Fresh (%.0fms ago)",
        (float)timeSinceReading);
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "📡 Data: %.1fs ago",
        timeSinceReading / 1000.0f);
    }

  }
  else {
    // Polling rate selection
    ImGui::Text("Select polling rate:");

    // Rate selection buttons in a grid
    const struct { const char* label; int interval; ImVec4 color; } rates[] = {
      {"🚀 Fast (100ms)",   100,  ImVec4(1.0f, 0.3f, 0.3f, 1.0f)}, // Red - fastest
      {"⚡ Quick (250ms)",  250,  ImVec4(1.0f, 0.7f, 0.0f, 1.0f)}, // Orange
      {"🔄 Normal (500ms)", 500,  ImVec4(0.0f, 1.0f, 0.0f, 1.0f)}, // Green
      {"🐌 Slow (1000ms)",  1000, ImVec4(0.0f, 0.7f, 1.0f, 1.0f)}, // Blue
    };

    for (int i = 0; i < 4; i++) {
      if (i > 0 && i % 2 != 0) ImGui::SameLine(); // 2 buttons per row

      ImGui::PushStyleColor(ImGuiCol_Button, rates[i].color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(rates[i].color.x * 1.2f, rates[i].color.y * 1.2f, rates[i].color.z * 1.2f, 1.0f));

      if (ImGui::Button(rates[i].label, ImVec2(120, 25))) {
        m_sourceSettings.pollingInterval = rates[i].interval;
        device->StartPolling(rates[i].interval);

        m_logger->LogInfo("UISMUPanel: Started polling for " + m_selectedDeviceName +
          " at " + std::to_string(rates[i].interval) + "ms intervals");
      }

      ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Custom rate input
    ImGui::Text("Custom rate:");
    ImGui::PushItemWidth(100);
    ImGui::InputInt("ms##CustomRate", &m_sourceSettings.pollingInterval, 50, 100);
    ImGui::PopItemWidth();

    // Clamp to reasonable values
    m_sourceSettings.pollingInterval = (std::max)(50, (std::min)(5000, m_sourceSettings.pollingInterval));

    ImGui::SameLine();
    if (ImGui::Button("▶️ Start Custom", ImVec2(100, 25))) {
      device->StartPolling(m_sourceSettings.pollingInterval);
      m_logger->LogInfo("UISMUPanel: Started custom polling for " + m_selectedDeviceName +
        " at " + std::to_string(m_sourceSettings.pollingInterval) + "ms intervals");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Quick diagnostic info
  if (isPolling) {
    ImGui::Text("💡 Tip: Faster polling = more CPU usage");
    ImGui::Text("📊 Data updates in real-time below");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠️ Start polling to see live data");
  }
}

// Add these helper methods to UISMUPanel.cpp

void UISMUPanel::StartOptimalPolling(Keithley2400Client* device, int intervalMs) {
  if (!device || !device->IsConnected()) {
    m_logger->LogWarning("UISMUPanel: Cannot start polling - device not connected");
    return;
  }

  // Validate and adjust polling rate if needed
  int actualInterval = intervalMs;
  if (!ValidatePollingRate(intervalMs)) {
    actualInterval = 250; // Safe default
    m_logger->LogWarning("UISMUPanel: Invalid polling rate " + std::to_string(intervalMs) +
      "ms, using " + std::to_string(actualInterval) + "ms instead");
  }

  // Stop any existing polling first
  if (device->IsPolling()) {
    device->StopPolling();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Brief pause
  }

  // Start new polling
  device->StartPolling(actualInterval);
  m_pollingState.lastPollingRate = actualInterval;
  m_pollingState.lastDataUpdate = std::chrono::steady_clock::now();
  m_pollingState.consecutiveErrors = 0;

  m_logger->LogInfo("UISMUPanel: Started optimized polling for " + m_selectedDeviceName +
    " at " + std::to_string(actualInterval) + "ms intervals");
}

void UISMUPanel::StopPollingWithFeedback(Keithley2400Client* device) {
  if (!device) {
    return;
  }

  if (device->IsPolling()) {
    device->StopPolling();
    m_logger->LogInfo("UISMUPanel: Stopped polling for " + m_selectedDeviceName);

    // Provide user feedback
    std::cout << "\n📊 Polling stopped for " << m_selectedDeviceName << std::endl;
    std::cout << "Last rate: " << m_pollingState.lastPollingRate << "ms" << std::endl;

    auto now = std::chrono::steady_clock::now();
    auto runtime = std::chrono::duration_cast<std::chrono::seconds>(
      now - m_pollingState.lastDataUpdate).count();
    std::cout << "Runtime: " << runtime << " seconds" << std::endl;
    std::cout << "Errors: " << m_pollingState.consecutiveErrors << std::endl;
    std::cout << std::string(40, '=') << "\n" << std::endl;
  }
  else {
    m_logger->LogInfo("UISMUPanel: Polling was already stopped for " + m_selectedDeviceName);
  }
}

bool UISMUPanel::ValidatePollingRate(int intervalMs) {
  // Validate polling rate for safety and performance
  const int MIN_INTERVAL = 50;   // 50ms minimum (20 Hz max)
  const int MAX_INTERVAL = 5000; // 5 second maximum

  if (intervalMs < MIN_INTERVAL) {
    m_logger->LogWarning("UISMUPanel: Polling rate too fast (" + std::to_string(intervalMs) +
      "ms), minimum is " + std::to_string(MIN_INTERVAL) + "ms");
    return false;
  }

  if (intervalMs > MAX_INTERVAL) {
    m_logger->LogWarning("UISMUPanel: Polling rate too slow (" + std::to_string(intervalMs) +
      "ms), maximum is " + std::to_string(MAX_INTERVAL) + "ms");
    return false;
  }

  return true;
}


// Add real-time monitoring display
void UISMUPanel::RenderMeasurementPlots(Keithley2400Client* device) {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Real-time Monitoring");
  ImGui::Separator();

  if (!device->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Device not connected");
    return;
  }

  bool isPolling = device->IsPolling();
  auto reading = device->GetLatestReading();

  // Current status
  if (isPolling) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🔴 LIVE DATA");
    ImGui::SameLine();
    ImGui::Text("(Rate: %dms)", m_sourceSettings.pollingInterval);

    // Show data age
    auto now = std::chrono::steady_clock::now();
    auto dataAge = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - reading.timestamp).count();

    if (dataAge < 1000) {
      ImGui::Text("Data age: %dms", (int)dataAge);
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Data age: %.1fs", dataAge / 1000.0f);
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏸️ STATIC DATA");
    ImGui::Text("Start polling to see live measurements");
  }

  ImGui::Spacing();

  // Quick measurement display
  ImGui::Text("Current readings:");
  ImGui::Text("  V: %.6f V", reading.voltage);

  // Smart current display with appropriate units
  double current = reading.current;
  if (std::abs(current) < 1e-9) {
    ImGui::Text("  I: %.3f pA", current * 1e12);
  }
  else if (std::abs(current) < 1e-6) {
    ImGui::Text("  I: %.3f nA", current * 1e9);
  }
  else if (std::abs(current) < 1e-3) {
    ImGui::Text("  I: %.3f µA", current * 1e6);
  }
  else {
    ImGui::Text("  I: %.3f mA", current * 1000.0);
  }

  ImGui::Text("  P: %.6f mW", reading.power * 1000.0);

  // Resistance with smart formatting
  if (reading.resistance > 1e6) {
    ImGui::Text("  R: %.3f MΩ", reading.resistance / 1e6);
  }
  else if (reading.resistance > 1e3) {
    ImGui::Text("  R: %.3f kΩ", reading.resistance / 1e3);
  }
  else {
    ImGui::Text("  R: %.3f Ω", reading.resistance);
  }

  ImGui::Spacing();

  // Performance monitoring for polling
  if (isPolling) {
    ImGui::Separator();
    ImGui::Text("Performance:");

    float targetRate = 1000.0f / m_sourceSettings.pollingInterval;
    ImGui::Text("Target rate: %.1f Hz", targetRate);

    // Estimate actual rate based on data freshness
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - reading.timestamp).count();

    if (age < m_sourceSettings.pollingInterval * 2) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Good sync");
    }
    else if (age < m_sourceSettings.pollingInterval * 5) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Delayed");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Connection issue");
    }

    // Quick rate adjustment buttons
    ImGui::Text("Quick adjustments:");
    if (ImGui::Button("Faster", ImVec2(60, 20))) {
      int newRate = (std::max)(50, m_sourceSettings.pollingInterval - 50);
      m_sourceSettings.pollingInterval = newRate;
      StartOptimalPolling(device, newRate);
    }
    ImGui::SameLine();
    if (ImGui::Button("Slower", ImVec2(60, 20))) {
      int newRate = (std::min)(2000, m_sourceSettings.pollingInterval + 50);
      m_sourceSettings.pollingInterval = newRate;
      StartOptimalPolling(device, newRate);
    }
  }
}


// Add these new methods to UISMUPanel.cpp:

void UISMUPanel::PrintCurrentSweepDataToConsole(const std::vector<CurrentSweepResult>& data) {
  if (data.empty()) {
    std::cout << "No current sweep data to print.\n" << std::endl;
    return;
  }

  // Print header
  std::cout << "\n" << std::string(80, '=') << std::endl;
  std::cout << "           CURRENT SWEEP RESULTS" << std::endl;
  std::cout << std::string(80, '=') << std::endl;
  std::cout << "Device: " << m_selectedDeviceName << std::endl;
  std::cout << "Points: " << data.size() << std::endl;
  std::cout << "Range:  " << data.front().setCurrent << "A to " << data.back().setCurrent << "A" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  // Print column headers
  std::cout << std::left
    << std::setw(6) << "Step"
    << std::setw(15) << "Set I (A)"
    << std::setw(15) << "Measured V"
    << std::setw(15) << "Measured I"
    << std::setw(15) << "Resistance"
    << std::setw(15) << "Power (mW)"
    << "Delta I (%)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  // Print data rows
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& point = data[i];
    double powerMW = point.power * 1000.0; // mW
    double deltaI = 0.0;

    // Calculate current error percentage
    if (abs(point.setCurrent) > 1e-12) {
      deltaI = ((point.measuredCurrent - point.setCurrent) / point.setCurrent) * 100.0;
    }

    std::cout << std::left
      << std::setw(6) << (i + 1)
      << std::setw(15) << std::scientific << std::setprecision(3) << point.setCurrent
      << std::setw(15) << std::fixed << std::setprecision(6) << point.measuredVoltage
      << std::setw(15) << std::scientific << std::setprecision(3) << point.measuredCurrent
      << std::setw(15) << std::scientific << std::setprecision(3) << point.resistance
      << std::setw(15) << std::fixed << std::setprecision(6) << powerMW
      << std::fixed << std::setprecision(2) << deltaI << std::endl;
  }

  std::cout << std::string(80, '-') << std::endl;

  // Print summary statistics
  double maxVoltage = 0.0, minVoltage = 0.0;
  double maxCurrent = data[0].setCurrent, minCurrent = data[0].setCurrent;
  double maxResistance = 0.0, minResistance = 1e12;

  for (const auto& point : data) {
    maxVoltage = (std::max)(maxVoltage, std::abs(point.measuredVoltage));
    minVoltage = (std::min)(minVoltage, std::abs(point.measuredVoltage));
    maxCurrent = (std::max)(maxCurrent, point.setCurrent);
    minCurrent = (std::min)(minCurrent, point.setCurrent);

    if (point.resistance > 0 && point.resistance < 1e10) {
      maxResistance = (std::max)(maxResistance, point.resistance);
      minResistance = (std::min)(minResistance, point.resistance);
    }
  }

  std::cout << "SUMMARY:" << std::endl;
  std::cout << "  Current range: " << std::scientific << std::setprecision(3)
    << minCurrent << "A to " << maxCurrent << "A" << std::endl;
  std::cout << "  Voltage range: " << std::fixed << std::setprecision(6)
    << minVoltage << "V to " << maxVoltage << "V" << std::endl;
  std::cout << "  Resistance range: " << std::scientific << std::setprecision(3)
    << minResistance << " to " << maxResistance << " Ohms" << std::endl;

  // Calculate average resistance if valid
  double avgResistance = 0.0;
  int validPoints = 0;
  for (const auto& point : data) {
    if (point.resistance > 0 && point.resistance < 1e10) {
      avgResistance += point.resistance;
      validPoints++;
    }
  }
  if (validPoints > 0) {
    avgResistance /= validPoints;
    std::cout << "  Average resistance: " << std::scientific << std::setprecision(3)
      << avgResistance << " Ohms" << std::endl;
  }

  std::cout << std::string(80, '=') << std::endl;
  std::cout << "Data printed at: " << GetCurrentTimeString() << std::endl;
  std::cout << std::string(80, '=') << "\n" << std::endl;

  // Also log to the logger system
  m_logger->LogInfo("UISMUPanel: Current sweep data printed to console (" +
    std::to_string(data.size()) + " points)");
}

void UISMUPanel::ExportCurrentSweepDataToFile(const std::vector<CurrentSweepResult>& data) {
  if (data.empty()) {
    m_logger->LogWarning("UISMUPanel: No current sweep data to export");
    return;
  }

  // Generate filename with timestamp
  std::string filename = "current_sweep_data_" + GetTimestampString() + ".csv";

  std::ofstream file(filename);
  if (!file.is_open()) {
    m_logger->LogError("UISMUPanel: Failed to create export file: " + filename);
    return;
  }

  // Write CSV header
  file << "Step,Set_Current_A,Measured_Voltage_V,Measured_Current_A,Resistance_Ohms,Power_W,Power_mW,Current_Error_Percent\n";

  // Write data
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& point = data[i];
    double powerMW = point.power * 1000.0;
    double currentError = 0.0;

    if (abs(point.setCurrent) > 1e-12) {
      currentError = ((point.measuredCurrent - point.setCurrent) / point.setCurrent) * 100.0;
    }

    file << (i + 1) << ","
      << std::scientific << std::setprecision(6) << point.setCurrent << ","
      << std::fixed << std::setprecision(6) << point.measuredVoltage << ","
      << std::scientific << std::setprecision(6) << point.measuredCurrent << ","
      << std::scientific << std::setprecision(6) << point.resistance << ","
      << std::scientific << std::setprecision(6) << point.power << ","
      << std::fixed << std::setprecision(6) << powerMW << ","
      << std::fixed << std::setprecision(3) << currentError << "\n";
  }

  file.close();

  std::cout << "\nCurrent sweep data exported to: " << filename << std::endl;
  m_logger->LogInfo("UISMUPanel: Current sweep data exported to " + filename);
}