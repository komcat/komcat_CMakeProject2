// HexRightControllerWindow.cpp
#include "HexRightControllerWindow.h"

HexRightControllerWindow::HexRightControllerWindow(PIControllerManager& controllerManager)
  : m_controllerManager(controllerManager) {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("HexRightControllerWindow initialized");
}

void HexRightControllerWindow::RenderUI() {
  if (!m_showWindow) return;

  // Set fixed window size
  //ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_Always);

  // Begin window
  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Display device name
  ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Device: hex-right");

  // Show controller connection status
  PIController* controller = m_controllerManager.GetController("hex-right");
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
    ImGui::InputInt("Analog Input", &m_analogInput);

    // Make the button more noticeable
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));

    if (ImGui::Button("Start FSM Scan", ImVec2(-1, 30))) {
      bool success = StartFSMScan();
      if (success) {
        m_logger->LogInfo("FSM Scan started successfully");
      }
      else {
        m_logger->LogError("Failed to start FSM Scan");
      }
    }

    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Controller not connected");
  }

  ImGui::End();
}

bool HexRightControllerWindow::StartFSMScan() {
  // Get the controller for hex-right
  PIController* controller = m_controllerManager.GetController("hex-right");

  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("HexRightControllerWindow: Cannot start FSM scan - controller not connected");
    return false;
  }

  // Call the FSM method
  m_logger->LogInfo("HexRightControllerWindow: Starting FSM scan with parameters:");
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