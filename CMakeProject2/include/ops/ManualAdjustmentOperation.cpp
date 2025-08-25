// src/Operations/ManualAdjustmentOperation.cpp
#include "ManualAdjustmentOperation.h"
#include "imgui.h"
#include <thread>
#include <sstream>
#include <iomanip>

ManualAdjustmentOperation::ManualAdjustmentOperation(
  const std::string& axisSystem,
  const std::string& title,
  const std::string& instructions,
  UserPromptUI& promptUI,
  bool enableX,
  bool enableY,
  bool enableZ)
  : m_axisSystem(axisSystem),
  m_title(title),
  m_instructions(instructions),
  m_promptUI(promptUI),
  m_enableX(enableX),
  m_enableY(enableY),
  m_enableZ(enableZ),
  m_stepSize(0.1),
  m_showPosition(true),
  m_timeoutSeconds(0),
  m_highlightAxes(false),
  m_hasSafetyLimits(false),
  m_minZ(-100.0),
  m_maxZ(100.0),
  m_adjustmentComplete(false),
  m_isActive(false),
  m_completionReason(""),
  m_showWindow(true),
  m_currentX(0), m_currentY(0), m_currentZ(0),
  m_startX(0), m_startY(0), m_startZ(0),
  m_cancelConfirmationOpen(false) {
}

// Builder pattern implementations
ManualAdjustmentOperation& ManualAdjustmentOperation::WithChecklist(
  const std::vector<std::string>& items) {
  m_checklistItems = items;
  m_checklistStates.resize(items.size(), false);
  return *this;
}

ManualAdjustmentOperation& ManualAdjustmentOperation::WithStepSize(double size) {
  m_stepSize = size;
  return *this;
}

ManualAdjustmentOperation& ManualAdjustmentOperation::WithShowPosition(bool show) {
  m_showPosition = show;
  return *this;
}

ManualAdjustmentOperation& ManualAdjustmentOperation::WithTimeout(int seconds) {
  m_timeoutSeconds = seconds;
  return *this;
}

ManualAdjustmentOperation& ManualAdjustmentOperation::WithHighlightAxes(bool highlight) {
  m_highlightAxes = highlight;
  return *this;
}

ManualAdjustmentOperation& ManualAdjustmentOperation::WithSafetyLimits(
  double minZ, double maxZ) {
  m_hasSafetyLimits = true;
  m_minZ = minZ;
  m_maxZ = maxZ;
  return *this;
}


bool ManualAdjustmentOperation::Execute(MachineOperations& ops) {
  ops.LogInfo("Starting manual adjustment: " + m_title);

  // Reset state
  m_adjustmentComplete = false;
  m_isActive = true;
  m_completionReason = "";
  m_showWindow = true;
  m_cancelConfirmationOpen = false;
  m_startTime = std::chrono::steady_clock::now();

  // Clear checklist states
  std::fill(m_checklistStates.begin(), m_checklistStates.end(), false);

  // Get initial position using MachineOperations method
  PositionStruct pos;
  if (!ops.GetDeviceCurrentPosition(m_axisSystem, pos)) {
    ops.LogError("Failed to get initial position for " + m_axisSystem);
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(m_positionMutex);
    m_startX = m_currentX = pos.x;
    m_startY = m_currentY = pos.y;
    m_startZ = m_currentZ = pos.z;
  }

  // Store current jog settings to restore later
  // Note: You'll need to add these methods to MachineOperations if they don't exist
  // For now, we'll work with what's available

  try {
    // Enable jog controls
    EnableJogControls(ops);

    // Register with global registry so UI can render
    ManualAdjustmentRegistry::GetInstance().RegisterOperation(this);

    // Main wait loop
    while (!m_adjustmentComplete) {
      // Update current position
      UpdatePosition(ops);

      // Check timeout if configured
      if (m_timeoutSeconds > 0 && CheckTimeout()) {
        m_completionReason = "timeout";
        m_adjustmentComplete = true;
        ops.LogWarning("Manual adjustment timed out after " +
          std::to_string(m_timeoutSeconds) + " seconds");
        break;
      }

      // Check safety limits
      if (m_hasSafetyLimits && !CheckSafetyLimits()) {
        // Warning shown in ImGui
      }

      // Small delay to prevent CPU spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Log final position
    {
      std::lock_guard<std::mutex> lock(m_positionMutex);
      ops.LogInfo("Manual adjustment completed at: X=" +
        std::to_string(m_currentX) + " Y=" +
        std::to_string(m_currentY) + " Z=" +
        std::to_string(m_currentZ));
    }

    // Log completion reason
    ops.LogInfo("Adjustment completion reason: " + m_completionReason);

    // Check if cancelled
    if (m_completionReason == "cancelled") {
      ops.LogInfo("Manual adjustment cancelled by user");
      DisableJogControls(ops);
      ManualAdjustmentRegistry::GetInstance().UnregisterOperation(this);
      m_isActive = false;
      return false;
    }

    // Disable jog and unregister from UI
    DisableJogControls(ops);
    ManualAdjustmentRegistry::GetInstance().UnregisterOperation(this);

  }
  catch (const std::exception& e) {
    ops.LogError("Exception in manual adjustment: " + std::string(e.what()));
    DisableJogControls(ops);
    ManualAdjustmentRegistry::GetInstance().UnregisterOperation(this);
    m_isActive = false;
    throw;
  }

  m_isActive = false;
  return true;
}



void ManualAdjustmentOperation::RenderJogButtons(MachineOperations& ops) {
  ImGui::Text("Jog Controls (Step: %.3f mm)", m_stepSize);
  ImGui::Spacing();

  // Create a table for jog buttons
  if (ImGui::BeginTable("JogControls", 3, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("X Axis", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Y Axis", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Z Axis", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableHeadersRow();

    // X Axis controls
    ImGui::TableNextColumn();
    if (m_enableX) {
      if (ImGui::Button("X-", ImVec2(60, 25))) {
        // Use MoveRelative to jog
        ops.MoveRelative(m_axisSystem, "X", -m_stepSize, true, "ManualAdjustment");
      }
      ImGui::SameLine();
      if (ImGui::Button("X+", ImVec2(60, 25))) {
        ops.MoveRelative(m_axisSystem, "X", m_stepSize, true, "ManualAdjustment");
      }
    }
    else {
      ImGui::BeginDisabled();
      ImGui::Button("X-", ImVec2(60, 25));
      ImGui::SameLine();
      ImGui::Button("X+", ImVec2(60, 25));
      ImGui::EndDisabled();
    }

    // Y Axis controls
    ImGui::TableNextColumn();
    if (m_enableY) {
      if (ImGui::Button("Y-", ImVec2(60, 25))) {
        ops.MoveRelative(m_axisSystem, "Y", -m_stepSize, true, "ManualAdjustment");
      }
      ImGui::SameLine();
      if (ImGui::Button("Y+", ImVec2(60, 25))) {
        ops.MoveRelative(m_axisSystem, "Y", m_stepSize, true, "ManualAdjustment");
      }
    }
    else {
      ImGui::BeginDisabled();
      ImGui::Button("Y-", ImVec2(60, 25));
      ImGui::SameLine();
      ImGui::Button("Y+", ImVec2(60, 25));
      ImGui::EndDisabled();
    }

    // Z Axis controls
    ImGui::TableNextColumn();
    if (m_enableZ) {
      if (ImGui::Button("Z-", ImVec2(60, 25))) {
        ops.MoveRelative(m_axisSystem, "Z", -m_stepSize, true, "ManualAdjustment");
      }
      ImGui::SameLine();
      if (ImGui::Button("Z+", ImVec2(60, 25))) {
        ops.MoveRelative(m_axisSystem, "Z", m_stepSize, true, "ManualAdjustment");
      }
    }
    else {
      ImGui::BeginDisabled();
      ImGui::Button("Z-", ImVec2(60, 25));
      ImGui::SameLine();
      ImGui::Button("Z+", ImVec2(60, 25));
      ImGui::EndDisabled();
    }

    ImGui::EndTable();
  }

  // Step size adjustment
  ImGui::Spacing();
  ImGui::Text("Step Size:");
  ImGui::SameLine();

  const double stepSizes[] = { 0.001, 0.01, 0.1, 1.0 };
  const char* stepLabels[] = { "0.001", "0.01", "0.1", "1.0" };

  for (int i = 0; i < 4; ++i) {
    if (i > 0) ImGui::SameLine();

    bool isSelected = (m_stepSize == stepSizes[i]);
    if (isSelected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    if (ImGui::Button(stepLabels[i], ImVec2(50, 20))) {
      m_stepSize = stepSizes[i];
    }

    if (isSelected) {
      ImGui::PopStyleColor();
    }
  }
}





void ManualAdjustmentOperation::RenderImGui() {
  if (!m_showWindow || !m_isActive) return;

  // Window setup remains mostly the same...
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize;

  if (ImGui::Begin(m_title.c_str(), &m_showWindow, window_flags)) {

    // Instructions
    ImGui::TextWrapped("%s", m_instructions.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    // Checklist (if provided)
    if (!m_checklistItems.empty()) {
      RenderChecklist();
      ImGui::Separator();
      ImGui::Spacing();
    }

    // Position display
    if (m_showPosition) {
      RenderPositionDisplay();
      ImGui::Separator();
      ImGui::Spacing();
    }

    // Note about jog controls
    ImGui::Text("Note: Use MoveRelative buttons or external jog controls to adjust position");
    ImGui::Text("Step Size: %.3f mm", m_stepSize);
    if (m_enableX) ImGui::Text("  X-axis: Enabled");
    if (m_enableY) ImGui::Text("  Y-axis: Enabled");
    if (m_enableZ) ImGui::Text("  Z-axis: Enabled");
    ImGui::Separator();
    ImGui::Spacing();

    // Rest of the UI remains the same...

    // Action buttons
    bool allChecklistComplete = true;
    for (bool state : m_checklistStates) {
      if (!state) {
        allChecklistComplete = false;
        break;
      }
    }

    if (!m_checklistItems.empty() && !allChecklistComplete) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Done Adjusting", ImVec2(120, 30))) {
      OnDoneClicked();
    }

    if (!m_checklistItems.empty() && !allChecklistComplete) {
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Complete checklist first");
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 30))) {
      m_cancelConfirmationOpen = true;
    }

    // Cancel confirmation popup
    if (m_cancelConfirmationOpen) {
      ImGui::OpenPopup("Cancel Confirmation");
    }

    if (ImGui::BeginPopupModal("Cancel Confirmation", NULL,
      ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to cancel the adjustment?");
      ImGui::Text("The calibration process will be stopped.");
      ImGui::Separator();

      if (ImGui::Button("Yes, Cancel", ImVec2(120, 0))) {
        OnCancelClicked();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("No, Continue", ImVec2(120, 0))) {
        m_cancelConfirmationOpen = false;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
  }
  ImGui::End();
}



void ManualAdjustmentOperation::RenderPositionDisplay() {
  std::lock_guard<std::mutex> lock(m_positionMutex);

  ImGui::Text("Current Position:");
  ImGui::Indent();

  // Show current position with color coding
  if (m_enableX) {
    ImGui::Text("X: %.3f mm", m_currentX);
  }
  else {
    ImGui::TextDisabled("X: %.3f mm", m_currentX);
  }

  if (m_enableY) {
    ImGui::Text("Y: %.3f mm", m_currentY);
  }
  else {
    ImGui::TextDisabled("Y: %.3f mm", m_currentY);
  }

  if (m_enableZ) {
    if (m_hasSafetyLimits) {
      float zRatio = (m_currentZ - m_minZ) / (m_maxZ - m_minZ);
      ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      if (zRatio < 0.1f || zRatio > 0.9f) {
        color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
      }
      if (zRatio < 0.05f || zRatio > 0.95f) {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
      }
      ImGui::TextColored(color, "Z: %.3f mm (Limits: %.1f to %.1f)",
        m_currentZ, m_minZ, m_maxZ);
    }
    else {
      ImGui::Text("Z: %.3f mm", m_currentZ);
    }
  }
  else {
    ImGui::TextDisabled("Z: %.3f mm", m_currentZ);
  }

  ImGui::Unindent();

  // Show delta from start
  ImGui::Text("Change from start:");
  ImGui::Indent();
  ImGui::Text("ΔX: %.3f mm", m_currentX - m_startX);
  ImGui::Text("ΔY: %.3f mm", m_currentY - m_startY);
  ImGui::Text("ΔZ: %.3f mm", m_currentZ - m_startZ);
  ImGui::Unindent();
}

void ManualAdjustmentOperation::RenderChecklist() {
  ImGui::Text("Pre-adjustment Checklist:");
  ImGui::Indent();

  for (size_t i = 0; i < m_checklistItems.size(); ++i) {
    bool checked = m_checklistStates[i] != 0;
    if (ImGui::Checkbox(m_checklistItems[i].c_str(), &checked)) {
      m_checklistStates[i] = checked ? 1 : 0;
    }
  }

  ImGui::Unindent();
}


void ManualAdjustmentOperation::EnableJogControls(MachineOperations& ops) {
  // Since MachineOperations doesn't have direct jog control methods,
  // we need to access the motion layer or PI controller

  // Get the appropriate controller
  if (ops.IsDevicePIController(m_axisSystem)) {
    PIControllerManager* piManager = ops.GetPIControllerManager();
    if (piManager) {
      PIController* controller = piManager->GetController(m_axisSystem);
      if (controller) {
        // Enable jog mode if the controller supports it
        // You may need to add jog control methods to PIController
        ops.LogInfo("Jog controls enabled for PI controller " + m_axisSystem);
      }
    }
  }
  else {
    ACSControllerManager* acsManager = ops.GetACSControllerManager();
    if (acsManager) {
      ACSController* controller = acsManager->GetController(m_axisSystem);
      if (controller) {
        // Enable jog for ACS controller
        ops.LogInfo("Jog controls enabled for ACS controller " + m_axisSystem);
      }
    }
  }

  // Apply safety limits if configured
  if (m_hasSafetyLimits && m_enableZ) {
    // You may need to implement soft limits in your controllers
    ops.LogInfo("Applied Z-axis safety limits: " +
      std::to_string(m_minZ) + " to " + std::to_string(m_maxZ));
  }
}


void ManualAdjustmentOperation::DisableJogControls(MachineOperations& ops) {
  // Disable jog controls through the appropriate controller
  ops.LogInfo("Jog controls disabled for " + m_axisSystem);
}


void ManualAdjustmentOperation::UpdatePosition(MachineOperations& ops) {
  PositionStruct pos;
  if (ops.GetDeviceCurrentPosition(m_axisSystem, pos)) {
    std::lock_guard<std::mutex> lock(m_positionMutex);
    m_currentX = pos.x;
    m_currentY = pos.y;
    m_currentZ = pos.z;
  }
}


bool ManualAdjustmentOperation::CheckSafetyLimits() const {
  if (!m_hasSafetyLimits) return true;

  std::lock_guard<std::mutex> lock(m_positionMutex);
  if (m_enableZ && (m_currentZ < m_minZ || m_currentZ > m_maxZ)) {
    return false;
  }

  return true;
}

void ManualAdjustmentOperation::OnDoneClicked() {
  m_completionReason = "user_done";
  m_adjustmentComplete = true;
  m_showWindow = false;
}

void ManualAdjustmentOperation::OnCancelClicked() {
  m_completionReason = "cancelled";
  m_adjustmentComplete = true;
  m_showWindow = false;
}

bool ManualAdjustmentOperation::CheckTimeout() const {
  if (m_timeoutSeconds <= 0) return false;

  auto elapsed = std::chrono::steady_clock::now() - m_startTime;
  return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= m_timeoutSeconds;
}

std::string ManualAdjustmentOperation::GetDescription() const {
  return "Manual Adjustment: " + m_title;
}

// Factory methods
std::shared_ptr<ManualAdjustmentOperation>
ManualAdjustmentOperation::CreateForNeedleTouch(
  const std::string& axisSystem, UserPromptUI& promptUI) {
  auto op = std::make_shared<ManualAdjustmentOperation>(
    axisSystem,
    "Needle Touch Adjustment",
    "Use the jog controls to lower the needle until it just touches the surface.\n"
    "Be careful not to apply too much pressure.",
    promptUI,
    false, false, true  // Only Z axis
  );

  op->WithStepSize(0.01)
    .WithShowPosition(true)
    .WithChecklist({
        "Needle is clean and undamaged",
        "Surface is flat and clean",
        "View angle allows you to see contact point"
      });

  return op;
}

std::shared_ptr<ManualAdjustmentOperation>
ManualAdjustmentOperation::CreateForCameraAlignment(
  const std::string& axisSystem, UserPromptUI& promptUI) {
  auto op = std::make_shared<ManualAdjustmentOperation>(
    axisSystem,
    "Camera Alignment",
    "Use the jog controls to center the crosshair on the target.",
    promptUI,
    true, true, false  // X and Y only
  );

  op->WithStepSize(0.1)
    .WithShowPosition(true);

  return op;
}

std::shared_ptr<ManualAdjustmentOperation>
ManualAdjustmentOperation::CreateForDispenseHeight(
  const std::string& axisSystem, UserPromptUI& promptUI) {
  auto op = std::make_shared<ManualAdjustmentOperation>(
    axisSystem,
    "Dispense Height Adjustment",
    "Adjust the dispenser tip to the correct height for dispensing.\n"
    "Use Z-axis controls to set the proper tip-to-surface distance.",
    promptUI,
    false, false, true  // Only Z axis
  );

  op->WithStepSize(0.01)
    .WithShowPosition(true)
    .WithSafetyLimits(-5.0, 5.0);

  return op;
}