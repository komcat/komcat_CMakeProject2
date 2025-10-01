#include "EmbeddedJogControl.h"
#include "pi_controller_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

EmbeddedJogControl::EmbeddedJogControl(GlobalMotionController& motionController)
  : m_motionController(motionController) {
  // Set up motion controller callbacks
  m_motionController.SetStatusCallback([this](const std::string& msg) {
    UpdateStatus(msg);
  });
}

void EmbeddedJogControl::Render() {
  Render("Motion Control - Global Coordinates");
}

void EmbeddedJogControl::Render(const std::string& title) {
  // Process keyboard input first
  ProcessKeyboardInput();

  if (!title.empty()) {
    ImGui::Text("%s", title.c_str());
    ImGui::Separator();
  }

  // Check if any controller is available
  if (!m_motionController.GetPIController() && !m_motionController.GetACSController()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No motion controllers available");
    return;
  }

  RenderDeviceSelector();

  // Add keyboard toggle button after device selector
  ImGui::Spacing();

  // Keyboard enable toggle with visual feedback
  bool wasKeyboardEnabled = m_keyboardEnabled;

  if (wasKeyboardEnabled) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
  }

  if (ImGui::Button(m_keyboardEnabled ? "Keyboard: ON" : "Keyboard: OFF", ImVec2(120, 0))) {
    m_keyboardEnabled = !m_keyboardEnabled;
    UpdateStatus(m_keyboardEnabled ? "Keyboard control enabled" : "Keyboard control disabled");
  }

  if (wasKeyboardEnabled) {
    ImGui::PopStyleColor(2);
  }

  // Show keyboard hints when enabled
  if (m_keyboardEnabled) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    if (ImGui::Button("?", ImVec2(20, 0))) {
      ImGui::OpenPopup("KeyboardHelp");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("KeyboardHelp")) {
      ImGui::Text("Keyboard Controls:");
      ImGui::Separator();
      ImGui::Text("A / D : Move X-axis");
      ImGui::Text("W / S : Move Y-axis");
      ImGui::Text("R / F : Move Z-axis");
      ImGui::EndPopup();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click for keyboard shortcuts");
    }
  }

  if (!m_compactMode) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  RenderStepControls();

  if (!m_compactMode) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // Render linear jog buttons
  if (m_compactMode) {
    RenderCompactJogButtons();
    RenderCompactRotationalJogButtons();  // ADD THIS LINE
  }
  else {
    RenderJogButtons();
    RenderRotationalJogButtons();  // ADD THIS LINE
  }

  if (m_showPositionDisplay) {
    if (!m_compactMode) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
    RenderPositionDisplay();
  }

  if (m_showTransformMatrix) {
    RenderTransformMatrixInfo();
  }

  ImGui::Spacing();
  RenderStopButton();
}


// Add these color definitions at the top of RenderDeviceSelector()
void EmbeddedJogControl::RenderDeviceSelector() {
  auto availableDevices = m_motionController.GetAvailableDevices();

  if (availableDevices.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No devices connected");
    return;
  }

  ImGui::Text("Device:");

  // Define colors for active/inactive states
  ImVec4 activeColor = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);
  ImVec4 activeHoverColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);

  // Render device buttons in a row
  for (size_t i = 0; i < availableDevices.size(); i++) {
    bool isActive = (m_jogState.selectedDevice == static_cast<int>(i));

    // Push active color if this is the selected device
    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
    }

    if (ImGui::Button(availableDevices[i].c_str(), ImVec2(80, 0))) {
      m_jogState.selectedDevice = static_cast<int>(i);
      m_jogState.activeDeviceId = availableDevices[i];
      UpdateStatus("Switched to " + m_jogState.activeDeviceId);
    }

    if (isActive) {
      ImGui::PopStyleColor(2);
    }

    // Add SameLine for all but last button
    if (i < availableDevices.size() - 1) {
      ImGui::SameLine();
    }
  }
}

void EmbeddedJogControl::RenderStepControls() {
  ImGui::Text("Step Size:");

  // Define colors for active state
  ImVec4 activeColor = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);
  ImVec4 activeHoverColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);

  // Helper lambda to render step button with active state
  auto renderStepButton = [&](const char* label, float value, bool sameLine = true) {
    bool isActive = (std::abs(m_jogState.stepSize - value) < 0.0001f);

    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoverColor);
    }

    if (ImGui::Button(label)) {
      m_jogState.stepSize = value;
    }

    if (isActive) {
      ImGui::PopStyleColor(2);
    }

    if (sameLine) {
      ImGui::SameLine();
    }
  };

  // Row 1: 1um, 2um, 5um, 10um
  renderStepButton("1um", 0.001f);
  renderStepButton("2um", 0.002f);
  renderStepButton("5um", 0.005f);
  renderStepButton("10um", 0.01f, false);

  // Row 2: 20um, 50um
  renderStepButton("20um", 0.02f);
  renderStepButton("50um", 0.05f, false);

  // Custom input field
  ImGui::SetNextItemWidth(80);
  ImGui::InputFloat("##Step_mm", &m_jogState.stepSize, 0, 0, "%.3f");
  ImGui::SameLine();
  ImGui::Text("mm");

  if (!m_compactMode) {
    ImGui::Text("Velocity:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("##Vel", &m_jogState.velocity, 0, 0, "%.1f");
    ImGui::SameLine();
    ImGui::Text("mm/s");
  }
}


// Update RenderJogButtons to show key hints when keyboard is enabled
void EmbeddedJogControl::RenderJogButtons() {
  ImGui::Text("Jog Controls (Global Frame):");

  // Add keyboard hint if enabled
  if (m_keyboardEnabled) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[Keys Active]");
  }

  float buttonSize = 60.0f;
  ImVec4 activeColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
  ImVec4 hoverColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);

  // Y+ button with key hint
  ImGui::Dummy(ImVec2(buttonSize, 0));
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button(m_keyboardEnabled ? "+Y [W]" : "+Y", ImVec2(buttonSize, buttonSize))) {
    HandleJogMovement(1, -m_jogState.stepSize);
  }
  ImGui::PopStyleColor(2);

  // X-, HOME, X+ buttons with key hints
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button(m_keyboardEnabled ? "-X [A]" : "-X", ImVec2(buttonSize, buttonSize))) {
    HandleJogMovement(0, -m_jogState.stepSize);
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.7f, 1.0f));
  if (ImGui::Button(":)", ImVec2(buttonSize, buttonSize))) {
    //UpdateStatus("Homing " + m_jogState.activeDeviceId);
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button(m_keyboardEnabled ? "+X [D]" : "+X", ImVec2(buttonSize, buttonSize))) {
    HandleJogMovement(0, m_jogState.stepSize);
  }
  ImGui::PopStyleColor(2);

  // Y- button with key hint
  ImGui::Dummy(ImVec2(buttonSize, 0));
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button(m_keyboardEnabled ? "-Y [S]" : "-Y", ImVec2(buttonSize, buttonSize))) {
    HandleJogMovement(1, m_jogState.stepSize);
  }
  ImGui::PopStyleColor(2);

  // Z controls with key hints
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(20, 0));
  ImGui::SameLine();

  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button(m_keyboardEnabled ? "+Z [R]" : "+Z", ImVec2(buttonSize, buttonSize / 2 - 2))) {
    HandleJogMovement(2, m_jogState.stepSize);
  }
  if (ImGui::Button(m_keyboardEnabled ? "-Z [F]" : "-Z", ImVec2(buttonSize, buttonSize / 2 - 2))) {
    HandleJogMovement(2, -m_jogState.stepSize);
  }
  ImGui::PopStyleColor(2);
  ImGui::EndGroup();
}



void EmbeddedJogControl::RenderCompactJogButtons() {
  float btnSize = 35.0f;

  if (ImGui::Button("-X", ImVec2(btnSize, btnSize))) HandleJogMovement(0, -m_jogState.stepSize);
  ImGui::SameLine();
  if (ImGui::Button("+X", ImVec2(btnSize, btnSize))) HandleJogMovement(0, m_jogState.stepSize);
  ImGui::SameLine();
  if (ImGui::Button("-Y", ImVec2(btnSize, btnSize))) HandleJogMovement(1, m_jogState.stepSize);
  ImGui::SameLine();
  if (ImGui::Button("+Y", ImVec2(btnSize, btnSize))) HandleJogMovement(1, -m_jogState.stepSize);
  ImGui::SameLine();
  if (ImGui::Button("-Z", ImVec2(btnSize, btnSize))) HandleJogMovement(2, -m_jogState.stepSize);
  ImGui::SameLine();
  if (ImGui::Button("+Z", ImVec2(btnSize, btnSize))) HandleJogMovement(2, m_jogState.stepSize);
}


void EmbeddedJogControl::RenderTransformMatrixInfo() {
  if (ImGui::TreeNode("Transformation Matrix")) {
    // The GlobalMotionController stores the matrices internally
    // We could add a method to retrieve and display them if needed
    ImGui::Text("Device: %s", m_jogState.activeDeviceId.c_str());
    ImGui::Text("Matrix display not yet implemented");
    ImGui::TreePop();
  }
}

void EmbeddedJogControl::RenderStopButton() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));

  if (ImGui::Button("EMERGENCY STOP", ImVec2(-1, 30))) {
    m_motionController.EmergencyStopGlobal();
    UpdateStatus("EMERGENCY STOP - All motion halted");
  }

  ImGui::PopStyleColor(2);
}

void EmbeddedJogControl::HandleJogMovement(int axis, float distance) {
  // Check if already moving
  if (m_motionController.IsAnyMovementPending()) {
    UpdateStatus("Movement already in progress");
    return;
  }

  m_isMoving = true;

  // Use async movement
  bool success = m_motionController.JogGlobalAsync(m_jogState.activeDeviceId,
    axis, distance, m_jogState.velocity);

  if (success) {
    std::string axisName = (axis == 0) ? "X" : (axis == 1) ? "Y" : "Z";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "Moving " << axisName << ": " << std::showpos << distance << " mm";
    UpdateStatus(ss.str());
  }
  else {
    UpdateStatus("Movement failed to start!");
    m_isMoving = false;
  }

  // Note: m_isMoving will be cleared when movement completes
}

// Update RenderPositionDisplay to show movement status
void EmbeddedJogControl::RenderPositionDisplay() {
  GlobalPosition pos = m_motionController.GetGlobalPosition(m_jogState.activeDeviceId);

  ImGui::Text("Global Position:");
  ImGui::Text("  X: %8.3f mm", pos.x);
  ImGui::Text("  Y: %8.3f mm", pos.y);
  ImGui::Text("  Z: %8.3f mm", pos.z);

  // Show if device is moving
  if (m_motionController.IsDeviceMoving(m_jogState.activeDeviceId) ||
    m_motionController.IsAnyMovementPending()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "MOVING");
    m_isMoving = true;
  }
  else {
    m_isMoving = false;
  }
}


std::string EmbeddedJogControl::GetDeviceIdFromSelection(int selection) {
  switch (selection) {
  case 0: return "hex-left";
  case 1: return "hex-right";
  case 2: return "hex-bottom";
  case 3: return "gantry-main";
  default: return "hex-left";
  }
}

void EmbeddedJogControl::UpdateStatus(const std::string& message) {
  if (m_statusCallback) {
    m_statusCallback(message);
  }
  else {
    std::cout << "JogControl: " << message << std::endl;
  }
}

// Add this method to handle keyboard input
void EmbeddedJogControl::ProcessKeyboardInput() {
  if (!m_keyboardEnabled) return;

  ImGuiIO& io = ImGui::GetIO();

  // Only process if no text input is active
  if (io.WantTextInput) return;

  // Check for key presses (using ImGui's key system)
  if (ImGui::IsKeyPressed(ImGuiKey_A)) {
    HandleJogMovement(0, -m_jogState.stepSize);
    UpdateStatus("Key: -X");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_D)) {
    HandleJogMovement(0, m_jogState.stepSize);
    UpdateStatus("Key: +X");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_W)) {
    HandleJogMovement(1, -m_jogState.stepSize);
    UpdateStatus("Key: +Y");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_S)) {
    HandleJogMovement(1, m_jogState.stepSize);
    UpdateStatus("Key: -Y");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_R)) {
    HandleJogMovement(2, m_jogState.stepSize);
    UpdateStatus("Key: +Z");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_F)) {
    HandleJogMovement(2, -m_jogState.stepSize);
    UpdateStatus("Key: -Z");
  }
}

void EmbeddedJogControl::HandleHexapodRotation(int rotAxis, float degrees) {
  // rotAxis: 0=U(Roll), 1=V(Pitch), 2=W(Yaw)

  // Only for hexapod devices
  if (m_jogState.selectedDevice >= 3) {
    UpdateStatus("Rotation not available for gantry");
    return;
  }

  // Check if already moving
  if (m_motionController.IsAnyMovementPending()) {
    UpdateStatus("Movement already in progress");
    return;
  }

  // Get the PI controller manager
  auto* piControllerManager = m_motionController.GetPIController();
  if (!piControllerManager) {
    UpdateStatus("PI Controller Manager not available");
    return;
  }

  // Get the specific controller for this device
  auto* controller = piControllerManager->GetController(m_jogState.activeDeviceId);
  if (!controller) {
    UpdateStatus("Controller not found for " + m_jogState.activeDeviceId);
    return;
  }

  // Map axis index to PI axis names
  std::string axisName;
  switch (rotAxis) {
  case 0: axisName = "U"; break;
  case 1: axisName = "V"; break;
  case 2: axisName = "W"; break;
  default: return;
  }

  m_isMoving = true;

  // Call MoveRelative on the specific controller
  bool success = controller->MoveRelative(axisName, degrees);

  if (success) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "Rotating " << axisName << ": " << std::showpos << degrees << " deg";
    UpdateStatus(ss.str());
  }
  else {
    UpdateStatus("Rotation failed!");
    m_isMoving = false;
  }
}


// Update RenderRotationalJogButtons - no keyboard shortcuts
void EmbeddedJogControl::RenderRotationalJogButtons() {
  // Only show for hexapod devices
  if (m_jogState.selectedDevice >= 3) {
    return;
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Hexapod Rotation (Local):");

  float buttonSize = 60.0f;
  ImVec4 activeColor = ImVec4(0.6f, 0.2f, 0.6f, 1.0f);
  ImVec4 hoverColor = ImVec4(0.7f, 0.3f, 0.7f, 1.0f);

  // Calculate rotation step in degrees
  float rotStep = m_jogState.stepSize * 10.0f; // Convert mm to degrees

  // U (Roll) controls
  ImGui::Text("U (Roll):");
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button("-U", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(0, -rotStep);
  }
  ImGui::SameLine();
  if (ImGui::Button("+U", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(0, rotStep);
  }
  ImGui::PopStyleColor(2);

  // V (Pitch) controls
  ImGui::Text("V (Pitch):");
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button("-V", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(1, -rotStep);
  }
  ImGui::SameLine();
  if (ImGui::Button("+V", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(1, rotStep);
  }
  ImGui::PopStyleColor(2);

  // W (Yaw) controls
  ImGui::Text("W (Yaw):");
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
  if (ImGui::Button("-W", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(2, -rotStep);
  }
  ImGui::SameLine();
  if (ImGui::Button("+W", ImVec2(buttonSize, buttonSize / 2))) {
    HandleHexapodRotation(2, rotStep);
  }
  ImGui::PopStyleColor(2);

  ImGui::Text("Rot Step: %.3f deg", rotStep);
}

// Compact version
void EmbeddedJogControl::RenderCompactRotationalJogButtons() {
  if (m_jogState.selectedDevice >= 3) {
    return;
  }

  float btnSize = 35.0f;
  float rotStep = m_jogState.stepSize * 10.0f;

  ImGui::Text("Rot:");
  ImGui::SameLine();

  if (ImGui::Button("-U", ImVec2(btnSize, btnSize))) HandleHexapodRotation(0, -rotStep);
  ImGui::SameLine();
  if (ImGui::Button("+U", ImVec2(btnSize, btnSize))) HandleHexapodRotation(0, rotStep);
  ImGui::SameLine();
  if (ImGui::Button("-V", ImVec2(btnSize, btnSize))) HandleHexapodRotation(1, -rotStep);
  ImGui::SameLine();
  if (ImGui::Button("+V", ImVec2(btnSize, btnSize))) HandleHexapodRotation(1, rotStep);
  ImGui::SameLine();
  if (ImGui::Button("-W", ImVec2(btnSize, btnSize))) HandleHexapodRotation(2, -rotStep);
  ImGui::SameLine();
  if (ImGui::Button("+W", ImVec2(btnSize, btnSize))) HandleHexapodRotation(2, rotStep);
}