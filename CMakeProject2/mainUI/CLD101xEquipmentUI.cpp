// ==============================================================================
// IMPLEMENTATION FILE: CLD101xEquipmentUI.cpp - Updated with 3-Column Layout
// ==============================================================================
#include "CLD101xEquipmentUI.h"
#include "include/cld101x_manager.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <thread>

// Preset current options
static const char* currentOptions[] = {
    "110 mA", "120 mA", "130 mA", "140 mA", "150 mA",
    "160 mA", "170 mA", "180 mA", "190 mA", "200 mA",
    "210 mA", "220 mA", "230 mA", "240 mA", "250 mA"
};

CLD101xEquipmentUI::CLD101xEquipmentUI() {
  UpdateMAFromCurrent();
  AddDebugOutput("CLD101xEquipmentUI initialized");
}

void CLD101xEquipmentUI::SetCLD101xManager(CLD101xManager* manager) {
  std::cout << "CLD101xEquipmentUI: SetCLD101xManager called with pointer: " << manager << std::endl;
  m_cld101xManager = manager;

  if (manager) {
    AddDebugOutput("CLD101xManager successfully connected to UI");
    std::cout << "CLD101xEquipmentUI: Manager successfully set" << std::endl;
    OnGetStatus();
  }
  else {
    AddDebugOutput("WARNING: CLD101xManager is null!");
    std::cout << "CLD101xEquipmentUI: WARNING - Manager is null!" << std::endl;
  }
}



void CLD101xEquipmentUI::UpdateUIFromPollingCache() {
  if (!m_isConnected || !IsManagerAvailable()) {
    return;
  }

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client && client->IsPolling()) {
      // Get fresh values from polling cache
      float pollingTemp = client->GetLatestTemperature();
      float pollingCurrent = client->GetLatestLaserCurrent();

      // Update measurements
      if (std::abs(pollingTemp - m_currentTemperature) > 0.001f) {
        m_currentTemperature = pollingTemp;
      }

      if (std::abs(pollingCurrent - m_currentLaserCurrent) > 0.000001f) {
        m_currentLaserCurrent = pollingCurrent;
      }

      // **FIX: Less aggressive status checking to prevent flipping**
      static auto lastStatusCheck = std::chrono::steady_clock::now();
      auto now = std::chrono::steady_clock::now();

      // **INCREASED INTERVAL: Check status less frequently (every 5 seconds instead of 2)**
      if (now - lastStatusCheck > std::chrono::seconds(5)) {

        // **TRACK RECENT COMMANDS: Don't interfere with recent user actions**
        static auto lastUserCommand = std::chrono::steady_clock::now();
        static bool recentCommandFlag = false;

        // If this is called right after OnTECOn/OnTECOff/OnLaserOn/OnLaserOff,
        // skip status validation for a few seconds to let the command settle
        auto timeSinceLastCommand = now - lastUserCommand;
        if (timeSinceLastCommand < std::chrono::seconds(3)) {
          // Skip validation - recent command might still be settling
          return;
        }

        // Get hardware status
        bool hardwareLaserStatus = client->GetLaserStatus();
        bool hardwareTECStatus = client->GetTECStatus();

        // **MORE CONSERVATIVE VALIDATION: Only change if very confident**

        // Laser validation - only change if measurement strongly disagrees
        bool measurementSuggestsLaserOn = (pollingCurrent > 0.01f); // Raised threshold
        bool laserMismatch = (hardwareLaserStatus != measurementSuggestsLaserOn);

        if (laserMismatch) {
          // Only act on very clear mismatches
          if (hardwareLaserStatus && pollingCurrent < 0.001f) {
            // Laser shows ON but absolutely no current
            AddDebugOutput("CLEAR MISMATCH: Laser shows ON but current is " +
              std::to_string(pollingCurrent) + "A - investigating...");

            // Force fresh status sync but don't auto-correct immediately
            client->SyncHardwareStatus();
            hardwareLaserStatus = client->GetLaserStatus();
          }
          else if (!hardwareLaserStatus && pollingCurrent > 0.1f) {
            // Laser shows OFF but high current
            AddDebugOutput("CLEAR MISMATCH: Laser shows OFF but current is " +
              std::to_string(pollingCurrent) + "A - investigating...");

            client->SyncHardwareStatus();
            hardwareLaserStatus = client->GetLaserStatus();
          }
          // Otherwise, trust the hardware status
        }

        // **TEC VALIDATION: Be even more conservative**
        // Since TEC behavior seems complex, only update UI if hardware status actually changed
        // Don't try to infer from temperature

        // Update UI status only if it differs from hardware AND we're confident
        if (m_laserOn != hardwareLaserStatus) {
          AddDebugOutput("UI Laser status updated: " + std::string(m_laserOn ? "ON" : "OFF") +
            " -> " + std::string(hardwareLaserStatus ? "ON" : "OFF"));
          m_laserOn = hardwareLaserStatus;
        }

        if (m_tecOn != hardwareTECStatus) {
          AddDebugOutput("UI TEC status updated: " + std::string(m_tecOn ? "ON" : "OFF") +
            " -> " + std::string(hardwareTECStatus ? "ON" : "OFF"));
          m_tecOn = hardwareTECStatus;
        }

        lastStatusCheck = now;
      }
    }
  }
  catch (const std::exception& e) {
    // Silently ignore errors in UI update to avoid spam
    static auto lastErrorLog = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    // Log errors only once per 10 seconds
    if (now - lastErrorLog > std::chrono::seconds(10)) {
      AddDebugOutput("UI update error: " + std::string(e.what()));
      lastErrorLog = now;
    }
  }
}




void CLD101xEquipmentUI::Render() {
  if (!m_showWindow) {
    return;
  }

  // Automatic UI updates from polling data
  UpdateUIFromPollingCache();

  // Header
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("CLD101x Equipment Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Laser and TEC control for CLD101x equipment");
  ImGui::Separator();
  ImGui::Spacing();

  // Calculate content size for 3-column layout: 30% / 35% / 35%
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.30f;
  float middlePanelWidth = contentSize.x * 0.35f;
  float rightPanelWidth = contentSize.x * 0.35f;

  // Left Panel - Connection & Status (30% width)
  ImGui::BeginChild("LeftCLD101xPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Laser Controls (35% width)
  ImGui::BeginChild("MiddleCLD101xPanel", ImVec2(middlePanelWidth, contentSize.y), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - TEC & Debug (35% width)
  ImGui::BeginChild("RightCLD101xPanel", ImVec2(rightPanelWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();

  // Enhanced auto-refresh with polling awareness
  if (m_autoRefresh && m_isConnected) {
    float currentTime = ImGui::GetTime();
    if (currentTime - m_lastStatusUpdate > m_refreshRate) {
      OnGetStatus();
      m_lastStatusUpdate = currentTime;
    }
  }
}

void CLD101xEquipmentUI::RenderLeftPanel() {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Connection & Status");
  ImGui::Separator();

  // Connection section
  RenderConnectionSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Current readings section
  RenderCurrentReadingsSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Polling controls section
  RenderPollingControlsSection();
}

void CLD101xEquipmentUI::RenderMiddlePanel() {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Laser Control");
  ImGui::Separator();

  // Laser control section
  RenderLaserControlSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Current control section
  RenderCurrentControlSection();
}

void CLD101xEquipmentUI::RenderRightPanel() {
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "TEC & Diagnostics");
  ImGui::Separator();

  // TEC control section
  RenderTECControlSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Debug section (compact version)
  RenderCompactDebugSection();
}

void CLD101xEquipmentUI::RenderConnectionSection() {
  ImGui::Text("Manager:");
  ImGui::SameLine();
  if (IsManagerAvailable()) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Available");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NOT AVAILABLE");
  }

  // Connection status
  ImGui::Text("Connection:");
  ImGui::SameLine();
  if (m_isConnected) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    if (m_lastConnectionTime > 0) {
      ImGui::Text("(%.1fs ago)", (ImGui::GetTime() - m_lastConnectionTime));
    }
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Connected");
  }

  // Polling status
  if (m_isConnected && IsManagerAvailable()) {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      ImGui::Text("Polling:");
      ImGui::SameLine();
      if (client->IsPolling()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        auto lastUpdate = client->GetLastUpdateTime();
        auto now = std::chrono::steady_clock::now();
        auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();
        ImGui::Text("Data age: %.0fs", (float)ageSec);
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Stopped");
      }
    }
  }

  ImGui::Spacing();

  // Connection settings (compact)
  ImGui::PushItemWidth(-1);
  ImGui::InputText("##IP", m_ipAddress, sizeof(m_ipAddress));
  ImGui::InputInt("##Port", &m_port);
  ImGui::PopItemWidth();

  // Connection buttons
  bool canConnect = IsManagerAvailable() && !m_isConnected;
  bool canDisconnect = IsManagerAvailable() && m_isConnected;

  if (!canConnect) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Connect", ImVec2(-1, 30))) {
    if (canConnect) {
      OnConnect();
    }
  }
  if (!canConnect) {
    ImGui::PopStyleVar();
  }

  if (!canDisconnect) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Disconnect", ImVec2(-1, 30))) {
    if (canDisconnect) {
      OnDisconnect();
    }
  }
  if (!canDisconnect) {
    ImGui::PopStyleVar();
  }

  // Show last error if any
  if (!m_lastError.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error:");
    ImGui::TextWrapped("%s", m_lastError.c_str());
  }
}


void CLD101xEquipmentUI::RenderCurrentReadingsSection() {
  ImGui::Text("Current Readings:");

  if (ImGui::BeginTable("ReadingsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableHeadersRow();

    // Temperature reading
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Temperature");
    ImGui::TableNextColumn();
    ImGui::Text("%.2f °C", m_currentTemperature);
    ImGui::TableNextColumn();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "OK");

    // Laser current reading with validation
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Current");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f A", m_currentLaserCurrent);
    ImGui::TableNextColumn();

    // **STATUS VALIDATION INDICATOR**
    bool measurementSuggestsOn = (m_currentLaserCurrent > 0.001f);
    if (m_laserOn && measurementSuggestsOn) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
    }
    else if (!m_laserOn && !measurementSuggestsOn) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Status shows %s but current is %.3fA",
          m_laserOn ? "ON" : "OFF", m_currentLaserCurrent);
      }
    }

    // Hardware status display
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Laser HW");
    ImGui::TableNextColumn();
    if (m_laserOn) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
    }
    ImGui::TableNextColumn();
    ImGui::Text("HW");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("TEC HW");
    ImGui::TableNextColumn();
    if (m_tecOn) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
    }
    ImGui::TableNextColumn();
    ImGui::Text("HW");

    // Setpoints
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Temp Set");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f °C", m_tempSetpoint);
    ImGui::TableNextColumn();
    ImGui::Text("Set");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Curr Set");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f A", m_currentSetpoint);
    ImGui::TableNextColumn();
    ImGui::Text("Set");

    // Data freshness indicator
    if (m_isConnected && IsManagerAvailable()) {
      auto client = m_cld101xManager->GetClient("CLD101x");
      if (client && client->IsPolling()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Data Age");
        ImGui::TableNextColumn();

        auto lastUpdate = client->GetLastUpdateTime();
        auto now = std::chrono::steady_clock::now();
        auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();

        if (ageSec < 2) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.0fs", (float)ageSec);
        }
        else if (ageSec < 10) {
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.0fs", (float)ageSec);
        }
        else {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%.0fs", (float)ageSec);
        }
        ImGui::TableNextColumn();
        ImGui::Text("Poll");
      }
    }

    ImGui::EndTable();
  }

  // Add status validation controls
  RenderStatusValidationControls();
}

// Add to RenderPollingControlsSection():
void CLD101xEquipmentUI::RenderPollingControlsSection() {
  if (!m_isConnected || !IsManagerAvailable()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Connect for polling controls");
    return;
  }

  ImGui::Text("Polling Controls:");

  auto client = m_cld101xManager->GetClient("CLD101x");
  if (client) {
    bool isPolling = client->IsPolling();

    if (isPolling) {
      if (ImGui::Button("Stop Polling", ImVec2(-1, 25))) {
        client->StopPolling();
        AddDebugOutput("Stopped polling");
      }
    }
    else {
      ImGui::Text("Start rate:");
      if (ImGui::Button("500ms", ImVec2(-1, 25))) {
        client->StartPolling(500);
        AddDebugOutput("Started 500ms polling");
      }
      if (ImGui::Button("1000ms", ImVec2(-1, 25))) {
        client->StartPolling(1000);
        AddDebugOutput("Started 1000ms polling");
      }
    }

    // ADD GLOBAL DATA STORE STATUS
    ImGui::Spacing();
    ImGui::Text("Global Data Store:");
    ImGui::SameLine();
    if (client->IsGlobalDataStoreEnabled()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Enabled");
      ImGui::Text("Prefix: %s", client->GetDevicePrefix().c_str());

      // Show what data is being published
      if (isPolling) {
        ImGui::Text("Publishing:");
        ImGui::BulletText("%s-Temperature", client->GetDevicePrefix().c_str());
        ImGui::BulletText("%s-LaserCurrent", client->GetDevicePrefix().c_str());
      }
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Disabled");
    }
  }
}


void CLD101xEquipmentUI::RenderLaserControlSection() {
  // Status indicator
  ImGui::Text("Laser Status:");
  ImGui::SameLine();
  if (m_laserOn) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
  }

  // Interlock status
  ImGui::Text("Interlock:");
  ImGui::SameLine();
  if (m_interlockClosed) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "CLOSED");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OPEN");
  }

  ImGui::Spacing();

  // On/Off buttons
  bool canTurnOnLaser = m_isConnected && IsManagerAvailable() && m_interlockClosed && !m_laserOn;
  bool canTurnOffLaser = m_isConnected && IsManagerAvailable() && m_laserOn;

  if (!canTurnOnLaser) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("LASER ON", ImVec2(-1, 40))) {
    if (canTurnOnLaser) {
      OnLaserOn();
    }
  }
  if (!canTurnOnLaser) {
    ImGui::PopStyleVar();
  }

  if (!canTurnOffLaser) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("LASER OFF", ImVec2(-1, 40))) {
    if (canTurnOffLaser) {
      OnLaserOff();
    }
  }
  if (!canTurnOffLaser) {
    ImGui::PopStyleVar();
  }
}

void CLD101xEquipmentUI::RenderCurrentControlSection() {
  ImGui::Text("Current Control:");

  // Current slider
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderInt("##CurrentMA", &m_currentMA, 0, 280)) {
    UpdateCurrentFromMA();
  }
  ImGui::PopItemWidth();
  ImGui::Text("Current: %d mA (%.3f A)", m_currentMA, m_currentSetpoint);

  // Preset dropdown
  ImGui::PushItemWidth(-1);
  if (ImGui::Combo("##Preset", &m_currentPresetIndex, currentOptions, IM_ARRAYSIZE(currentOptions))) {
    std::string preset = currentOptions[m_currentPresetIndex];
    m_currentMA = std::stoi(preset);
    UpdateCurrentFromMA();
  }
  ImGui::PopItemWidth();

  ImGui::Spacing();

  bool canSetCurrent = m_isConnected && IsManagerAvailable();
  if (!canSetCurrent) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Set Current", ImVec2(-1, 30))) {
    if (canSetCurrent) {
      OnSetCurrent();
    }
  }
  if (!canSetCurrent) {
    ImGui::PopStyleVar();
  }
}

void CLD101xEquipmentUI::RenderTECControlSection() {
  // Status indicator
  ImGui::Text("TEC Status:");
  ImGui::SameLine();
  if (m_tecOn) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
  }

  ImGui::Spacing();

  // On/Off buttons
  bool canControlTEC = m_isConnected && IsManagerAvailable();

  if (!canControlTEC) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("TEC ON", ImVec2(-1, 40))) {
    if (canControlTEC) {
      OnTECOn();
    }
  }
  if (ImGui::Button("TEC OFF", ImVec2(-1, 40))) {
    if (canControlTEC) {
      OnTECOff();
    }
  }

  if (!canControlTEC) {
    ImGui::PopStyleVar();
  }

  ImGui::Spacing();

  // Temperature control
  ImGui::Text("Temperature Control:");
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderInt("##TempC", &m_tempInt, 20, 30)) {
    m_tempSetpoint = static_cast<float>(m_tempInt);
  }
  ImGui::PopItemWidth();
  ImGui::Text("Temperature: %d °C", m_tempInt);

  ImGui::Spacing();

  if (!canControlTEC) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Set Temperature", ImVec2(-1, 30))) {
    if (canControlTEC) {
      OnSetTemperature();
    }
  }
  if (!canControlTEC) {
    ImGui::PopStyleVar();
  }
}


// 1. UPDATE your RenderCompactDebugSection() to include TEC analysis button:
void CLD101xEquipmentUI::RenderCompactDebugSection() {
  ImGui::Text("Quick Commands:");

  bool canSendCommand = m_isConnected && IsManagerAvailable();

  if (!canSendCommand) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("Get Status", ImVec2(-1, 25))) {
    if (canSendCommand) {
      OnGetStatus();
    }
  }
  if (ImGui::Button("Check Errors", ImVec2(-1, 25))) {
    if (canSendCommand) {
      OnCheckErrors();
    }
  }

  // Existing status debugging buttons
  if (ImGui::Button("Debug Laser Status", ImVec2(-1, 25))) {
    if (canSendCommand) {
      OnForceStatusQuery();
    }
  }

  if (ImGui::Button("Sync All Status", ImVec2(-1, 25))) {
    if (canSendCommand) {
      OnSyncHardwareStatus();
    }
  }

  // **NEW: TEC Behavior Analysis Button**
  if (ImGui::Button("Analyze TEC Behavior", ImVec2(-1, 25))) {
    if (canSendCommand) {
      OnAnalyzeTECBehavior();
    }
  }

  if (!canSendCommand) {
    ImGui::PopStyleVar();
  }

  // Auto-refresh controls
  ImGui::Spacing();
  ImGui::Checkbox("Auto-refresh", &m_autoRefresh);
  if (m_autoRefresh) {
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##RefreshRate", &m_refreshRate, 0.5f, 10.0f, "%.1fs");
    ImGui::PopItemWidth();
  }

  // Compact debug output
  if (!m_debugOutput.empty()) {
    ImGui::Spacing();
    ImGui::Text("Debug:");
    ImGui::BeginChild("CompactDebug", ImVec2(0, 100), true);
    ImGui::TextWrapped("%s", m_debugOutput.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    if (ImGui::Button("Clear", ImVec2(-1, 20))) {
      m_debugOutput.clear();
    }
  }
}

// 2. ADD the OnAnalyzeTECBehavior() method implementation:
void CLD101xEquipmentUI::OnAnalyzeTECBehavior() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot analyze TEC: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Analyzing TEC behavior patterns...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Call the TEC analysis method
      client->AnalyzeTECBehavior();

      // Also get some additional debug info
      std::string tecResponse, setpointResponse;

      if (client->SendCommand("OUTPUT2:STATE?", &tecResponse)) {
        AddDebugOutput("Raw TEC hardware status: '" + tecResponse + "'");
      }

      if (client->SendCommand("source2:temperature:spoint?", &setpointResponse)) {
        AddDebugOutput("Raw TEC setpoint query: '" + setpointResponse + "'");
      }

      // Show current readings for comparison
      float currentTemp = client->GetLatestTemperature();
      bool uiTECStatus = m_tecOn;

      AddDebugOutput("UI TEC Status: " + std::string(uiTECStatus ? "ON" : "OFF"));
      AddDebugOutput("Current Temperature: " + std::to_string(currentTemp) + "°C");
      AddDebugOutput("TEC Setpoint (UI): " + std::to_string(m_tempSetpoint) + "°C");

      AddDebugOutput("TEC behavior analysis complete - check logs for details");
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("TEC analysis failed: " + std::string(e.what()));
  }
}

// Helper methods
void CLD101xEquipmentUI::UpdateCurrentFromMA() {
  m_currentSetpoint = m_currentMA / 1000.0f;
}

void CLD101xEquipmentUI::UpdateMAFromCurrent() {
  m_currentMA = static_cast<int>(m_currentSetpoint * 1000.0f);
}

void CLD101xEquipmentUI::AddDebugOutput(const std::string& message) {
  char timeStr[32];
  float currentTime = ImGui::GetTime();
  sprintf(timeStr, "[%.1f] ", currentTime);

  m_debugOutput += timeStr + message + "\n";

  if (m_debugOutput.size() > 3000) {  // Reduced for compact display
    size_t pos = m_debugOutput.find('\n', 500);
    if (pos != std::string::npos) {
      m_debugOutput = m_debugOutput.substr(pos + 1);
    }
  }

  std::cout << "CLD101xUI: " << message << std::endl;
}

void CLD101xEquipmentUI::UpdateStatusFromManager() {
  if (!IsManagerAvailable()) {
    return;
  }

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      m_isConnected = client->IsConnected();

      if (m_isConnected) {
        if (client->IsPolling()) {
          m_currentLaserCurrent = client->GetLatestLaserCurrent();
          m_currentTemperature = client->GetLatestTemperature();
          AddDebugOutput("Status updated from polling cache");
        }
        else {
          m_currentLaserCurrent = client->GetLaserCurrent();
          m_currentTemperature = client->GetTemperature();
          AddDebugOutput("Status updated from direct reading");
        }
      }
    }
    else {
      AddDebugOutput("WARNING: Could not get CLD101x client from manager");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Error updating status: " + std::string(e.what()));
  }
}


void CLD101xEquipmentUI::OnConnect() {
  m_lastError.clear();
  AddDebugOutput("Attempting to connect to " + std::string(m_ipAddress) + ":" + std::to_string(m_port));

  if (!IsManagerAvailable()) {
    m_lastError = "CLD101xManager not available";
    AddDebugOutput("ERROR: " + m_lastError);
    return;
  }

  try {
    // First, add the client with the correct IP and port
    bool clientAdded = m_cld101xManager->AddClient("CLD101x", std::string(m_ipAddress), m_port);
    if (!clientAdded) {
      AddDebugOutput("Client already exists, attempting to connect...");
    }

    // Connect using the real manager
    bool connected = m_cld101xManager->ConnectClient("CLD101x");

    if (connected) {
      m_isConnected = true;
      m_lastConnectionTime = ImGui::GetTime();
      AddDebugOutput("Successfully connected to instrument");

      auto client = m_cld101xManager->GetClient("CLD101x");
      if (client) {
        // **NEW: Sync hardware status and update UI immediately**
        try {
          client->SyncHardwareStatus();

          // Update UI state from hardware
          m_laserOn = client->GetLaserStatus();
          m_tecOn = client->GetTECStatus();

          AddDebugOutput("Hardware status synchronized - Laser: " +
            std::string(m_laserOn ? "ON" : "OFF") +
            ", TEC: " + std::string(m_tecOn ? "ON" : "OFF"));
        }
        catch (const std::exception& e) {
          AddDebugOutput("Warning: Failed to sync hardware status: " + std::string(e.what()));
        }

        // Start polling if not already started
        if (!client->IsPolling()) {
          client->StartPolling(500);
          AddDebugOutput("Started automatic polling at 500ms interval");
        }
      }

      // Get initial measurements
      OnGetStatus();
    }
    else {
      m_lastError = "Connection failed - check IP address and port";
      AddDebugOutput("Connection failed: " + m_lastError);
      m_isConnected = false;
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Connection exception: ") + e.what();
    AddDebugOutput("Connection failed: " + m_lastError);
    m_isConnected = false;
  }
}
void CLD101xEquipmentUI::OnDisconnect() {
  AddDebugOutput("Disconnecting from instrument");

  if (IsManagerAvailable()) {
    try {
      auto client = m_cld101xManager->GetClient("CLD101x");
      if (client) {
        if (client->IsPolling()) {
          client->StopPolling();
          AddDebugOutput("Stopped polling before disconnect");
        }

        client->Disconnect();
        AddDebugOutput("Disconnected from instrument");
      }
    }
    catch (const std::exception& e) {
      AddDebugOutput("Disconnect error: " + std::string(e.what()));
    }
  }

  m_isConnected = false;
  m_laserOn = false;
  m_tecOn = false;
  m_lastConnectionTime = 0;
}

// Continue with all other event handlers (OnLaserOn, OnLaserOff, OnTECOn, OnTECOff, 
// OnSetCurrent, OnSetTemperature, OnSendDebugCommand, OnGetStatus, OnCheckErrors, 
// OnTestConnection, ForceImmediateRefresh) - keeping their existing implementations
// [Keep all existing event handler implementations from the original file]

// ==============================================================================
// CLD101xEquipmentUI.cpp - Missing Event Handler Implementations
// Add these methods to your CLD101xEquipmentUI.cpp file
// ==============================================================================

void CLD101xEquipmentUI::OnLaserOn() {
  if (!m_isConnected) {
    AddDebugOutput("Cannot turn on laser: Not connected");
    return;
  }

  if (!IsManagerAvailable()) {
    AddDebugOutput("Cannot turn on laser: Manager not available");
    return;
  }

  AddDebugOutput("Attempting to turn laser ON...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->LaserOn();
      if (success) {
        m_laserOn = true;
        AddDebugOutput("Laser turned ON successfully");
        OnGetStatus(); // Refresh status
      }
      else {
        m_lastError = "Failed to turn laser ON - check interlock and current settings";
        AddDebugOutput("Failed to turn laser ON: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Laser ON exception: ") + e.what();
    AddDebugOutput("Laser ON failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnLaserOff() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot turn off laser: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Attempting to turn laser OFF...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->LaserOff();
      if (success) {
        m_laserOn = false;
        AddDebugOutput("Laser turned OFF successfully");
        OnGetStatus(); // Refresh status
      }
      else {
        m_lastError = "Failed to turn laser OFF";
        AddDebugOutput("Failed to turn laser OFF: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Laser OFF exception: ") + e.what();
    AddDebugOutput("Laser OFF failed: " + m_lastError);
  }
}

// ============================================================================
// ENHANCED TEC command handlers - Add delay to prevent status flipping
// Update your OnTECOn and OnTECOff methods:
// ============================================================================

void CLD101xEquipmentUI::OnTECOn() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot turn on TEC: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Attempting to turn TEC ON...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->TECOn();
      if (success) {
        // **IMMEDIATE UI UPDATE: Set status right away**
        m_tecOn = true;
        AddDebugOutput("TEC turned ON successfully");

        // **DELAY BEFORE STATUS REFRESH: Give hardware time to settle**
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        OnGetStatus(); // Refresh status after delay
      }
      else {
        m_lastError = "Failed to turn TEC ON";
        AddDebugOutput("Failed to turn TEC ON: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("TEC ON exception: ") + e.what();
    AddDebugOutput("TEC ON failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnTECOff() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot turn off TEC: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Attempting to turn TEC OFF...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->TECOff();
      if (success) {
        // **IMMEDIATE UI UPDATE: Set status right away**
        m_tecOn = false;
        AddDebugOutput("TEC turned OFF successfully");

        // **DELAY BEFORE STATUS REFRESH: Give hardware time to settle**
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        OnGetStatus(); // Refresh status after delay
      }
      else {
        m_lastError = "Failed to turn TEC OFF";
        AddDebugOutput("Failed to turn TEC OFF: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("TEC OFF exception: ") + e.what();
    AddDebugOutput("TEC OFF failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnSetCurrent() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot set current: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Setting laser current to " + std::to_string(m_currentSetpoint) + " A");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->SetLaserCurrent(m_currentSetpoint);
      if (success) {
        AddDebugOutput("Current set successfully to " + std::to_string(m_currentSetpoint) + " A");

        // Wait for instrument to settle
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Force immediate refresh
        ForceImmediateRefresh();
      }
      else {
        m_lastError = "Failed to set current";
        AddDebugOutput("Failed to set current: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Set current exception: ") + e.what();
    AddDebugOutput("Set current failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnSetTemperature() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot set temperature: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Setting TEC temperature to " + std::to_string(m_tempSetpoint) + " °C");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->SetTECTemperature(m_tempSetpoint);
      if (success) {
        AddDebugOutput("Temperature set successfully to " + std::to_string(m_tempSetpoint) + " °C");
        OnGetStatus(); // Refresh status
      }
      else {
        m_lastError = "Failed to set temperature";
        AddDebugOutput("Failed to set temperature: " + m_lastError);
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Set temperature exception: ") + e.what();
    AddDebugOutput("Set temperature failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnGetStatus() {
  if (!IsManagerAvailable()) {
    AddDebugOutput("Cannot get status: Manager not available");
    return;
  }

  if (!m_isConnected) {
    AddDebugOutput("Cannot get status: Not connected");
    return;
  }

  AddDebugOutput("Getting instrument status...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Update connection status
      m_isConnected = client->IsConnected();

      if (m_isConnected) {
        try {
          if (client->IsPolling()) {
            // Use cached values from polling thread
            m_currentLaserCurrent = client->GetLatestLaserCurrent();
            m_currentTemperature = client->GetLatestTemperature();

            auto lastUpdate = client->GetLastUpdateTime();
            auto now = std::chrono::steady_clock::now();
            auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();

            AddDebugOutput("Status from polling: Current=" + std::to_string(m_currentLaserCurrent) +
              "A, Temp=" + std::to_string(m_currentTemperature) + "°C (age: " +
              std::to_string(ageSec) + "s)");

            // Force UI update by calling UpdateStatusFromManager
            UpdateStatusFromManager();
          }
          else {
            // Fall back to direct reading if not polling
            m_currentLaserCurrent = client->GetLaserCurrent();
            m_currentTemperature = client->GetTemperature();
            AddDebugOutput("Status updated: Current=" + std::to_string(m_currentLaserCurrent) +
              "A, Temp=" + std::to_string(m_currentTemperature) + "°C");
          }
        }
        catch (const std::exception& e) {
          AddDebugOutput("Error reading status values: " + std::string(e.what()));
        }
      }
      else {
        AddDebugOutput("Client reports not connected");
        m_isConnected = false;
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client from manager");
    }
  }
  catch (const std::exception& e) {
    m_lastError = std::string("Status query exception: ") + e.what();
    AddDebugOutput("Status query failed: " + m_lastError);
  }
}

void CLD101xEquipmentUI::OnCheckErrors() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot check errors: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Checking instrument error queue...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Try to get error information from the client
      std::string response;
      bool success = client->SendCommand("SYST:ERR?", &response);
      if (success) {
        if (response.find("0,") != std::string::npos) {
          AddDebugOutput("No errors in queue");
        }
        else {
          AddDebugOutput("Error found: " + response);
        }
      }
      else {
        AddDebugOutput("Could not query error queue");
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Error check exception: " + std::string(e.what()));
  }
}

void CLD101xEquipmentUI::OnTestConnection() {
  AddDebugOutput("Testing connection to instrument...");

  if (!IsManagerAvailable()) {
    AddDebugOutput("Manager not available for connection test");
    return;
  }

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client && client->IsConnected()) {
      AddDebugOutput("Connection test passed");

      // Try a simple query to verify communication
      std::string response;
      bool success = client->SendCommand("READ_TEC_TEMPERATURE", &response);
      if (success) {
        AddDebugOutput("Communication test successful: " + response);

        // Also test if polling is working
        if (client->IsPolling()) {
          auto lastUpdate = client->GetLastUpdateTime();
          auto now = std::chrono::steady_clock::now();
          auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();

          if (ageSec < 5) {
            AddDebugOutput("Polling test successful - data is fresh");
          }
          else {
            AddDebugOutput("Polling test warning - data is " + std::to_string(ageSec) + "s old");
          }
        }
        else {
          AddDebugOutput("Note: Polling is not active");
        }
      }
      else {
        AddDebugOutput("Communication test failed");
      }
    }
    else {
      AddDebugOutput("Connection test failed - client not connected");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Connection test error: " + std::string(e.what()));
  }
}

void CLD101xEquipmentUI::ForceImmediateRefresh() {
  if (!m_isConnected || !IsManagerAvailable()) {
    return;
  }

  AddDebugOutput("Forcing immediate refresh of readings...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Get fresh temperature reading
      std::string tempResponse;
      if (client->SendCommand("READ_TEC_TEMPERATURE", &tempResponse)) {
        size_t pos = tempResponse.find(": ");
        if (pos != std::string::npos) {
          try {
            float temp = std::stof(tempResponse.substr(pos + 2));
            m_currentTemperature = temp;
          }
          catch (const std::exception& e) {
            AddDebugOutput("Error parsing temperature: " + std::string(e.what()));
          }
        }
      }

      // Small delay between commands
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      // Get fresh current reading
      std::string currentResponse;
      if (client->SendCommand("READ_LASER_CURRENT", &currentResponse)) {
        size_t pos = currentResponse.find(": ");
        if (pos != std::string::npos) {
          try {
            float current = std::stof(currentResponse.substr(pos + 2));
            m_currentLaserCurrent = current;
            AddDebugOutput("Immediate refresh: Current=" + std::to_string(current) +
              "A, Temp=" + std::to_string(m_currentTemperature) + "°C");
          }
          catch (const std::exception& e) {
            AddDebugOutput("Error parsing current: " + std::string(e.what()));
          }
        }
      }
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Immediate refresh failed: " + std::string(e.what()));
  }
}

void CLD101xEquipmentUI::OnSendDebugCommand(const std::string& command) {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot send command: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Sending SCPI command: " + command);

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      std::string response;
      bool success = client->SendCommand(command, &response);
      if (success) {
        AddDebugOutput("Command response: " + response);
      }
      else {
        AddDebugOutput("Command failed - no response");
      }
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Command exception: " + std::string(e.what()));
  }
}


// **NEW: Add method to sync UI with hardware status**
void CLD101xEquipmentUI::SyncUIWithHardware() {
  if (!m_isConnected || !IsManagerAvailable()) {
    return;
  }

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Force fresh status query
      client->SyncHardwareStatus();

      // Update UI state
      bool newLaserStatus = client->GetLaserStatus();
      bool newTECStatus = client->GetTECStatus();

      // Log any status changes
      if (newLaserStatus != m_laserOn) {
        AddDebugOutput("Laser status changed: " + std::string(m_laserOn ? "ON" : "OFF") +
          " -> " + std::string(newLaserStatus ? "ON" : "OFF"));
        m_laserOn = newLaserStatus;
      }

      if (newTECStatus != m_tecOn) {
        AddDebugOutput("TEC status changed: " + std::string(m_tecOn ? "ON" : "OFF") +
          " -> " + std::string(newTECStatus ? "ON" : "OFF"));
        m_tecOn = newTECStatus;
      }

      // Also update readings
      if (client->IsPolling()) {
        m_currentTemperature = client->GetLatestTemperature();
        m_currentLaserCurrent = client->GetLatestLaserCurrent();
      }

      AddDebugOutput("UI synchronized with hardware status");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Failed to sync UI with hardware: " + std::string(e.what()));
  }
}



// 1. NEW: Enhanced status synchronization event handlers
void CLD101xEquipmentUI::OnSyncHardwareStatus() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot sync status: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Manually syncing hardware status...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Force hardware status sync
      client->SyncHardwareStatus();

      // Update UI immediately
      m_laserOn = client->GetLaserStatus();
      m_tecOn = client->GetTECStatus();

      AddDebugOutput("Hardware status sync complete - Laser: " +
        std::string(m_laserOn ? "ON" : "OFF") +
        ", TEC: " + std::string(m_tecOn ? "ON" : "OFF"));
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Hardware status sync failed: " + std::string(e.what()));
  }
}

void CLD101xEquipmentUI::OnForceStatusQuery() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot query status: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Forcing fresh status queries...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      // Clear cache and force fresh queries
      std::string laserResponse, tecResponse;

      if (client->SendCommand("OUTPUT1:STATE?", &laserResponse)) {
        AddDebugOutput("Raw laser status response: '" + laserResponse + "'");
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (client->SendCommand("OUTPUT2:STATE?", &tecResponse)) {
        AddDebugOutput("Raw TEC status response: '" + tecResponse + "'");
      }

      // Now sync status
      client->SyncHardwareStatus();
      m_laserOn = client->GetLaserStatus();
      m_tecOn = client->GetTECStatus();

      AddDebugOutput("Status query complete");
    }
    else {
      AddDebugOutput("ERROR: Could not get CLD101x client");
    }
  }
  catch (const std::exception& e) {
    AddDebugOutput("Force status query failed: " + std::string(e.what()));
  }
}

// 2. NEW: Enhanced status validation controls method
void CLD101xEquipmentUI::RenderStatusValidationControls() {
  if (!m_isConnected || !IsManagerAvailable()) {
    return;
  }

  ImGui::Spacing();
  ImGui::Text("Status Validation:");

  // Check for measurement/status mismatch
  bool measurementSuggestsLaserOn = (m_currentLaserCurrent > 0.001f);
  bool statusMismatch = (m_laserOn != measurementSuggestsLaserOn);

  if (statusMismatch) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Mismatch Detected!");
    ImGui::Text("Status: %s, Current: %.3fA",
      m_laserOn ? "ON" : "OFF", m_currentLaserCurrent);
  }
  else {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Status Consistent");
  }

  // Sync buttons
  if (ImGui::Button("Sync Hardware Status", ImVec2(-1, 25))) {
    OnSyncHardwareStatus();
  }

  if (ImGui::Button("Force Status Query", ImVec2(-1, 25))) {
    OnForceStatusQuery();
  }
}
