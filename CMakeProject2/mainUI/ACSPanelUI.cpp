// ACSPanelUI.cpp - Implementation of embedded ACS panel UI
#include "ACSPanelUI.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/acs_controller.h"
#include "include/motions/MotionConfigManager.h"
#include "imgui.h"
#include <map>
#include <algorithm>
#include <sstream>

ACSPanelUI::ACSPanelUI(ACSControllerManager& acsManager)
  : m_acsManager(acsManager), m_jogDistance(1.0) {
  // Constructor - initialize jog distance
}

ACSPanelUI::~ACSPanelUI() {
  // Destructor - no cleanup needed
}

void ACSPanelUI::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Calculate content size for left/right panel layout
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.25f;
  float rightPanelWidth = contentSize.x * 0.75f;

  // Left Panel - ACS Controller List (25% width)
  ImGui::BeginChild("LeftACSPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Selected Controller Interface (75% width)
  ImGui::BeginChild("RightACSPanel", ImVec2(rightPanelWidth, contentSize.y), false);
  RenderRightPanel();
  ImGui::EndChild();
}

void ACSPanelUI::RenderLeftPanel() {
  ImGui::Text("ACS Controllers");
  ImGui::Separator();

  // Connection controls for all controllers
  if (ImGui::Button("Connect All", ImVec2(-1, 30))) {
    m_acsManager.ConnectAll();
  }

  if (ImGui::Button("Disconnect All", ImVec2(-1, 30))) {
    m_acsManager.DisconnectAll();
  }

  ImGui::Separator();

  // Render controller list
  RenderControllerList();
}

void ACSPanelUI::RenderRightPanel() {
  if (m_selectedControllerName.empty()) {
    RenderNoSelectionMessage();
  }
  else {
    RenderSelectedControllerUI();
  }
}

void ACSPanelUI::RenderControllerList() {
  // Get all ACS controllers from the manager
  // Note: We need to access the config manager through the ACS manager
  // For now, we'll look for common ACS controller names based on your config

  std::vector<std::string> controllerNames = {
      "gantry-main"  // Based on your motion_config.json
  };

  for (const std::string& name : controllerNames) {
    ACSController* controller = m_acsManager.GetController(name);

    if (controller) {  // Only show controllers that exist
      ImGui::PushID(name.c_str());

      // Connection status indicator
      bool isConnected = controller->IsConnected();
      ImVec4 statusColor = isConnected ?
        ImVec4(0.0f, 0.8f, 0.0f, 1.0f) :  // Green for connected
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f);   // Red for disconnected

      // Status dot
      ImGui::TextColored(statusColor, "●");
      ImGui::SameLine();

      // Selectable controller name
      bool isSelected = (m_selectedControllerName == name);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        m_selectedControllerName = name;
      }

      // Show connection status text and controller type
      ImGui::Indent(20.0f);
      ImGui::TextColored(statusColor, "%s",
        isConnected ? "Connected" : "Disconnected");
      ImGui::Text("Type: Gantry System");

      // Show available axes for this controller
      std::vector<std::string> axes = GetControllerAxes(name);
      if (!axes.empty()) {
        std::string axesStr = "Axes: ";
        for (size_t i = 0; i < axes.size(); i++) {
          axesStr += axes[i];
          if (i < axes.size() - 1) axesStr += ", ";
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", axesStr.c_str());
      }

      ImGui::Unindent(20.0f);
      ImGui::Separator();
      ImGui::PopID();
    }
  }
}

std::vector<std::string> ACSPanelUI::GetControllerAxes(const std::string& controllerName) {
  // Default gantry axes - this should ideally come from the config manager
  // through the ACS manager, but for now we'll use typical gantry axes

  if (controllerName == "gantry-main") {
    return { "X", "Y", "Z" };  // Standard 3-axis gantry
  }

  // Default fallback
  return { "X", "Y", "Z" };
}

void ACSPanelUI::RenderSelectedControllerUI() {
  ACSController* controller = m_acsManager.GetController(m_selectedControllerName);

  if (!controller) {
    ImGui::Text("Controller '%s' not found", m_selectedControllerName.c_str());
    ImGui::Text("Please check the configuration.");

    if (ImGui::Button("Clear Selection")) {
      m_selectedControllerName.clear();
    }
    return;
  }

  // === EMBEDDED CONTROLLER UI ===
  RenderControllerHeader(controller);
  ImGui::Separator();

  if (controller->IsConnected()) {
    RenderMotionStatus(controller);
    ImGui::Separator();

    RenderJogControls(controller);
    ImGui::Separator();

    RenderPositionDisplay(controller);
    ImGui::Separator();

    RenderNamedPositions(controller);
    ImGui::Separator();

    RenderUtilityControls(controller);
  }
  else {
    RenderConnectionControls(controller);
  }
}

void ACSPanelUI::RenderControllerHeader(ACSController* controller) {
  ImGui::SetWindowFontScale(1.3f);
  ImGui::Text("ACS Controller: %s", m_selectedControllerName.c_str());
  ImGui::SetWindowFontScale(1.0f);

  // Connection status
  bool isConnected = controller->IsConnected();
  ImVec4 statusColor = isConnected ?
    ImVec4(0.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

  ImGui::Text("Status: ");
  ImGui::SameLine();
  ImGui::TextColored(statusColor, "%s", isConnected ? "Connected" : "Disconnected");

  if (isConnected) {
    ImGui::SameLine();
    if (ImGui::Button("Disconnect")) {
      controller->Disconnect();
    }
  }

  // Show controller type
  ImGui::Text("Type: Gantry System (ACS)");
}

void ACSPanelUI::RenderConnectionControls(ACSController* controller) {
  ImGui::Text("Connection Setup");
  ImGui::Separator();

  static char ipBuffer[64] = "192.168.0.50";
  static int port = 701;

  ImGui::Text("IP Address:");
  ImGui::SetNextItemWidth(200);
  ImGui::InputText("##IP", ipBuffer, sizeof(ipBuffer));

  ImGui::Text("Port:");
  ImGui::SetNextItemWidth(100);
  ImGui::InputInt("##Port", &port);

  ImGui::Spacing();
  if (ImGui::Button("Connect", ImVec2(120, 40))) {
    controller->Connect(ipBuffer, port);
  }

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: ACS controllers typically use port 701");
}

void ACSPanelUI::RenderMotionStatus(ACSController* controller) {
  ImGui::Text("Motion Status");

  // Get motion status for available axes
  std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);
  bool anyMoving = false;

  for (const std::string& axis : axes) {
    if (controller->IsMoving(axis)) {
      anyMoving = true;
      break;
    }
  }

  // Overall status
  ImVec4 statusColor = anyMoving ?
    ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 0.0f, 1.0f);

  ImGui::Text("System Status: ");
  ImGui::SameLine();
  ImGui::TextColored(statusColor, "%s", anyMoving ? "MOVING" : "IDLE");

  // Emergency stop button
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
  if (ImGui::Button("STOP ALL", ImVec2(100, 30))) {
    controller->StopAllAxes();
  }
  ImGui::PopStyleColor();
}

void ACSPanelUI::RenderJogControls(ACSController* controller) {
  ImGui::Text("Jog Controls");

  // Jog distance selection
  RenderJogDistanceControl();

  ImGui::Spacing();

  // Get available axes for this controller
  std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);

  // Jog buttons in a table
  if (ImGui::BeginTable("JogTable", 4, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Axis");
    ImGui::TableSetupColumn("Position");
    ImGui::TableSetupColumn("Status");
    ImGui::TableSetupColumn("Jog");
    ImGui::TableHeadersRow();

    for (const std::string& axis : axes) {
      ImGui::PushID(axis.c_str());
      ImGui::TableNextRow();

      // Axis name with description
      ImGui::TableNextColumn();
      std::string axisLabel = axis;
      if (axis == "X") axisLabel += " (Linear)";
      else if (axis == "Y") axisLabel += " (Linear)";
      else if (axis == "Z") axisLabel += " (Vertical)";
      ImGui::Text("%s", axisLabel.c_str());

      // Current position
      ImGui::TableNextColumn();
      double position = 0.0;
      if (controller->GetPosition(axis, position)) {
        ImGui::Text("%.3f", position);
      }
      else {
        ImGui::Text("N/A");
      }

      // Status
      ImGui::TableNextColumn();
      bool isMoving = controller->IsMoving(axis);
      ImVec4 color = isMoving ?
        ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
      ImGui::TextColored(color, "%s", isMoving ? "Moving" : "Idle");

      // Jog buttons
      ImGui::TableNextColumn();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
      if (ImGui::Button(("-##" + axis).c_str(), ImVec2(30, 25))) {
        controller->MoveRelative(axis, -m_jogDistance, false);
      }
      ImGui::PopStyleColor();

      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
      if (ImGui::Button(("+##" + axis).c_str(), ImVec2(30, 25))) {
        controller->MoveRelative(axis, m_jogDistance, false);
      }
      ImGui::PopStyleColor();

      ImGui::PopID();
    }

    ImGui::EndTable();
  }
}

void ACSPanelUI::RenderJogDistanceControl() {
  static const std::vector<double> jogDistanceValues = {
      0.001, 0.002, 0.005,
      0.01, 0.02, 0.05,
      0.1, 0.2, 0.5,
      1.0, 2.0, 5.0,
      10.0, 20.0, 50.0
  };

  // Find current index
  int currentIndex = 9; // Default to 1.0
  for (size_t i = 0; i < jogDistanceValues.size(); i++) {
    if (std::abs(m_jogDistance - jogDistanceValues[i]) < 0.0000001) {
      currentIndex = static_cast<int>(i);
      break;
    }
  }

  ImGui::Text("Jog Distance:");
  ImGui::SetNextItemWidth(150);

  // Create labels
  static std::vector<std::string> labels;
  if (labels.empty()) {
    for (double val : jogDistanceValues) {
      char buffer[32];
      if (val < 0.01) {
        snprintf(buffer, sizeof(buffer), "%.3f mm", val);
      }
      else if (val < 1.0) {
        snprintf(buffer, sizeof(buffer), "%.2f mm", val);
      }
      else {
        snprintf(buffer, sizeof(buffer), "%.1f mm", val);
      }
      labels.push_back(buffer);
    }
  }

  if (ImGui::BeginCombo("##JogDistance", labels[currentIndex].c_str())) {
    for (int i = 0; i < static_cast<int>(jogDistanceValues.size()); i++) {
      bool isSelected = (currentIndex == i);
      if (ImGui::Selectable(labels[i].c_str(), isSelected)) {
        m_jogDistance = jogDistanceValues[i];
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void ACSPanelUI::RenderPositionDisplay(ACSController* controller) {
  ImGui::Text("Current Positions");

  std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);

  if (ImGui::BeginTable("PositionTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Axis");
    ImGui::TableSetupColumn("Position");
    ImGui::TableSetupColumn("Unit");
    ImGui::TableHeadersRow();

    for (const std::string& axis : axes) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      // Axis name with description
      std::string axisLabel = axis;
      if (axis == "X") axisLabel += " (Linear)";
      else if (axis == "Y") axisLabel += " (Linear)";
      else if (axis == "Z") axisLabel += " (Vertical)";
      ImGui::Text("%s", axisLabel.c_str());

      ImGui::TableNextColumn();
      double position = 0.0;
      if (controller->GetPosition(axis, position)) {
        ImGui::Text("%.6f", position);
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableNextColumn();
      ImGui::Text("mm");
    }

    ImGui::EndTable();
  }

  // Copy position button (if ACSController has this method)
  if (ImGui::Button("Copy Position as JSON", ImVec2(200, 30))) {
    // Note: This would need to be implemented in ACSController
    // Similar to PIController::CopyPositionToClipboard()
  }
}

void ACSPanelUI::RenderNamedPositions(ACSController* controller) {
  ImGui::Text("Named Positions");

  // This would need access to the config manager to get named positions
  // for the ACS controller, similar to the PI implementation
  ImGui::Text("Named position controls will be implemented here");
  ImGui::Text("This requires access to MotionConfigManager through ACSControllerManager");

  // Example of how it could look:
  ImGui::Text("Available positions for %s:", m_selectedControllerName.c_str());
  ImGui::BulletText("home");
  ImGui::BulletText("safe");
  ImGui::BulletText("midback");
  ImGui::BulletText("seepic");
  // etc. - these would come from the config
}

void ACSPanelUI::RenderUtilityControls(ACSController* controller) {
  ImGui::Text("Utility Controls");

  // Homing controls
  if (ImGui::Button("Home All Axes", ImVec2(150, 30))) {
    std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);
    for (const std::string& axis : axes) {
      controller->HomeAxis(axis);
    }
  }

  ImGui::SameLine();

  // Enable/disable axes
  if (ImGui::Button("Enable All Axes", ImVec2(150, 30))) {
    std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);
    for (const std::string& axis : axes) {
      controller->EnableServo(axis, true);
    }
  }

  ImGui::Spacing();

  // Velocity Controls
  RenderVelocityControls(controller);

  // Debug controls (if available)
  // Note: ACSController might not have debug verbose like PIController
  ImGui::Spacing();
  ImGui::Text("Debug & Diagnostics");

  if (ImGui::Button("Check Axis Status", ImVec2(150, 30))) {
    // Could trigger status refresh or detailed diagnostics
  }
}

void ACSPanelUI::RenderNoSelectionMessage() {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("ACS Controller Management");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select an ACS controller from the list on the left to view");
  ImGui::Text("its controls and status information.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("ACS Controllers (Gantry Systems):");
  ImGui::BulletText("High-precision linear motion systems");
  ImGui::BulletText("Typically XYZ or XY axis configurations");
  ImGui::BulletText("Used for gantry and positioning applications");

  ImGui::Spacing();
  ImGui::Text("Available Operations:");
  ImGui::BulletText("Connect/Disconnect individual controllers");
  ImGui::BulletText("View real-time position and status");
  ImGui::BulletText("Manual jog controls for each axis");
  ImGui::BulletText("Home all axes");
  ImGui::BulletText("Move to named positions");

  ImGui::Spacing();
  ImGui::Text("Use the 'Connect All' and 'Disconnect All' buttons");
  ImGui::Text("in the left panel to control all controllers at once.");
}

void ACSPanelUI::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

void ACSPanelUI::RenderVelocityControls(ACSController* controller) {
  ImGui::Text("Velocity Control");

  std::vector<std::string> axes = GetControllerAxes(m_selectedControllerName);

  // Initialize velocity map if empty
  if (m_axisVelocities.empty()) {
    for (const std::string& axis : axes) {
      m_axisVelocities[axis] = 10.0; // Default velocity
    }
  }

  if (ImGui::BeginTable("VelocityTable", 4, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Axis");
    ImGui::TableSetupColumn("Current Vel");
    ImGui::TableSetupColumn("Set Velocity");
    ImGui::TableSetupColumn("Apply");
    ImGui::TableHeadersRow();

    for (const std::string& axis : axes) {
      ImGui::PushID(axis.c_str());
      ImGui::TableNextRow();

      // Axis name
      ImGui::TableNextColumn();
      ImGui::Text("%s", axis.c_str());

      // Current velocity (read from controller if available)
      ImGui::TableNextColumn();
      double currentVel = 0.0;
      if (controller->GetVelocity(axis, currentVel)) {
        ImGui::Text("%.2f", currentVel);
      }
      else {
        ImGui::Text("N/A");
      }

      // Set velocity input - convert double to float for ImGui
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(80);
      float velocityFloat = static_cast<float>(m_axisVelocities[axis]);
      if (ImGui::DragFloat(("##vel" + axis).c_str(), &velocityFloat, 0.1f, 0.1f, 100.0f, "%.2f")) {
        m_axisVelocities[axis] = static_cast<double>(velocityFloat);
      }

      // Apply button
      ImGui::TableNextColumn();
      if (ImGui::Button(("Set##" + axis).c_str(), ImVec2(50, 25))) {
        controller->SetVelocity(axis, m_axisVelocities[axis]);
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  ImGui::Spacing();

  // Quick velocity presets
  ImGui::Text("Quick Presets:");

  if (ImGui::Button("Slow (1.0)", ImVec2(80, 25))) {
    for (const std::string& axis : axes) {
      m_axisVelocities[axis] = 1.0;
      controller->SetVelocity(axis, 1.0);
    }
  }
  ImGui::SameLine();

  if (ImGui::Button("Medium (10.0)", ImVec2(80, 25))) {
    for (const std::string& axis : axes) {
      m_axisVelocities[axis] = 10.0;
      controller->SetVelocity(axis, 10.0);
    }
  }
  ImGui::SameLine();

  if (ImGui::Button("Fast (25.0)", ImVec2(80, 25))) {
    for (const std::string& axis : axes) {
      m_axisVelocities[axis] = 25.0;
      controller->SetVelocity(axis, 25.0);
    }
  }
  ImGui::SameLine();

  if (ImGui::Button("Max (50.0)", ImVec2(80, 25))) {
    for (const std::string& axis : axes) {
      m_axisVelocities[axis] = 50.0;
      controller->SetVelocity(axis, 50.0);
    }
  }
}