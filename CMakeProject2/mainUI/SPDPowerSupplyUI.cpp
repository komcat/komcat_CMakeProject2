
#include "SPDPowerSupplyUI.h"

SPDPowerSupplyUI::SPDPowerSupplyUI(SPDPowerSupplyManager* spdManager)
  : m_spdManager(spdManager) {
}

void SPDPowerSupplyUI::RenderUI() {
  if (!m_spdManager) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SPD Power Supply Manager not available");
    return;
  }

  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("SPD Power Supply Control");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();

  RenderStatusSection();
  ImGui::Separator();

  RenderConnectionControls();
  ImGui::Separator();

  RenderOutputControl();
  ImGui::Separator();

  RenderModeControls();
  ImGui::Separator();

  RenderPollingControls();
  ImGui::Separator();

  RenderLiveDataDisplay();
}

void SPDPowerSupplyUI::RenderStatusSection() {
  ImGui::Text("Status:");
  ImGui::SameLine();

  int connectedCount = m_spdManager->GetConnectedCount();
  int totalCount = m_spdManager->GetDeviceNames().size();

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
