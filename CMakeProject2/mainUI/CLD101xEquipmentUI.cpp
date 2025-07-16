
// ==============================================================================
// IMPLEMENTATION FILE: CLD101xEquipmentUI.cpp
// ==============================================================================
#include "CLD101xEquipmentUI.h"
#include <algorithm>
#include <string>

// Preset current options
static const char* currentOptions[] = {
    "110 mA", "120 mA", "130 mA", "140 mA", "150 mA",
    "160 mA", "170 mA", "180 mA", "190 mA", "200 mA",
    "210 mA", "220 mA", "230 mA", "240 mA", "250 mA"
};

CLD101xEquipmentUI::CLD101xEquipmentUI() {
  UpdateMAFromCurrent();
}

void CLD101xEquipmentUI::Render() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("CLD101x Equipment Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Laser and TEC control for CLD101x equipment");
  ImGui::Separator();
  ImGui::Spacing();

  // Two-column layout
  ImGui::Columns(2, "CLD101xMainColumns", true);

  // Adjust column width (optional)
  static bool first_time = true;
  if (first_time) {
    ImGui::SetColumnWidth(0, 420.0f);  // Left column
    first_time = false;
  }

  // === LEFT COLUMN ===
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

  RenderConnectionSection();

  // End two-column layout
  ImGui::Columns(1);
}



void CLD101xEquipmentUI::RenderConnectionSection() {
  ImGui::Text("Connection Settings:");

  // Connection status
  ImGui::Text("Status:");
  ImGui::SameLine();
  if (m_isConnected) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Connected");
  }

  // Connection settings
  ImGui::InputText("IP Address", m_ipAddress, sizeof(m_ipAddress));
  ImGui::InputInt("Port", &m_port);

  // Connection buttons
  if (ImGui::Button("Connect", ImVec2(100, 30))) {
    OnConnect();
  }
  ImGui::SameLine();
  if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
    OnDisconnect();
  }
}

void CLD101xEquipmentUI::RenderLaserControlSection() {
  ImGui::Text("Laser Control:");

  // Status indicator
  ImGui::Text("Laser Status:");
  ImGui::SameLine();
  if (m_laserOn) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "OFF");
  }

  // On/Off buttons
  if (ImGui::Button("LASER ON", ImVec2(100, 40))) {
    OnLaserOn();
  }
  ImGui::SameLine();
  if (ImGui::Button("LASER OFF", ImVec2(100, 40))) {
    OnLaserOff();
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
    // Extract value from string (e.g., "150 mA" -> 150)
    std::string preset = currentOptions[m_currentPresetIndex];
    m_currentMA = std::stoi(preset);
    UpdateCurrentFromMA();
  }

  if (ImGui::Button("Set Current", ImVec2(120, 30))) {
    OnSetCurrent();
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
  if (ImGui::Button("TEC ON", ImVec2(100, 40))) {
    OnTECOn();
  }
  ImGui::SameLine();
  if (ImGui::Button("TEC OFF", ImVec2(100, 40))) {
    OnTECOff();
  }

  // Temperature control
  ImGui::Text("Temperature Control:");
  if (ImGui::SliderInt("Temperature (°C)", &m_tempInt, 20, 30)) {
    m_tempSetpoint = static_cast<float>(m_tempInt);
  }

  if (ImGui::Button("Set Temperature", ImVec2(150, 30))) {
    OnSetTemperature();
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

    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Implementation Notes:");
  ImGui::BulletText("Ready for CLD101xManager integration");
  ImGui::BulletText("Add error handling and validation");
  ImGui::BulletText("Connect to actual hardware for live data");
  ImGui::BulletText("Add data logging and history graphs");
}

// Helper methods
void CLD101xEquipmentUI::UpdateCurrentFromMA() {
  m_currentSetpoint = m_currentMA / 1000.0f;
}

void CLD101xEquipmentUI::UpdateMAFromCurrent() {
  m_currentMA = static_cast<int>(m_currentSetpoint * 1000.0f);
}

// Event handlers (placeholder implementations)
void CLD101xEquipmentUI::OnConnect() {
  // TODO: Implement actual connection logic
  // if (m_cld101xManager) {
  //     m_isConnected = m_cld101xManager->Connect(m_ipAddress, m_port);
  // }
  m_isConnected = true; // Simulate successful connection
}

void CLD101xEquipmentUI::OnDisconnect() {
  // TODO: Implement actual disconnect logic
  // if (m_cld101xManager) {
  //     m_cld101xManager->Disconnect();
  // }
  m_isConnected = false;
  m_laserOn = false;
  m_tecOn = false;
}

void CLD101xEquipmentUI::OnLaserOn() {
  if (!m_isConnected) return;
  // TODO: Implement actual laser on logic
  // if (m_cld101xManager) {
  //     m_laserOn = m_cld101xManager->LaserOn();
  // }
  m_laserOn = true;
}

void CLD101xEquipmentUI::OnLaserOff() {
  if (!m_isConnected) return;
  // TODO: Implement actual laser off logic
  // if (m_cld101xManager) {
  //     m_laserOn = !m_cld101xManager->LaserOff();
  // }
  m_laserOn = false;
}

void CLD101xEquipmentUI::OnTECOn() {
  if (!m_isConnected) return;
  // TODO: Implement actual TEC on logic
  // if (m_cld101xManager) {
  //     m_tecOn = m_cld101xManager->TECOn();
  // }
  m_tecOn = true;
}

void CLD101xEquipmentUI::OnTECOff() {
  if (!m_isConnected) return;
  // TODO: Implement actual TEC off logic
  // if (m_cld101xManager) {
  //     m_tecOn = !m_cld101xManager->TECOff();
  // }
  m_tecOn = false;
}

void CLD101xEquipmentUI::OnSetCurrent() {
  if (!m_isConnected) return;
  // TODO: Implement actual set current logic
  // if (m_cld101xManager) {
  //     m_cld101xManager->SetLaserCurrent(m_currentSetpoint);
  // }
  m_currentLaserCurrent = m_currentSetpoint; // Simulate setting
}

void CLD101xEquipmentUI::OnSetTemperature() {
  if (!m_isConnected) return;
  // TODO: Implement actual set temperature logic
  // if (m_cld101xManager) {
  //     m_cld101xManager->SetTECTemperature(m_tempSetpoint);
  // }
  m_currentTemperature = m_tempSetpoint; // Simulate setting
}