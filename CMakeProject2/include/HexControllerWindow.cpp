// HexControllerWindow.cpp
#include "HexControllerWindow.h"

HexControllerWindow::HexControllerWindow(PIControllerManager& controllerManager)
  : m_controllerManager(controllerManager) {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("HexControllerWindow initialized");
}

std::string HexControllerWindow::GetSelectedDeviceName() const {
  if (m_selectedDeviceIndex >= 0 && m_selectedDeviceIndex < m_availableDevices.size()) {
    return m_availableDevices[m_selectedDeviceIndex];
  }
  return "hex-right"; // Default fallback
}

void HexControllerWindow::RenderUI() {
  if (!m_showWindow) return;

  // Begin window - resizable
  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Device selection dropdown
  if (ImGui::BeginCombo("Select Controller", GetSelectedDeviceName().c_str())) {
    for (int i = 0; i < m_availableDevices.size(); i++) {
      bool isSelected = (m_selectedDeviceIndex == i);
      if (ImGui::Selectable(m_availableDevices[i].c_str(), isSelected)) {
        m_selectedDeviceIndex = i;
        m_logger->LogInfo("Selected controller: " + GetSelectedDeviceName());
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Get the current device name
  std::string deviceName = GetSelectedDeviceName();

  // Display device name
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Device: %s", deviceName.c_str());

  // Show controller connection status
  PIController* controller = m_controllerManager.GetController(deviceName);
  bool isConnected = controller && controller->IsConnected();

  ImGui::Separator();
  ImGui::Text("Status: %s", isConnected ? "Connected" : "Disconnected");

  // Add the FSM scan button
  if (isConnected) {
    ImGui::Separator();

    // Add some parameters that can be adjusted
    ImGui::Text("Scan Parameters:");

    // Axis selection dropdowns
    const char* axisOptions[] = { "X", "Y", "Z", "U", "V", "W" };
    int axis1Index = 0;
    int axis2Index = 1;

    for (int i = 0; i < IM_ARRAYSIZE(axisOptions); i++) {
      if (m_axis1 == axisOptions[i]) axis1Index = i;
      if (m_axis2 == axisOptions[i]) axis2Index = i;
    }

    if (ImGui::Combo("Axis 1", &axis1Index, axisOptions, IM_ARRAYSIZE(axisOptions))) {
      m_axis1 = axisOptions[axis1Index];
    }

    if (ImGui::Combo("Axis 2", &axis2Index, axisOptions, IM_ARRAYSIZE(axisOptions))) {
      m_axis2 = axisOptions[axis2Index];
    }

    // Simple input for other parameters
    ImGui::InputDouble("Length 1 (mm)", &m_length1, 0.1, 0.5);
    ImGui::InputDouble("Length 2 (mm)", &m_length2, 0.1, 0.5);
    ImGui::InputDouble("Distance (mm)", &m_distance, 0.01, 0.1);
    ImGui::InputDouble("Threshold (V)", &m_threshold, 0.1, 0.5);

    // Analog input depends on the device
    if (deviceName == "hex-left") {
      // Different defaults for hex-left
      if (m_selectedDeviceIndex != 0) { // First switch to hex-left
        m_analogInput = 5; // Default for hex-left
      }
    }
    else {
      // Different defaults for hex-right
      if (m_selectedDeviceIndex != 1) { // First switch to hex-right
        m_analogInput = 6; // Default for hex-right
      }
    }

    ImGui::InputInt("Analog Input", &m_analogInput);

    // Display channel information based on selection
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
      "Monitoring: %s-A-%d",
      deviceName.c_str(),
      m_analogInput);

    // Make the button more noticeable
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));

    std::string buttonText = "Start FSM Scan (" + deviceName + ")";

    if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 30))) {
      bool success = StartFSMScan();
      if (success) {
        m_logger->LogInfo("FSM Scan started successfully on " + deviceName);
      }
      else {
        m_logger->LogError("Failed to start FSM Scan on " + deviceName);
      }
    }

    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Controller not connected");
  }

  // Position display section - show current position
  if (isConnected) {
    ImGui::Separator();
    ImGui::Text("Current Position:");

    // Get positions for the controller
    std::map<std::string, double> positions;
    if (controller->GetPositions(positions)) {
      for (const auto& [axis, position] : positions) {
        ImGui::Text("%s: %.4f mm", axis.c_str(), position);
      }
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Failed to read positions");
    }
  }

  ImGui::End();
}

bool HexControllerWindow::StartFSMScan() {
  std::string deviceName = GetSelectedDeviceName();

  // Get the controller for the selected device
  PIController* controller = m_controllerManager.GetController(deviceName);

  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("HexControllerWindow: Cannot start FSM scan - controller not connected");
    return false;
  }

  // Call the FSM method
  m_logger->LogInfo("HexControllerWindow: Starting FSM scan on " + deviceName + " with parameters:");
  m_logger->LogInfo("  Axis 1: " + m_axis1 + ", Length: " + std::to_string(m_length1));
  m_logger->LogInfo("  Axis 2: " + m_axis2 + ", Length: " + std::to_string(m_length2));
  m_logger->LogInfo("  Threshold: " + std::to_string(m_threshold));
  m_logger->LogInfo("  Distance: " + std::to_string(m_distance));
  m_logger->LogInfo("  Analog Input: " + std::to_string(m_analogInput));

  return controller->FSM(
    m_axis1, m_length1,
    m_axis2, m_length2,
    m_threshold, m_distance,
    m_analogInput
  );
}