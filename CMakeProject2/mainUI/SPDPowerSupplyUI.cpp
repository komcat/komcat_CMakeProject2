
#include "SPDPowerSupplyUI.h"

SPDPowerSupplyUI::SPDPowerSupplyUI(SPDPowerSupplyManager* spdManager)
  : m_spdManager(spdManager) {
}


void SPDPowerSupplyUI::RenderUI() {
  if (!m_spdManager) return;

  //ImGui::Begin("SPD Power Supply Control", nullptr, ImGuiWindowFlags_None);

  // Get available width for 3-column layout
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float col1Width = availableWidth * 0.25f;
  float col2Width = availableWidth * 0.25f;
  float col3Width = availableWidth * 0.50f;

  // ==================== COLUMN 1 (25%) ====================
  ImGui::BeginGroup();
  ImGui::PushItemWidth(col1Width - 20);

  // Status and Connection
  RenderStatusSection();
  ImGui::Spacing();
  RenderConnectionControls();
  ImGui::Spacing();
  RenderOutputControl();

  ImGui::PopItemWidth();
  ImGui::EndGroup();

  ImGui::SameLine();

  // Vertical separator
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
  ImGui::Separator();
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

  // ==================== COLUMN 2 (25%) ====================
  ImGui::BeginGroup();
  ImGui::PushItemWidth(col2Width - 20);

  // Operating modes and polling
  RenderModeControls();
  ImGui::Spacing();
  RenderPollingControls();
  ImGui::Spacing();
  RenderLiveDataDisplay();

  ImGui::PopItemWidth();
  ImGui::EndGroup();

  ImGui::SameLine();

  // Vertical separator  
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
  ImGui::Separator();
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

  // ==================== COLUMN 3 (50%) ====================
  ImGui::BeginGroup();
  ImGui::PushItemWidth(col3Width - 20);

  // Sweep controls and results
  RenderSweepControls();
  ImGui::Spacing();
  RenderSweepResultsTable();

  ImGui::PopItemWidth();
  ImGui::EndGroup();

 // ImGui::End();
}



void SPDPowerSupplyUI::RenderStatusSection() {
  ImGui::Text("Status:");
  ImGui::SameLine();

  int connectedCount = m_spdManager->GetConnectedCount();
  int totalCount = static_cast<int>(m_spdManager->GetDeviceNames().size());

  if (connectedCount > 0) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d/%d devices connected", connectedCount, totalCount);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No devices connected");
  }

  ImGui::Text("Polling:");
  ImGui::SameLine();
  bool isPolling = m_spdManager->IsPollingActive();
  ImGui::TextColored(isPolling ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
    isPolling ? "Active" : "Stopped");
}

void SPDPowerSupplyUI::RenderConnectionControls() {
  if (ImGui::Button("Connect All", ImVec2(120, 30))) {
    int connected = m_spdManager->ConnectAll();
    Logger::GetInstance()->LogInfo("Connected " + std::to_string(connected) + " SPD devices");
  }

  ImGui::SameLine();
  if (ImGui::Button("Disconnect All", ImVec2(120, 30))) {
    m_spdManager->DisconnectAll();
    Logger::GetInstance()->LogInfo("Disconnected all SPD devices");
  }
}

void SPDPowerSupplyUI::RenderOutputControl() {
  // Use button instead of checkbox for more prominent control
  ImVec4 buttonColor = m_outputEnabled ?
    ImVec4(0.0f, 0.8f, 0.0f, 1.0f) :  // Green when enabled
    ImVec4(0.8f, 0.0f, 0.0f, 1.0f);   // Red when disabled

  ImVec4 hoverColor = m_outputEnabled ?
    ImVec4(0.0f, 0.9f, 0.0f, 1.0f) :  // Brighter green on hover
    ImVec4(0.9f, 0.0f, 0.0f, 1.0f);   // Brighter red on hover

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);

  std::string buttonText = m_outputEnabled ? "OUTPUT: ON" : "OUTPUT: OFF";
  if (ImGui::Button(buttonText.c_str(), ImVec2(150, 40))) {
    m_outputEnabled = !m_outputEnabled;
    m_spdManager->SetAllOutputs(m_outputEnabled);
    Logger::GetInstance()->LogInfo("Set all outputs: " + std::string(m_outputEnabled ? "ON" : "OFF"));
  }

  ImGui::PopStyleColor(2);
}

void SPDPowerSupplyUI::RenderModeControls() {
  ImGui::Text("Operating Mode:");

  // Constant Voltage Mode
  ImGui::Text("Constant Voltage (CV):");
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##cv_voltage", &m_cvVoltage, 0.0f, 30.0f, "%.2f V")) {
    // Value changed via slider
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double-click to manually enter value");
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##cv_current_limit", &m_cvCurrentLimit, 0.0f, 5.0f, "%.2f A limit")) {
    // Value changed via slider
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double-click to manually enter value");
  }

  ImGui::SameLine();
  if (ImGui::Button("Set CV", ImVec2(80, 0))) {
    m_spdManager->SetConstantVoltageMode(m_cvVoltage, m_cvCurrentLimit);
    Logger::GetInstance()->LogInfo("Set CV mode: " + std::to_string(m_cvVoltage) + "V, " +
      std::to_string(m_cvCurrentLimit) + "A limit");
  }

  // Constant Current Mode
  ImGui::Text("Constant Current (CC):");
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##cc_current", &m_ccCurrent, 0.0f, 5.0f, "%.2f A")) {
    // Value changed via slider
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double-click to manually enter value");
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##cc_voltage_limit", &m_ccVoltageLimit, 0.0f, 30.0f, "%.2f V limit")) {
    // Value changed via slider
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double-click to manually enter value");
  }

  ImGui::SameLine();
  if (ImGui::Button("Set CC", ImVec2(80, 0))) {
    m_spdManager->SetConstantCurrentMode(m_ccCurrent, m_ccVoltageLimit);
    Logger::GetInstance()->LogInfo("Set CC mode: " + std::to_string(m_ccCurrent) + "A, " +
      std::to_string(m_ccVoltageLimit) + "V limit");
  }
}

void SPDPowerSupplyUI::RenderPollingControls() {
  ImGui::Text("Data Polling:");

  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderInt("Interval (ms)", &m_pollingInterval, 100, 5000)) {
    // Value changed via slider
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double-click to manually enter value");
  }

  bool isPolling = m_spdManager->IsPollingActive();
  if (!isPolling) {
    if (ImGui::Button("Start Polling", ImVec2(120, 30))) {
      m_spdManager->StartAllPolling(m_pollingInterval);
      Logger::GetInstance()->LogInfo("Started SPD polling at " + std::to_string(m_pollingInterval) + "ms");
    }
  }
  else {
    if (ImGui::Button("Stop Polling", ImVec2(120, 30))) {
      m_spdManager->StopAllPolling();
      Logger::GetInstance()->LogInfo("Stopped SPD polling");
    }
  }

  // Show current polling status
  if (isPolling) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Polling Active");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Polling Stopped");
  }
}

void SPDPowerSupplyUI::RenderLiveDataDisplay() {
  int connectedCount = m_spdManager->GetConnectedCount();
  if (connectedCount == 0) {
    return;
  }

  ImGui::Text("Live Data:");

  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "GlobalDataStore not available");
    return;
  }

  auto channels = dataStore->GetAvailableChannels();
  bool foundSPDData = false;

  for (const auto& channel : channels) {
    if (channel.find("SPD-") == 0) {
      foundSPDData = true;
      float value = dataStore->GetValue(channel);

      ImVec4 color = GetChannelColor(channel, value);
      ImGui::TextColored(color, "%s: %.3f", channel.c_str(), value);
    }
  }

  if (!foundSPDData) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No SPD data - start polling to see live values");
  }
}

ImVec4 SPDPowerSupplyUI::GetChannelColor(const std::string& channel, float value) const {
  if (channel.find("-Voltage") != std::string::npos) {
    return ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan for voltage
  }
  else if (channel.find("-Current") != std::string::npos) {
    return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for current
  }
  else if (channel.find("-Power") != std::string::npos) {
    return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange for power
  }
  else if (channel.find("-Output") != std::string::npos) {
    return value > 0.5f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Green/Red
  }
  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White default
}




// ========================================
// Updated RenderSweepControls() method:

void SPDPowerSupplyUI::RenderSweepControls() {
  ImGui::Text("Sweep Functions:");

  if (ImGui::BeginTabBar("SweepTabs", ImGuiTabBarFlags_None)) {

    // VOLTAGE SWEEP TAB
    if (ImGui::BeginTabItem("⚡ CV Sweep")) {
      ImGui::Spacing();

      // Compact voltage sweep parameters
      ImGui::Text("Voltage Range:");
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Start V", &m_sweepStartV, 0.0f, 30.0f, "%.2f");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Stop V", &m_sweepStopV, 0.0f, 30.0f, "%.2f");

      ImGui::SetNextItemWidth(80);
      ImGui::SliderInt("Steps", &m_sweepSteps, 2, 50);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("I Limit", &m_sweepCurrentLimit, 0.1f, 5.0f, "%.2f A");

      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Delay", &m_sweepDelay, 10.0f, 2000.0f, "%.0f ms");

      ImGui::Spacing();

      // Sweep buttons - compact layout
      if (ImGui::Button("🚀 Start CV Sweep", ImVec2(150, 30))) {
        if (m_spdManager) {
          auto deviceNames = m_spdManager->GetDeviceNames();
          if (!deviceNames.empty()) {
            m_lastSweepResults.clear();
            m_lastSweepDevice = deviceNames[0];
            m_lastSweepType = "Voltage";

            bool success = m_spdManager->PerformVoltageSweep(
              deviceNames[0], 1, m_sweepStartV, m_sweepStopV, m_sweepSteps,
              m_sweepCurrentLimit, m_sweepDelay, m_lastSweepResults
            );

            std::string status = success ? "completed" : "failed";
            Logger::GetInstance()->LogInfo("CV Sweep " + status + " - " +
              std::to_string(m_lastSweepResults.size()) + " points");
          }
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("🔄 All Devices", ImVec2(120, 30))) {
        if (m_spdManager) {
          std::unordered_map<std::string, std::vector<PowerSupply::SPDSweepResult>> allResults;
          int successCount = m_spdManager->PerformVoltageSweepAll(
            1, m_sweepStartV, m_sweepStopV, m_sweepSteps,
            m_sweepCurrentLimit, m_sweepDelay, allResults
          );

          Logger::GetInstance()->LogInfo("CV Sweep completed on " +
            std::to_string(successCount) + " devices");
        }
      }

      ImGui::EndTabItem();
    }

    // CURRENT SWEEP TAB
    if (ImGui::BeginTabItem("🔋 CC Sweep")) {
      ImGui::Spacing();

      // Compact current sweep parameters
      ImGui::Text("Current Range:");
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Start A", &m_sweepStartA, 0.0f, 5.0f, "%.3f");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Stop A", &m_sweepStopA, 0.0f, 5.0f, "%.3f");

      ImGui::SetNextItemWidth(80);
      ImGui::SliderInt("Steps##CC", &m_sweepSteps, 2, 50);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("V Limit", &m_sweepVoltageLimit, 1.0f, 30.0f, "%.1f V");

      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Delay##CC", &m_sweepDelay, 10.0f, 2000.0f, "%.0f ms");

      ImGui::Spacing();

      // Sweep buttons - compact layout
      if (ImGui::Button("🚀 Start CC Sweep", ImVec2(150, 30))) {
        if (m_spdManager) {
          auto deviceNames = m_spdManager->GetDeviceNames();
          if (!deviceNames.empty()) {
            m_lastSweepResults.clear();
            m_lastSweepDevice = deviceNames[0];
            m_lastSweepType = "Current";

            bool success = m_spdManager->PerformCurrentSweep(
              deviceNames[0], 1, m_sweepStartA, m_sweepStopA, m_sweepSteps,
              m_sweepVoltageLimit, m_sweepDelay, m_lastSweepResults
            );

            std::string status = success ? "completed" : "failed";
            Logger::GetInstance()->LogInfo("CC Sweep " + status + " - " +
              std::to_string(m_lastSweepResults.size()) + " points");
          }
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("🔄 All Devices##CC", ImVec2(120, 30))) {
        if (m_spdManager) {
          std::unordered_map<std::string, std::vector<PowerSupply::SPDSweepResult>> allResults;
          int successCount = m_spdManager->PerformCurrentSweepAll(
            1, m_sweepStartA, m_sweepStopA, m_sweepSteps,
            m_sweepVoltageLimit, m_sweepDelay, allResults
          );

          Logger::GetInstance()->LogInfo("CC Sweep completed on " +
            std::to_string(successCount) + " devices");
        }
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

// ========================================
// New method to add to SPDPowerSupplyUI.cpp:

void SPDPowerSupplyUI::RenderSweepResultsTable() {
  ImGui::Text("Sweep Results:");

  if (m_lastSweepResults.empty()) {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No sweep data available");
    ImGui::Text("Run a sweep to see results here");
    return;
  }

  // Header info
  ImGui::Text("Device: %s | Type: %s Sweep | Points: %zu",
    m_lastSweepDevice.c_str(),
    m_lastSweepType.c_str(),
    m_lastSweepResults.size());

  ImGui::Spacing();

  // Results table
  if (ImGui::BeginTable("SweepResults", 4,
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {

    // Table headers
    ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn(m_lastSweepType == "Voltage" ? "Set V" : "Set A",
      ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Meas V", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Meas A", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableHeadersRow();

    // Table data
    for (size_t i = 0; i < m_lastSweepResults.size(); ++i) {
      const auto& result = m_lastSweepResults[i];

      ImGui::TableNextRow();

      // Step number
      ImGui::TableNextColumn();
      ImGui::Text("%zu", i + 1);

      // Set value
      ImGui::TableNextColumn();
      if (m_lastSweepType == "Voltage") {
        ImGui::Text("%.3f V", result.setValue);
      }
      else {
        ImGui::Text("%.3f A", result.setValue);
      }

      // Measured voltage
      ImGui::TableNextColumn();
      ImGui::Text("%.3f V", result.measuredVoltage);

      // Measured current
      ImGui::TableNextColumn();
      ImGui::Text("%.3f A", result.measuredCurrent);
    }

    ImGui::EndTable();
  }

  ImGui::Spacing();

  // Action buttons for results
  if (ImGui::Button("📋 Copy Results", ImVec2(120, 25))) {
    // Create CSV-style text for clipboard
    std::string csvData = "Step," + m_lastSweepType + ",MeasuredV,MeasuredA\n";
    for (size_t i = 0; i < m_lastSweepResults.size(); ++i) {
      const auto& result = m_lastSweepResults[i];
      csvData += std::to_string(i + 1) + "," +
        std::to_string(result.setValue) + "," +
        std::to_string(result.measuredVoltage) + "," +
        std::to_string(result.measuredCurrent) + "\n";
    }
    ImGui::SetClipboardText(csvData.c_str());
    Logger::GetInstance()->LogInfo("Sweep results copied to clipboard");
  }

  ImGui::SameLine();
  if (ImGui::Button("🗑️ Clear", ImVec2(80, 25))) {
    m_lastSweepResults.clear();
    m_lastSweepDevice.clear();
    m_lastSweepType.clear();
  }
}