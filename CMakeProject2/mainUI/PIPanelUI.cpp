// PIPanelUI.cpp - Implementation of embedded PI panel UI
#include "PIPanelUI.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/pi_controller.h"
#include "include/motions/MotionConfigManager.h"
#include "imgui.h"
#include <map>
#include <iomanip>
#include <sstream>

PIPanelUI::PIPanelUI(PIControllerManager& piManager)
  : m_piManager(piManager), m_jogDistance(1.0), m_systemVelocity(10.0) {
  // Constructor - initialize jog distance and system velocity
}

PIPanelUI::~PIPanelUI() {
  // Destructor - no cleanup needed
}

void PIPanelUI::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Calculate content size for left/right panel layout
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.25f;
  float rightPanelWidth = contentSize.x * 0.75f;

  // Left Panel - PI Controller List (25% width)
  ImGui::BeginChild("LeftPIPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Selected Controller Interface (75% width)
  ImGui::BeginChild("RightPIPanel", ImVec2(rightPanelWidth, contentSize.y), false);
  RenderRightPanel();
  ImGui::EndChild();
}

void PIPanelUI::RenderLeftPanel() {
  ImGui::Text("PI Controllers");
  ImGui::Separator();

  // Connection controls for all controllers
  if (ImGui::Button("Connect All", ImVec2(-1, 30))) {
    m_piManager.ConnectAll();
  }

  if (ImGui::Button("Disconnect All", ImVec2(-1, 30))) {
    m_piManager.DisconnectAll();
  }

  ImGui::Separator();

  // Render controller list
  RenderControllerList();
}

void PIPanelUI::RenderRightPanel() {
  if (m_selectedControllerName.empty()) {
    RenderNoSelectionMessage();
  }
  else {
    RenderSelectedControllerUI();
  }
}

void PIPanelUI::RenderControllerList() {
  // Get all devices from the config manager (via PI manager)
  // For now, we'll iterate through known controller names
  // In a real implementation, you might want to add a method to PIControllerManager
  // to get the list of controller names

  std::vector<std::string> controllerNames = {
      "hex-left", "hex-right", "gantry-main"  // Common PI controller names
  };

  for (const std::string& name : controllerNames) {
    PIController* controller = m_piManager.GetController(name);

    if (controller) {  // Only show controllers that exist
      ImGui::PushID(name.c_str());

      // Connection status indicator
      bool isConnected = controller->IsConnected();
      ImVec4 statusColor = isConnected ?
        ImVec4(0.0f, 0.8f, 0.0f, 1.0f) :  // Green for connected
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f);   // Red for disconnected

      // Status dot
      ImGui::TextColored(statusColor, reinterpret_cast<const char*>(u8"●"));
      ImGui::SameLine();

      // Selectable controller name
      bool isSelected = (m_selectedControllerName == name);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        m_selectedControllerName = name;
      }

      // Show connection status text
      ImGui::Indent(20.0f);
      ImGui::TextColored(statusColor, "%s",
        isConnected ? "Connected" : "Disconnected");
      ImGui::Unindent(20.0f);

      ImGui::Separator();
      ImGui::PopID();
    }
  }
}

void PIPanelUI::RenderSelectedControllerUI() {
  PIController* controller = m_piManager.GetController(m_selectedControllerName);

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

void PIPanelUI::RenderControllerHeader(PIController* controller) {
  ImGui::SetWindowFontScale(1.3f);
  ImGui::Text("Controller: %s", m_selectedControllerName.c_str());
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
}

void PIPanelUI::RenderConnectionControls(PIController* controller) {
  ImGui::Text("Connection Setup");
  ImGui::Separator();

  static char ipBuffer[64] = "192.168.0.10";
  static int port = 50000;

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
}

void PIPanelUI::RenderMotionStatus(PIController* controller) {
  ImGui::Text("Motion Status");

  // Get motion status for all axes
  std::vector<std::string> axes = { "X", "Y", "Z", "U", "V", "W" };
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

void PIPanelUI::RenderJogControls(PIController* controller) {
  ImGui::Text("Jog Controls");

  // System velocity control
  RenderSystemVelocityControl(controller);

  ImGui::Spacing();

  // Jog distance selection
  RenderJogDistanceControl();

  ImGui::Spacing();

  // Jog buttons in a table
  if (ImGui::BeginTable("JogTable", 4, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Axis");
    ImGui::TableSetupColumn("Position");
    ImGui::TableSetupColumn("Status");
    ImGui::TableSetupColumn("Jog");
    ImGui::TableHeadersRow();

    std::vector<std::pair<std::string, std::string>> axisLabels = {
        {"X", "X"}, {"Y", "Y"}, {"Z", "Z"},
        {"U", "U (Roll)"}, {"V", "V (Pitch)"}, {"W", "W (Yaw)"}
    };

    for (const auto& [axis, label] : axisLabels) {
      ImGui::PushID(axis.c_str());
      ImGui::TableNextRow();

      // Axis name
      ImGui::TableNextColumn();
      ImGui::Text("%s", label.c_str());

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

void PIPanelUI::RenderSystemVelocityControl(PIController* controller) {
  ImGui::Text("System Velocity Control");

  // Get current system velocity from controller
  double currentVelocity = 0.0;
  bool velocityReadSuccess = controller->GetSystemVelocity(currentVelocity);

  if (velocityReadSuccess) {
    m_systemVelocity = currentVelocity;
  }

  // Velocity input field
  ImGui::Text("Velocity:");
  ImGui::SetNextItemWidth(120);

  // Use a temporary variable for the input to handle formatting
  float velocityInput = static_cast<float>(m_systemVelocity);

  if (ImGui::InputFloat("##SystemVelocity", &velocityInput, 0.1f, 1.0f, "%.2f")) {
    if (velocityInput > 0.0f && velocityInput <= 100.0f) {  // Reasonable velocity limits
      m_systemVelocity = static_cast<double>(velocityInput);
    }
  }

  ImGui::SameLine();
  ImGui::Text("mm/s");

  // Set velocity button
  ImGui::SameLine();
  if (ImGui::Button("Set Velocity", ImVec2(100, 0))) {
    if (controller->SetSystemVelocity(m_systemVelocity)) {
      // Success - velocity set
      ImGui::SetTooltip("Velocity set successfully");
    }
    else {
      // Failed to set velocity
      ImGui::SetTooltip("Failed to set velocity");
    }
  }

  // Show current velocity status
  if (velocityReadSuccess) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Current: %.2f mm/s", currentVelocity);
  }
  else {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Current: Unable to read");
  }

  // Velocity presets
  ImGui::Spacing();
  ImGui::Text("Quick Presets:");

  // Preset buttons
  std::vector<std::pair<double, std::string>> presets = {
      {1.0, "Slow"},
      {5.0, "Medium"},
      {10.0, "Fast"},
      {20.0, "Very Fast"}
  };

  for (size_t i = 0; i < presets.size(); i++) {
    if (i > 0) ImGui::SameLine();

    const auto& [velocity, label] = presets[i];

    // Highlight current preset if it matches
    bool isCurrentPreset = (std::abs(m_systemVelocity - velocity) < 0.01);
    if (isCurrentPreset) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.8f, 1.0f));
    }

    if (ImGui::Button((label + "##" + std::to_string(i)).c_str(), ImVec2(60, 25))) {
      m_systemVelocity = velocity;
      controller->SetSystemVelocity(m_systemVelocity);
    }

    if (isCurrentPreset) {
      ImGui::PopStyleColor();
    }

    // Tooltip showing velocity value
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%.1f mm/s", velocity);
    }
  }
}
void PIPanelUI::RenderJogDistanceControl() {
  static const std::vector<double> jogDistanceValues = {
      0.0001, 0.0002, 0.0005,
      0.001, 0.002, 0.005,
      0.01, 0.02, 0.05,
      0.1, 0.2, 0.5,
      1.0, 2.0, 5.0
  };

  // Find current index
  int currentIndex = 6; // Default to 0.01
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
      if (val < 0.001) {
        snprintf(buffer, sizeof(buffer), "%.4f mm", val);
      }
      else if (val < 0.01) {
        snprintf(buffer, sizeof(buffer), "%.3f mm", val);
      }
      else {
        snprintf(buffer, sizeof(buffer), "%.2f mm", val);
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


void PIPanelUI::RenderPositionDisplay(PIController* controller) {
  ImGui::Text("Current Positions");

  if (ImGui::BeginTable("PositionTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Axis");
    ImGui::TableSetupColumn("Position");
    ImGui::TableSetupColumn("Unit");
    ImGui::TableHeadersRow();

    std::vector<std::pair<std::string, std::string>> axes = {
        {"X", "Linear"}, {"Y", "Linear"}, {"Z", "Linear"},
        {"U", "Rotation"}, {"V", "Rotation"}, {"W", "Rotation"}
    };

    for (const auto& [axis, type] : axes) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", axis.c_str());

      ImGui::TableNextColumn();
      double position = 0.0;
      if (controller->GetPosition(axis, position)) {
        ImGui::Text("%.6f", position);
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableNextColumn();
      ImGui::Text("%s", (type == "Linear") ? "mm" : "deg");
    }

    ImGui::EndTable();
  }

  // Copy position button
  if (ImGui::Button("Copy Position as JSON", ImVec2(200, 30))) {
    if (controller->CopyPositionToClipboard()) {
      // Success feedback could be added here
    }
  }
}

void PIPanelUI::RenderNamedPositions(PIController* controller) {
  ImGui::Text("Named Positions");

  // This would need access to the config manager to get named positions
  // For now, show placeholder
  ImGui::Text("Named position controls will be implemented here");
  ImGui::Text("This requires access to MotionConfigManager through PIControllerManager");
}

void PIPanelUI::RenderUtilityControls(PIController* controller) {
  ImGui::Text("Utility Controls");

  // Debug controls
  bool debugVerbose = controller->GetDebugVerbose();
  if (ImGui::Checkbox("Verbose Debug", &debugVerbose)) {
    controller->SetDebugVerbose(debugVerbose);
  }

  // Additional utility buttons could go here
}

void PIPanelUI::RenderNoSelectionMessage() {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("PI Controller Management");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a PI controller from the list on the left to view");
  ImGui::Text("its controls and status information.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Available Operations:");
  ImGui::BulletText("Connect/Disconnect individual controllers");
  ImGui::BulletText("View real-time position and status");
  ImGui::BulletText("Manual jog controls");
  ImGui::BulletText("Move to named positions");
  ImGui::BulletText("Copy position data to clipboard");

  ImGui::Spacing();
  ImGui::Text("Use the 'Connect All' and 'Disconnect All' buttons");
  ImGui::Text("in the left panel to control all controllers at once.");
}

void PIPanelUI::ToggleWindow() {
  m_showWindow = !m_showWindow;
}