// PowerSupplyTestUI.cpp
#include "PowerSupplyTestUI.h"
#include "include/logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

PowerSupplyTestUI::PowerSupplyTestUI() {
  Logger* logger = Logger::GetInstance();

  // Initialize manager and storage
  m_manager = std::make_shared<PowerSupplyManager>();
  m_storage = std::make_shared<FileResultStorage>();

  // Initialize storage with test directory
  if (m_storage->Initialize("./power_supply_test_data")) {
    m_manager->SetResultStorage(m_storage);
    AddLogMessage("Storage initialized at ./power_supply_test_data");
  }
  else {
    AddLogMessage("Failed to initialize storage");
  }

  // Set thread-safe mode
  m_manager->SetThreadSafe(true);

  logger->LogInfo("PowerSupplyTestUI initialized");
}

PowerSupplyTestUI::~PowerSupplyTestUI() {
  // Cleanup: turn off and disconnect all devices
  if (m_manager) {
    m_manager->TurnOffAll();
    m_manager->DisconnectAllDevices();
  }
}
void PowerSupplyTestUI::Render() {
  if (!m_showWindow) return;

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_windowName.c_str(), &m_showWindow)) {

    // Top toolbar
    if (ImGui::Button("Refresh")) {
      RefreshDeviceList();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Refresh", &m_state.autoRefresh);
    ImGui::SameLine();
    ImGui::Text("| Devices: %zu", m_deviceIds.size());

    ImGui::Separator();

    // Main content in columns
    ImGui::Columns(2, "MainColumns", true);

    // Left column - Controls
    ImGui::BeginChild("ControlPanel", ImVec2(0, -100), true);
    RenderDeviceManagement();
    ImGui::Separator();
    RenderDeviceControl();
    ImGui::Separator();
    RenderSweepControl();
    ImGui::EndChild();

    ImGui::NextColumn();

    // Right column - Display
    ImGui::BeginChild("DisplayPanel", ImVec2(0, -100), true);
    RenderMeasurementDisplay();
    ImGui::Separator();
    RenderStatusDisplay();
    ImGui::EndChild();

    ImGui::Columns(1);

    // Bottom log area
    ImGui::BeginChild("LogArea", ImVec2(0, 0), true);
    ImGui::Text("Log Messages:");
    ImGui::Separator();
    for (const auto& msg : m_state.display.logMessages) {
      ImGui::TextWrapped("%s", msg.c_str());
    }
    ImGui::EndChild();
  }
  ImGui::End();

  // Additional windows
  if (m_state.display.showStorageViewer) {
    RenderStorageViewer();
  }

  if (m_state.display.showSweepResults) {
    RenderSweepResultsWindow();
  }

  // Auto refresh
  if (m_state.autoRefresh) {
    static auto lastRefresh = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefresh).count() > 500) {
      UpdateMeasurement(m_state.deviceId);
      lastRefresh = now;
    }
  }

  // Update sweep progress if running
  if (m_state.sweep.running) {
    UpdateSweepProgress();
  }


}

void PowerSupplyTestUI::RenderDeviceManagement() {
  ImGui::Text("Device Management");
  ImGui::Separator();

  if (ImGui::Button("Add Mock Device")) {
    std::string deviceName = "PS" + std::to_string(m_nextDeviceNum);
    auto mock = std::make_shared<MockPowerSupplyDevice>(
      "MockPS" + std::to_string(m_nextDeviceNum),
      m_nextDeviceNum,
      "MOCK-" + std::to_string(1000 + m_nextDeviceNum)
    );

    if (m_manager->AddDevice(mock, deviceName)) {
      AddLogMessage("Added device: " + deviceName);
      m_deviceIds.push_back(deviceName);
      strcpy_s(m_state.deviceId, deviceName.c_str());
      m_nextDeviceNum++;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Connect All")) {
    auto result = m_manager->ConnectAllDevices();
    AddLogMessage("Connected " + std::to_string(result.successCount) +
      " devices, " + std::to_string(result.failureCount) + " failed");
  }

  ImGui::SameLine();
  if (ImGui::Button("Disconnect All")) {
    auto result = m_manager->DisconnectAllDevices();
    AddLogMessage("Disconnected " + std::to_string(result.successCount) + " devices");
  }

  // Device selector
  if (ImGui::BeginCombo("Select Device", m_state.deviceId)) {
    for (const auto& id : m_deviceIds) {
      bool isSelected = (id == std::string(m_state.deviceId));
      if (ImGui::Selectable(id.c_str(), isSelected)) {
        strcpy_s(m_state.deviceId, id.c_str());
      }
    }
    ImGui::EndCombo();
  }
}

void PowerSupplyTestUI::RenderDeviceControl() {
  ImGui::Text("Device Control");
  ImGui::Separator();

  ImGui::InputText("Device ID", m_state.deviceId, sizeof(m_state.deviceId));
  ImGui::InputInt("Channel", &m_state.channel);
  ImGui::DragFloat("Voltage (V)", &m_state.voltage, 0.1f, 0.0f, 30.0f);
  ImGui::DragFloat("Current (A)", &m_state.current, 0.01f, 0.0f, 5.0f);

  // Mode selection
  if (ImGui::Button("Set CV Mode")) {
    if (m_manager->SetModeConstantVoltage(m_state.deviceId, m_state.channel)) {
      AddLogMessage("Set CV mode for " + std::string(m_state.deviceId));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Set CC Mode")) {
    if (m_manager->SetModeConstantCurrent(m_state.deviceId, m_state.channel)) {
      AddLogMessage("Set CC mode for " + std::string(m_state.deviceId));
    }
  }

  // Set values
  if (ImGui::Button("Set Voltage")) {
    if (m_manager->SetVoltage(m_state.deviceId, m_state.voltage, m_state.channel)) {
      AddLogMessage("Set voltage to " + std::to_string(m_state.voltage) + "V");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Set Current")) {
    if (m_manager->SetCurrent(m_state.deviceId, m_state.current, m_state.channel)) {
      AddLogMessage("Set current to " + std::to_string(m_state.current) + "A");
    }
  }

  // Output control
  if (ImGui::Button("Turn On")) {
    if (m_manager->TurnOn(m_state.deviceId, m_state.channel)) {
      AddLogMessage("Output ON for " + std::string(m_state.deviceId));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Turn Off")) {
    if (m_manager->TurnOff(m_state.deviceId, m_state.channel)) {
      AddLogMessage("Output OFF for " + std::string(m_state.deviceId));
    }
  }

  // Batch operations
  if (ImGui::Button("All On")) {
    auto result = m_manager->TurnOnAll(m_state.channel);
    AddLogMessage("Turned on " + std::to_string(result.successCount) + " devices");
  }
  ImGui::SameLine();
  if (ImGui::Button("All Off")) {
    auto result = m_manager->TurnOffAll(m_state.channel);
    AddLogMessage("Turned off " + std::to_string(result.successCount) + " devices");
  }
}

void PowerSupplyTestUI::RenderMeasurementDisplay() {
  ImGui::Text("Measurements");
  ImGui::Separator();

  if (ImGui::Button("Read Once")) {
    UpdateMeasurement(m_state.deviceId);
  }
  ImGui::SameLine();
  if (ImGui::Button("Store Measurement")) {
    if (m_manager->StoreCurrentMeasurement(m_state.deviceId, "manual_test", m_state.channel)) {
      AddLogMessage("Measurement stored");
    }
  }

  // Display last measurement
  ImGui::Text("Last Measurement:");
  ImGui::Text("  Voltage: %.3f V", m_state.display.lastMeasurement.voltage);
  ImGui::Text("  Current: %.3f A", m_state.display.lastMeasurement.current);
  ImGui::Text("  Power: %.3f W",
    m_state.display.lastMeasurement.voltage * m_state.display.lastMeasurement.current);

  // Time since last update
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
    now - m_state.display.lastUpdate).count();
  ImGui::Text("  Updated: %lld seconds ago", elapsed);

  ImGui::Separator();

  // Storage operations
  if (ImGui::Button("View Storage")) {
    m_state.display.showStorageViewer = !m_state.display.showStorageViewer;
  }
  ImGui::SameLine();
  if (ImGui::Button("Export Data")) {
    IResultStorage::QueryFilter filter;
    filter.deviceType = "PowerSupply";
    if (m_storage->ExportToFile("power_supply_export.json", filter)) {
      AddLogMessage("Data exported to power_supply_export.json");
    }
  }
}

void PowerSupplyTestUI::RenderSweepControl() {
  ImGui::Text("Sweep Control");
  ImGui::Separator();

  const char* modes[] = { "Constant Voltage", "Constant Current" };
  ImGui::Combo("Mode", &m_state.sweep.mode, modes, 2);

  ImGui::DragFloat("Start", &m_state.sweep.startValue, 0.1f, 0.0f, 30.0f);
  ImGui::DragFloat("End", &m_state.sweep.endValue, 0.1f, 0.0f, 30.0f);
  ImGui::DragFloat("Step", &m_state.sweep.stepSize, 0.01f, 0.01f, 5.0f);
  ImGui::DragInt("Delay (ms)", &m_state.sweep.delayMs, 10, 10, 1000);

  if (!m_state.sweep.running) {
    if (ImGui::Button("Start Sweep")) {
      StartSweep();
    }
  }
  else {
    if (ImGui::Button("Stop Sweep")) {
      m_manager->StopSweep(m_state.deviceId);
      m_state.sweep.running = false;
      AddLogMessage("Sweep stopped");
    }

    // Progress bar
    ImGui::ProgressBar(m_state.sweep.progress, ImVec2(-1, 0));
  }

  if (ImGui::Button("View Last Results")) {
    m_state.display.showSweepResults = !m_state.display.showSweepResults;
  }
}

void PowerSupplyTestUI::RenderStatusDisplay() {
  ImGui::Text("Device Status");
  ImGui::Separator();

  auto allStatus = m_manager->GetAllDeviceStatus();

  for (const auto& [id, status] : allStatus) {
    ImGui::Text("%s:", id.c_str());
    ImGui::Indent();
    ImGui::Text("Connected: %s", status.connected ? "Yes" : "No");
    ImGui::Text("Sweep: %s (%.1f%%)",
      status.sweepRunning ? "Running" : "Idle",
      status.sweepProgress * 100);

    for (const auto& [ch, on] : status.channelOutputOn) {
      ImGui::Text("Ch%d: %s", ch, on ? "ON" : "OFF");
    }
    ImGui::Unindent();
  }
}

void PowerSupplyTestUI::RenderStorageViewer() {
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Storage Viewer", &m_state.display.showStorageViewer)) {

    // Query controls
    static char deviceFilter[64] = "";
    static int maxResults = 20;

    ImGui::InputText("Device Filter", deviceFilter, sizeof(deviceFilter));
    ImGui::InputInt("Max Results", &maxResults);

    if (ImGui::Button("Query")) {
      IResultStorage::QueryFilter filter;
      if (strlen(deviceFilter) > 0) {
        filter.deviceId = deviceFilter;
      }
      filter.deviceType = "PowerSupply";
      filter.maxResults = maxResults;

      auto results = m_manager->QueryStoredResults(filter);

      ImGui::Text("Found %zu records", results.size());
      ImGui::Separator();

      // Display results in a table
      if (ImGui::BeginTable("ResultsTable", 5)) {
        ImGui::TableSetupColumn("Device");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Label");
        ImGui::TableSetupColumn("Voltage");
        ImGui::TableSetupColumn("Current");
        ImGui::TableHeadersRow();

        for (const auto& record : results) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s", record.deviceId.c_str());
          ImGui::TableNextColumn();
          ImGui::Text("%s", record.resultType.c_str());
          ImGui::TableNextColumn();
          ImGui::Text("%s", record.label.c_str());

          ImGui::TableNextColumn();
          auto vIt = record.numericValues.find("voltage");
          if (vIt != record.numericValues.end()) {
            ImGui::Text("%.3f", vIt->second);
          }

          ImGui::TableNextColumn();
          auto cIt = record.numericValues.find("current");
          if (cIt != record.numericValues.end()) {
            ImGui::Text("%.3f", cIt->second);
          }
        }

        ImGui::EndTable();
      }
    }

    ImGui::Separator();
    size_t storageSize = m_storage->GetStorageSize();
    ImGui::Text("Storage size: %.2f KB", storageSize / 1024.0);
  }
  ImGui::End();
}

void PowerSupplyTestUI::RenderSweepResultsWindow() {
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Sweep Results", &m_state.display.showSweepResults)) {

    if (m_lastSweepResult.completed) {
      ImGui::Text("Sweep completed successfully");
      ImGui::Text("Points: %zu", m_lastSweepResult.measurements.size());
      ImGui::Separator();

      // Display results in table
      if (ImGui::BeginTable("SweepTable", 3)) {
        ImGui::TableSetupColumn("Set Value");
        ImGui::TableSetupColumn("Voltage (V)");
        ImGui::TableSetupColumn("Current (A)");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < m_lastSweepResult.measurements.size(); ++i) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%.3f", m_lastSweepResult.sweepValues[i]);
          ImGui::TableNextColumn();
          ImGui::Text("%.3f", m_lastSweepResult.measurements[i].voltage);
          ImGui::TableNextColumn();
          ImGui::Text("%.3f", m_lastSweepResult.measurements[i].current);
        }

        ImGui::EndTable();
      }

      if (ImGui::Button("Store Results")) {
        if (m_manager->StoreSweepResults(m_state.deviceId, "sweep_test")) {
          AddLogMessage("Sweep results stored");
        }
      }
    }
    else {
      ImGui::Text("No sweep results available");
      if (!m_lastSweepResult.errorMessage.empty()) {
        ImGui::Text("Error: %s", m_lastSweepResult.errorMessage.c_str());
      }
    }
  }
  ImGui::End();
}

void PowerSupplyTestUI::AddLogMessage(const std::string& msg) {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
  ss << " - " << msg;

  m_state.display.logMessages.push_back(ss.str());

  // Keep only last 50 messages
  if (m_state.display.logMessages.size() > 50) {
    m_state.display.logMessages.erase(m_state.display.logMessages.begin());
  }

  // Also log to main logger
  Logger::GetInstance()->LogInfo("PowerSupplyTest: " + msg);
}

void PowerSupplyTestUI::RefreshDeviceList() {
  m_deviceIds = m_manager->GetDeviceIds();
  AddLogMessage("Refreshed device list: " + std::to_string(m_deviceIds.size()) + " devices");
}

void PowerSupplyTestUI::UpdateMeasurement(const std::string& deviceId) {
  m_state.display.lastMeasurement = m_manager->ReadMeasurement(deviceId, m_state.channel);
  m_state.display.lastUpdate = std::chrono::steady_clock::now();
}

void PowerSupplyTestUI::StartSweep() {
  IPowerSupplyDevice::SweepConfig config;
  config.mode = (m_state.sweep.mode == 0) ?
    IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE :
    IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_CURRENT;
  config.startValue = m_state.sweep.startValue;
  config.endValue = m_state.sweep.endValue;
  config.stepSize = m_state.sweep.stepSize;
  config.delayMs = m_state.sweep.delayMs;
  config.channel = m_state.channel;

  if (m_manager->StartSweep(m_state.deviceId, config)) {
    m_state.sweep.running = true;
    m_state.sweep.progress = 0.0f;
    AddLogMessage("Sweep started");
  }
  else {
    AddLogMessage("Failed to start sweep");
  }
}

void PowerSupplyTestUI::UpdateSweepProgress() {
  if (m_manager->IsSweepRunning(m_state.deviceId)) {
    m_state.sweep.progress = m_manager->GetSweepProgress(m_state.deviceId);
  }
  else {
    // Sweep finished
    m_state.sweep.running = false;
    m_lastSweepResult = m_manager->GetSweepResults(m_state.deviceId);
    if (m_lastSweepResult.completed) {
      AddLogMessage("Sweep completed with " +
        std::to_string(m_lastSweepResult.measurements.size()) + " points");

      // AUTO-STORE THE SWEEP RESULTS
      if (m_manager->StoreSweepResults(m_state.deviceId, "auto_sweep")) {
        AddLogMessage("Sweep results auto-saved to storage");
      }
      else {
        AddLogMessage("Failed to auto-save sweep results");
      }
    }
  }
}