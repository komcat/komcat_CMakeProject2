// ==============================================================================
// IMPLEMENTATION FILE: CLD101xEquipmentUI.cpp - Enhanced with Polling Support
// ==============================================================================
#include "CLD101xEquipmentUI.h"
#include "include/cld101x_manager.h"  // Add this include
#include <algorithm>
#include <string>
#include <iostream>

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

    // Try to get initial status if manager is available
    OnGetStatus();
  }
  else {
    AddDebugOutput("WARNING: CLD101xManager is null!");
    std::cout << "CLD101xEquipmentUI: WARNING - Manager is null!" << std::endl;
  }
}



// NEW: Add this method to automatically update UI from polling cache
void CLD101xEquipmentUI::UpdateUIFromPollingCache() {
  if (!m_isConnected || !IsManagerAvailable()) {
    return;
  }

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client && client->IsPolling()) {
      // Get fresh values from polling cache every frame
      float pollingTemp = client->GetLatestTemperature();
      float pollingCurrent = client->GetLatestLaserCurrent();

      // Only update if values have changed (to avoid unnecessary updates)
      if (std::abs(pollingTemp - m_currentTemperature) > 0.001f) {
        m_currentTemperature = pollingTemp;
      }

      if (std::abs(pollingCurrent - m_currentLaserCurrent) > 0.000001f) {
        m_currentLaserCurrent = pollingCurrent;
      }
    }
  }
  catch (const std::exception& e) {
    // Silently ignore errors in UI update to avoid spam
  }
}

void CLD101xEquipmentUI::Render() {
  if (!m_showWindow) {
    return;
  }

  // NEW: Automatic UI updates from polling data
  UpdateUIFromPollingCache();
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("CLD101x Equipment Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Laser and TEC control for CLD101x equipment");
  ImGui::Separator();
  ImGui::Spacing();

  // Two-column layout
  ImGui::Columns(2, "CLD101xMainColumns", true);

  // Adjust column width
  static bool first_time = true;
  if (first_time) {
    ImGui::SetColumnWidth(0, 420.0f);  // Left column
    first_time = false;
  }

  // === LEFT COLUMN ===
  RenderConnectionSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  RenderStatusSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  RenderLaserControlSection();

  // Switch to right column
  ImGui::NextColumn();

  // === RIGHT COLUMN ===
  RenderTECControlSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  RenderDebugSection();

  // End two-column layout
  ImGui::Columns(1);

  // Enhanced auto-refresh with polling awareness
  if (m_autoRefresh && m_isConnected) {
    float currentTime = ImGui::GetTime();
    if (currentTime - m_lastStatusUpdate > m_refreshRate) {
      OnGetStatus();
      m_lastStatusUpdate = currentTime;
    }
  }
}

void CLD101xEquipmentUI::RenderConnectionSection() {
  ImGui::Text("Connection Status:");

  // Show manager availability status
  ImGui::Text("Manager:");
  ImGui::SameLine();
  if (IsManagerAvailable()) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Available");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NOT AVAILABLE");
    ImGui::TextWrapped("CLD101xManager not initialized. Check MainUIManager::SetCLD101xManager()");
  }

  // Connection status with enhanced info
  ImGui::Text("Connection:");
  ImGui::SameLine();
  if (m_isConnected) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    if (m_lastConnectionTime > 0) {
      ImGui::SameLine();
      ImGui::Text("(%.1fs ago)", (ImGui::GetTime() - m_lastConnectionTime));
    }
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Connected");
  }

  // NEW: Show polling status
  if (m_isConnected && IsManagerAvailable()) {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      ImGui::Text("Polling:");
      ImGui::SameLine();
      if (client->IsPolling()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active (%dms)", client->GetPollingInterval());

        // Show data age
        auto lastUpdate = client->GetLastUpdateTime();
        auto now = std::chrono::steady_clock::now();
        auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();
        ImGui::SameLine();
        ImGui::Text("(%.0fs ago)", (float)ageSec);
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Stopped");
      }
    }
  }

  ImGui::Spacing();

  // Connection settings
  ImGui::InputText("IP Address", m_ipAddress, sizeof(m_ipAddress));
  ImGui::InputInt("Port", &m_port);

  // Connection buttons - disable if manager not available
  bool canConnect = IsManagerAvailable() && !m_isConnected;
  bool canDisconnect = IsManagerAvailable() && m_isConnected;

  if (!canConnect) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Connect", ImVec2(100, 30))) {
    if (canConnect) {
      OnConnect();
    }
  }
  if (!canConnect) {
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Cannot connect: %s",
        !IsManagerAvailable() ? "CLD101xManager not available" : "Already connected");
    }
  }

  ImGui::SameLine();

  if (!canDisconnect) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
    if (canDisconnect) {
      OnDisconnect();
    }
  }
  if (!canDisconnect) {
    ImGui::PopStyleVar();
  }

  // Test connection button
  if (m_isConnected && IsManagerAvailable()) {
    if (ImGui::Button("Test Connection", ImVec2(120, 30))) {
      OnTestConnection();
    }
  }

  // Show last error if any
  if (!m_lastError.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Last Error:");
    ImGui::TextWrapped("%s", m_lastError.c_str());
  }
}

void CLD101xEquipmentUI::RenderLaserControlSection() {
  ImGui::Text("Laser Control:");

  // Enhanced status indicator
  ImGui::Text("Laser Status:");
  ImGui::SameLine();
  if (m_laserOn) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
  }

  // Show interlock status
  ImGui::Text("Interlock:");
  ImGui::SameLine();
  if (m_interlockClosed) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "CLOSED");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OPEN");
  }

  // On/Off buttons with safety checks
  bool canTurnOnLaser = m_isConnected && IsManagerAvailable() && m_interlockClosed && !m_laserOn;
  bool canTurnOffLaser = m_isConnected && IsManagerAvailable() && m_laserOn;

  if (!canTurnOnLaser) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("LASER ON", ImVec2(100, 40))) {
    if (canTurnOnLaser) {
      OnLaserOn();
    }
  }
  if (!canTurnOnLaser) {
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) {
      std::string tooltip = "Cannot turn on laser:\n";
      if (!m_isConnected) tooltip += "- Not connected\n";
      if (!IsManagerAvailable()) tooltip += "- Manager not available\n";
      if (!m_interlockClosed) tooltip += "- Interlock open\n";
      if (m_laserOn) tooltip += "- Laser already on\n";
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }

  ImGui::SameLine();

  if (!canTurnOffLaser) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("LASER OFF", ImVec2(100, 40))) {
    if (canTurnOffLaser) {
      OnLaserOff();
    }
  }
  if (!canTurnOffLaser) {
    ImGui::PopStyleVar();
  }

  // Current control
  ImGui::Text("Current Control:");
  if (ImGui::SliderInt("Current (mA)", &m_currentMA, 0, 280)) {
    UpdateCurrentFromMA();
  }
  ImGui::SameLine();
  ImGui::Text("(%.3f A)", m_currentSetpoint);

  // Preset dropdown
  if (ImGui::Combo("Preset Current", &m_currentPresetIndex,
    currentOptions, IM_ARRAYSIZE(currentOptions))) {
    std::string preset = currentOptions[m_currentPresetIndex];
    m_currentMA = std::stoi(preset);
    UpdateCurrentFromMA();
  }

  bool canSetCurrent = m_isConnected && IsManagerAvailable();
  if (!canSetCurrent) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Set Current", ImVec2(120, 30))) {
    if (canSetCurrent) {
      OnSetCurrent();
    }
  }
  if (!canSetCurrent) {
    ImGui::PopStyleVar();
  }
}

void CLD101xEquipmentUI::RenderTECControlSection() {
  ImGui::Text("TEC Control:");

  // Status indicator
  ImGui::Text("TEC Status:");
  ImGui::SameLine();
  if (m_tecOn) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
  }

  // On/Off buttons
  bool canControlTEC = m_isConnected && IsManagerAvailable();

  if (!canControlTEC) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("TEC ON", ImVec2(100, 40))) {
    if (canControlTEC) {
      OnTECOn();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("TEC OFF", ImVec2(100, 40))) {
    if (canControlTEC) {
      OnTECOff();
    }
  }

  if (!canControlTEC) {
    ImGui::PopStyleVar();
  }

  // Temperature control
  ImGui::Text("Temperature Control:");
  if (ImGui::SliderInt("Temperature (°C)", &m_tempInt, 20, 30)) {
    m_tempSetpoint = static_cast<float>(m_tempInt);
  }

  if (!canControlTEC) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Set Temperature", ImVec2(150, 30))) {
    if (canControlTEC) {
      OnSetTemperature();
    }
  }
  if (!canControlTEC) {
    ImGui::PopStyleVar();
  }
}

void CLD101xEquipmentUI::RenderStatusSection() {
  ImGui::Text("Current Readings:");

  // Create status table
  if (ImGui::BeginTable("StatusTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Temperature reading
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Current Temperature");
    ImGui::TableNextColumn();
    ImGui::Text("%.2f °C", m_currentTemperature);

    // Laser current reading
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Laser Current");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f A (%.0f mA)", m_currentLaserCurrent, m_currentLaserCurrent * 1000.0f);

    // Temperature setpoint
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Temperature Setpoint");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f °C", m_tempSetpoint);

    // Current setpoint
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Current Setpoint");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f A (%d mA)", m_currentSetpoint, m_currentMA);

    // Manager status
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Manager Status");
    ImGui::TableNextColumn();
    if (IsManagerAvailable()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Available");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Available");
    }

    // NEW: Data freshness indicator when polling
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
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.0fs (Fresh)", (float)ageSec);
        }
        else if (ageSec < 10) {
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.0fs (Stale)", (float)ageSec);
        }
        else {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%.0fs (Old)", (float)ageSec);
        }
      }
    }

    ImGui::EndTable();
  }
}

void CLD101xEquipmentUI::RenderDebugSection() {
  ImGui::Text("Debug & Diagnostics:");
  ImGui::Separator();

  // NEW: Enhanced polling controls
  if (m_isConnected && IsManagerAvailable()) {
    ImGui::Text("Continuous Reading Controls:");

    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool isPolling = client->IsPolling();

      ImGui::Text("Polling Status:");
      ImGui::SameLine();
      if (isPolling) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active (%dms)", client->GetPollingInterval());

        if (ImGui::Button("Stop Polling", ImVec2(100, 25))) {
          client->StopPolling();
          AddDebugOutput("Stopped continuous polling");
        }
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Stopped");

        ImGui::Text("Start polling:");
        if (ImGui::Button("250ms", ImVec2(60, 25))) {
          client->StartPolling(250);
          AddDebugOutput("Started polling at 250ms interval");
        }
        ImGui::SameLine();
        if (ImGui::Button("500ms", ImVec2(60, 25))) {
          client->StartPolling(500);
          AddDebugOutput("Started polling at 500ms interval");
        }
        ImGui::SameLine();
        if (ImGui::Button("1000ms", ImVec2(60, 25))) {
          client->StartPolling(1000);
          AddDebugOutput("Started polling at 1000ms interval");
        }
        ImGui::SameLine();
        if (ImGui::Button("2000ms", ImVec2(60, 25))) {
          client->StartPolling(2000);
          AddDebugOutput("Started polling at 2000ms interval");
        }
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // Manual command interface
  ImGui::Text("Manual SCPI Commands:");

  static char command[256] = "";
  ImGui::InputText("Command", command, sizeof(command));

  bool canSendCommand = m_isConnected && IsManagerAvailable();
  if (!canSendCommand) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Send Command", ImVec2(120, 25))) {
    if (canSendCommand && strlen(command) > 0) {
      OnSendDebugCommand(command);
    }
  }
  if (!canSendCommand) {
    ImGui::PopStyleVar();
  }

  ImGui::Spacing();

  // Quick test buttons
  ImGui::Text("Quick Tests:");

  if (!canSendCommand) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  if (ImGui::Button("Get Status", ImVec2(100, 25))) {
    if (canSendCommand) {
      OnGetStatus();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Check Errors", ImVec2(100, 25))) {
    if (canSendCommand) {
      OnCheckErrors();
    }
  }
  if (!canSendCommand) {
    ImGui::PopStyleVar();
  }

  // Enhanced auto-refresh controls
  ImGui::Spacing();
  ImGui::Checkbox("Auto-refresh status", &m_autoRefresh);
  if (m_autoRefresh) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat("Rate(s)", &m_refreshRate, 0.5f, 10.0f, "%.1f")) {
      m_refreshRate = (std::max)(0.1f, (std::min)(m_refreshRate, 10.0f)); // Clamp between 0.1s and 10s
    }
    ImGui::SameLine();
    ImGui::Text("(%.1fs)", m_refreshRate);
  }

  // Debug output area
  if (!m_debugOutput.empty()) {
    ImGui::Spacing();
    ImGui::Text("Debug Output:");
    ImGui::BeginChild("DebugOutput", ImVec2(0, 150), true);
    ImGui::TextWrapped("%s", m_debugOutput.c_str());

    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    if (ImGui::Button("Clear Debug", ImVec2(100, 25))) {
      m_debugOutput.clear();
    }
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
  // Add timestamp
  char timeStr[32];
  float currentTime = ImGui::GetTime();
  sprintf(timeStr, "[%.1f] ", currentTime);

  m_debugOutput += timeStr + message + "\n";

  // Limit debug output size to prevent memory issues
  if (m_debugOutput.size() > 5000) {
    size_t pos = m_debugOutput.find('\n', 1000);
    if (pos != std::string::npos) {
      m_debugOutput = m_debugOutput.substr(pos + 1);
    }
  }

  // Also output to console for debugging
  std::cout << "CLD101xUI: " << message << std::endl;
}

void CLD101xEquipmentUI::UpdateStatusFromManager() {
  if (!IsManagerAvailable()) {
    return;
  }

  try {
    // Get the CLD101x client from the manager
    auto client = m_cld101xManager->GetClient("CLD101x"); // Adjust client name as needed
    if (client) {
      // Update status from real hardware
      m_isConnected = client->IsConnected();

      if (m_isConnected) {
        // Use cached readings from polling if available
        if (client->IsPolling()) {
          m_currentLaserCurrent = client->GetLatestLaserCurrent();
          m_currentTemperature = client->GetLatestTemperature();
          AddDebugOutput("Status updated from polling cache");
        }
        else {
          // Fall back to direct reading if not polling
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

// Real event handlers connected to CLD101xManager
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
      // Client might already exist, try to connect anyway
      AddDebugOutput("Client already exists, attempting to connect...");
    }

    // Connect using the real manager - use ConnectClient method
    bool connected = m_cld101xManager->ConnectClient("CLD101x");

    if (connected) {
      m_isConnected = true;
      m_lastConnectionTime = ImGui::GetTime();
      AddDebugOutput("Successfully connected to instrument");

      // NEW: Start polling automatically if not already started
      auto client = m_cld101xManager->GetClient("CLD101x");
      if (client && !client->IsPolling()) {
        client->StartPolling(500); // Start with 500ms polling
        AddDebugOutput("Started automatic polling at 500ms interval");
      }

      // Get initial status
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
        // NEW: Stop polling before disconnecting
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
      bool success = client->LaserOn(); // Use LaserOn() not SetLaserOn()
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
      bool success = client->LaserOff(); // Use LaserOff() not SetLaserOff()
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

void CLD101xEquipmentUI::OnTECOn() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot turn on TEC: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Attempting to turn TEC ON...");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->TECOn(); // Use TECOn() not SetTECOn()
      if (success) {
        m_tecOn = true;
        AddDebugOutput("TEC turned ON successfully");
        OnGetStatus(); // Refresh status
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
      bool success = client->TECOff(); // Use TECOff() not SetTECOff()
      if (success) {
        m_tecOn = false;
        AddDebugOutput("TEC turned OFF successfully");
        OnGetStatus(); // Refresh status
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


// Add this implementation to CLD101xEquipmentUI.cpp:
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

// Then update OnSetCurrent to use this:
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
// Add these method implementations to your existing CLD101xEquipmentUI.cpp file

void CLD101xEquipmentUI::OnSetTemperature() {
  if (!m_isConnected || !IsManagerAvailable()) {
    AddDebugOutput("Cannot set temperature: Not connected or manager unavailable");
    return;
  }

  AddDebugOutput("Setting TEC temperature to " + std::to_string(m_tempSetpoint) + " °C");

  try {
    auto client = m_cld101xManager->GetClient("CLD101x");
    if (client) {
      bool success = client->SetTECTemperature(m_tempSetpoint); // This method exists
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
      bool success = client->SendCommand(command, &response); // Use pointer for response
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
        // FIXED: Always update the UI values regardless of polling status
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

            // IMPORTANT: Force UI update by calling UpdateStatusFromManager
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