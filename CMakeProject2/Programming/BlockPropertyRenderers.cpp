// BlockPropertyRenderers.cpp
#include "BlockPropertyRenderers.h"
#include "MachineBlockUI.h" // For MachineBlock and BlockType definitions
#include <iostream>

// ═══════════════════════════════════════════════════════════════════════════════════════
// BASE CLASS HELPER METHODS
// ═══════════════════════════════════════════════════════════════════════════════════════

void BlockPropertyRenderer::RenderStandardParameters(MachineBlock* block) {
  for (auto& param : block->parameters) {
    RenderParameter(param);
  }
}

void BlockPropertyRenderer::RenderParameter(BlockParameter& param) {
  ImGui::PushID(param.name.c_str());
  ImGui::Text("%s:", param.name.c_str());

  if (param.type == "bool") {
    bool value = param.value == "true";
    if (ImGui::Checkbox("##value", &value)) {
      param.value = value ? "true" : "false";
    }
  }
  else {
    char buffer[256];
    strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
      param.value = std::string(buffer);
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", param.description.c_str());
  }

  ImGui::Spacing();
  ImGui::PopID();
}

void BlockPropertyRenderer::RenderSuccessPopup(const std::string& message) {
  if (ImGui::BeginPopupModal("Save Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("SUCCESS: %s", message.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void BlockPropertyRenderer::RenderErrorPopup(const std::string& message) {
  if (ImGui::BeginPopupModal("Save Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    ImGui::Text("ERROR: %s", message.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// START BLOCK RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void StartBlockRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("START Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("This is the starting point of your program.");
  ImGui::TextWrapped("Every program must have exactly one START block.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void StartBlockRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Start Actions:");
  ImGui::TextWrapped("No additional actions available for START blocks.");
}

void StartBlockRenderer::RenderValidation(MachineBlock* block) {
  // START blocks are generally always valid
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("START block is valid.");
  ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// END BLOCK RENDERER  
// ═══════════════════════════════════════════════════════════════════════════════════════

void EndBlockRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("END Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.0f, 0.0f, 1.0f)); // Red
  ImGui::TextWrapped("This marks the end of your program execution.");
  ImGui::TextWrapped("Programs should have at least one END block.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void EndBlockRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("End Actions:");
  ImGui::TextWrapped("No additional actions available for END blocks.");
}

void EndBlockRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("END block is valid.");
  ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// MOVE_NODE RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void MoveNodeRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("MOVE_NODE Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.6f, 1.0f, 1.0f)); // Blue
  ImGui::TextWrapped("Moves a device to a specific node in the motion graph.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void MoveNodeRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Position Management:");

  auto [deviceName, graphName, nodeId] = ExtractMoveNodeParameters(block);

  if (!deviceName.empty() && !graphName.empty() && !nodeId.empty()) {
    RenderPositionInfo(deviceName, graphName, nodeId);
    RenderSavePositionButton(deviceName, graphName, nodeId, machineOps);
    RenderValidationWarnings(deviceName, graphName, nodeId);
    RenderHelperText();
  }
}

void MoveNodeRenderer::RenderValidation(MachineBlock* block) {
  auto [deviceName, graphName, nodeId] = ExtractMoveNodeParameters(block);

  if (deviceName.empty() || graphName.empty() || nodeId.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Missing required parameters for MOVE_NODE");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("MOVE_NODE parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string, std::string> MoveNodeRenderer::ExtractMoveNodeParameters(MachineBlock* block) {
  std::string deviceName, graphName, nodeId;
  for (const auto& param : block->parameters) {
    if (param.name == "device_name") deviceName = param.value;
    else if (param.name == "graph_name") graphName = param.value;
    else if (param.name == "node_id") nodeId = param.value;
  }
  return { deviceName, graphName, nodeId };
}

void MoveNodeRenderer::RenderPositionInfo(const std::string& deviceName, const std::string& graphName, const std::string& nodeId) {
  ImGui::TextWrapped("Device: %s", deviceName.c_str());
  ImGui::TextWrapped("Graph: %s", graphName.c_str());
  ImGui::TextWrapped("Target Node: %s", nodeId.c_str());
  ImGui::Spacing();
}

void MoveNodeRenderer::RenderSavePositionButton(const std::string& deviceName, const std::string& graphName,
  const std::string& nodeId, MachineOperations* machineOps) {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Green
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

  std::string buttonText = "Save Current Position (Node: " + nodeId + ")";

  if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 0))) {
    HandleSavePosition(deviceName, graphName, nodeId, machineOps);
  }

  ImGui::PopStyleColor(3);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Save current position to the position that node '%s' references", nodeId.c_str());
  }
}

void MoveNodeRenderer::HandleSavePosition(const std::string& deviceName, const std::string& graphName,
  const std::string& nodeId, MachineOperations* machineOps) {
  if (machineOps) {
    bool success = machineOps->SaveCurrentPositionForNode(deviceName, graphName, nodeId);

    if (success) {
      printf("[SUCCESS] Position saved for node: %s (%s)\n", nodeId.c_str(), deviceName.c_str());

      printf("[INFO] Reloading motion configuration...\n");
      bool reloadSuccess = machineOps->ReloadMotionConfig();

      if (reloadSuccess) {
        printf("[SUCCESS] Motion configuration reloaded successfully\n");
      }
      else {
        printf("[WARNING] Position saved but failed to reload configuration\n");
      }

      ImGui::OpenPopup("Save Success");
    }
    else {
      printf("[ERROR] Failed to save position for node: %s (%s)\n", nodeId.c_str(), deviceName.c_str());
      ImGui::OpenPopup("Save Error");
    }
  }
  else {
    printf("[ERROR] MachineOperations not available for saving position\n");
    ImGui::OpenPopup("Save Error");
  }

  // Render popups
  RenderSuccessPopup("Position saved and configuration reloaded!");
  RenderErrorPopup("Failed to save position. Check console for details.");
}

void MoveNodeRenderer::RenderValidationWarnings(const std::string& deviceName, const std::string& graphName, const std::string& nodeId) {
  if (deviceName.empty() || graphName.empty() || nodeId.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: All parameters must be set to save position");
    ImGui::PopStyleColor();
  }
}

void MoveNodeRenderer::RenderHelperText() {
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray
  ImGui::TextWrapped("This will save the current physical position to the position name that this node references in the motion graph.");
  ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// WAIT RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void WaitRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("WAIT Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Orange
  ImGui::TextWrapped("Pauses execution for a specified amount of time.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void WaitRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Wait Actions:");
  ImGui::TextWrapped("No additional actions available for WAIT blocks.");
  ImGui::TextWrapped("Consider adding a 'Test Wait' button in future versions.");
}

void WaitRenderer::RenderValidation(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "milliseconds") {
      ValidateWaitTime(param.value);
      break;
    }
  }
}

void WaitRenderer::ValidateWaitTime(const std::string& waitTimeStr) {
  try {
    int waitTime = std::stoi(waitTimeStr);
    if (waitTime < 0) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
      ImGui::TextWrapped("WARNING: Wait time cannot be negative");
      ImGui::PopStyleColor();
    }
    else if (waitTime > 60000) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
      ImGui::TextWrapped("WARNING: Wait time over 1 minute (%d ms)", waitTime);
      ImGui::PopStyleColor();
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
      ImGui::TextWrapped("Wait time is valid (%d ms)", waitTime);
      ImGui::PopStyleColor();
    }
  }
  catch (...) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    ImGui::TextWrapped("ERROR: Invalid wait time format");
    ImGui::PopStyleColor();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// SET_OUTPUT RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void SetOutputRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("SET_OUTPUT Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.5f, 1.0f)); // Lime green
  ImGui::TextWrapped("Sets a digital output pin to ON state.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void SetOutputRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Output Actions:");

  auto [deviceName, pin, state, delay] = ExtractOutputParameters(block);

  if (!deviceName.empty() && !pin.empty()) {
    RenderTestButton(deviceName, pin, state, delay, machineOps);
  }
  else {
    ImGui::TextWrapped("Set device name and pin to enable test functionality.");
  }
}

void SetOutputRenderer::RenderValidation(MachineBlock* block) {
  auto [deviceName, pin, state, delay] = ExtractOutputParameters(block);

  if (deviceName.empty() || pin.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Device name and pin must be specified");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("SET_OUTPUT parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string, std::string, std::string> SetOutputRenderer::ExtractOutputParameters(MachineBlock* block) {
  std::string deviceName, pin, state, delay;
  for (const auto& param : block->parameters) {
    if (param.name == "device_name") deviceName = param.value;
    else if (param.name == "pin") pin = param.value;
    else if (param.name == "state") state = param.value;
    else if (param.name == "delay_ms") delay = param.value;
  }
  return { deviceName, pin, state, delay };
}

void SetOutputRenderer::RenderTestButton(const std::string& deviceName, const std::string& pin,
  const std::string& state, const std::string& delay, MachineOperations* machineOps) {
  if (ImGui::Button("Test Output", ImVec2(-1, 0))) {
    printf("[TEST] Would set %s pin %s to %s (delay: %s ms)\n",
      deviceName.c_str(), pin.c_str(), state.c_str(), delay.c_str());
    // Future: Could implement actual test functionality here
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test this output configuration (simulation only)");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// CLEAR_OUTPUT RENDERER  
// ═══════════════════════════════════════════════════════════════════════════════════════

void ClearOutputRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("CLEAR_OUTPUT Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
  ImGui::TextWrapped("Clears (turns OFF) a digital output pin.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void ClearOutputRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Clear Output Actions:");

  auto [deviceName, pin, delay] = ExtractClearOutputParameters(block);

  if (!deviceName.empty() && !pin.empty()) {
    RenderTestButton(deviceName, pin, delay, machineOps);
  }
  else {
    ImGui::TextWrapped("Set device name and pin to enable test functionality.");
  }
}

void ClearOutputRenderer::RenderValidation(MachineBlock* block) {
  auto [deviceName, pin, delay] = ExtractClearOutputParameters(block);

  if (deviceName.empty() || pin.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Device name and pin must be specified");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("CLEAR_OUTPUT parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string, std::string> ClearOutputRenderer::ExtractClearOutputParameters(MachineBlock* block) {
  std::string deviceName, pin, delay;
  for (const auto& param : block->parameters) {
    if (param.name == "device_name") deviceName = param.value;
    else if (param.name == "pin") pin = param.value;
    else if (param.name == "delay_ms") delay = param.value;
  }
  return { deviceName, pin, delay };
}

void ClearOutputRenderer::RenderTestButton(const std::string& deviceName, const std::string& pin,
  const std::string& delay, MachineOperations* machineOps) {
  if (ImGui::Button("Test Clear Output", ImVec2(-1, 0))) {
    printf("[TEST] Would clear %s pin %s (delay: %s ms)\n",
      deviceName.c_str(), pin.c_str(), delay.c_str());
    // Future: Could implement actual test functionality here
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test this clear output configuration (simulation only)");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// DEFAULT RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void DefaultRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Unknown Block Type:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
  ImGui::TextWrapped("This block type is not recognized. Using default renderer.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void DefaultRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Actions:");
  ImGui::TextWrapped("No actions available for unknown block types.");
}

void DefaultRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
  ImGui::TextWrapped("WARNING: Unknown block type cannot be validated.");
  ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// RENDERER FACTORY IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════════════

std::unique_ptr<BlockPropertyRenderer> BlockRendererFactory::CreateRenderer(BlockType type) {
  switch (type) {
  case BlockType::START:
    return std::make_unique<StartBlockRenderer>();
  case BlockType::END:
    return std::make_unique<EndBlockRenderer>();
  case BlockType::MOVE_NODE:
    return std::make_unique<MoveNodeRenderer>();
  case BlockType::WAIT:
    return std::make_unique<WaitRenderer>();
  case BlockType::SET_OUTPUT:
    return std::make_unique<SetOutputRenderer>();
  case BlockType::CLEAR_OUTPUT:
    return std::make_unique<ClearOutputRenderer>();
  case BlockType::EXTEND_SLIDE:    // NEW
    return std::make_unique<ExtendSlideRenderer>();
  case BlockType::RETRACT_SLIDE:   // NEW
    return std::make_unique<RetractSlideRenderer>();


  case BlockType::SET_LASER_CURRENT:    // NEW
    return std::make_unique<SetLaserCurrentRenderer>();
  case BlockType::LASER_ON:             // NEW
    return std::make_unique<LaserOnRenderer>();
  case BlockType::LASER_OFF:            // NEW
    return std::make_unique<LaserOffRenderer>();
  case BlockType::SET_TEC_TEMPERATURE:  // NEW
    return std::make_unique<SetTECTemperatureRenderer>();
  case BlockType::TEC_ON:               // NEW
    return std::make_unique<TECOnRenderer>();
  case BlockType::TEC_OFF:              // NEW
    return std::make_unique<TECOffRenderer>();



  default:
    return std::make_unique<DefaultRenderer>();
  }
}




// Step 15: Implement ExtendSlideRenderer methods in BlockPropertyRenderers.cpp
void ExtendSlideRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("EXTEND SLIDE Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("Extends a pneumatic slide to its extended position.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void ExtendSlideRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Extend Slide Actions:");

  std::string slideName = ExtractSlideName(block);

  if (!slideName.empty()) {
    RenderTestButton(slideName, machineOps);
  }
  else {
    ImGui::TextWrapped("Set slide name to enable test functionality.");
  }
}

void ExtendSlideRenderer::RenderValidation(MachineBlock* block) {
  std::string slideName = ExtractSlideName(block);

  if (slideName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Slide name must be specified");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("EXTEND_SLIDE parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::string ExtendSlideRenderer::ExtractSlideName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "slide_name") {
      return param.value;
    }
  }
  return "";
}

void ExtendSlideRenderer::RenderTestButton(const std::string& slideName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Extend Slide", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Extending slide: %s\n", slideName.c_str());
      machineOps->ExtendSlide(slideName, true);
    }
    else {
      printf("[TEST] Would extend slide: %s\n", slideName.c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test extending slide: %s", slideName.c_str());
  }
}

// Step 16: Implement RetractSlideRenderer methods in BlockPropertyRenderers.cpp
void RetractSlideRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("RETRACT SLIDE Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Light red
  ImGui::TextWrapped("Retracts a pneumatic slide to its retracted position.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void RetractSlideRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Retract Slide Actions:");

  std::string slideName = ExtractSlideName(block);

  if (!slideName.empty()) {
    RenderTestButton(slideName, machineOps);
  }
  else {
    ImGui::TextWrapped("Set slide name to enable test functionality.");
  }
}

void RetractSlideRenderer::RenderValidation(MachineBlock* block) {
  std::string slideName = ExtractSlideName(block);

  if (slideName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Slide name must be specified");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("RETRACT_SLIDE parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::string RetractSlideRenderer::ExtractSlideName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "slide_name") {
      return param.value;
    }
  }
  return "";
}

void RetractSlideRenderer::RenderTestButton(const std::string& slideName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Retract Slide", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Retracting slide: %s\n", slideName.c_str());
      machineOps->RetractSlide(slideName, true);
    }
    else {
      printf("[TEST] Would retract slide: %s\n", slideName.c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test retracting slide: %s", slideName.c_str());
  }
}


// BlockPropertyRenderers.cpp - Laser and TEC Control Block Renderers Implementation

// ═══════════════════════════════════════════════════════════════════════════════════════
// SET LASER CURRENT RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void SetLaserCurrentRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("SET LASER CURRENT Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f)); // Orange
  ImGui::TextWrapped("Sets the laser current in milliamps (mA).");
  ImGui::TextWrapped("Typical range: 0.050 - 0.300 mA");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  // Show current value prominently
  auto [current, laserName] = ExtractLaserCurrentParameters(block);
  if (!current.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Current Setting: %s mA", current.c_str());
    if (!laserName.empty()) {
      ImGui::Text("Laser: %s", laserName.c_str());
    }
    ImGui::PopStyleColor();
  }
}

void SetLaserCurrentRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Laser Current Actions:");

  auto [current, laserName] = ExtractLaserCurrentParameters(block);

  if (!current.empty()) {
    RenderTestButton(current, laserName, machineOps);
  }
  else {
    ImGui::TextWrapped("Set laser current to enable test functionality.");
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("⚠️  Safety: Ensure TEC is on and stable before setting high current!");
  ImGui::PopStyleColor();
}

void SetLaserCurrentRenderer::RenderValidation(MachineBlock* block) {
  auto [current, laserName] = ExtractLaserCurrentParameters(block);

  if (current.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Laser current must be specified");
    ImGui::PopStyleColor();
    return;
  }

  try {
    float currentValue = std::stof(current);

    if (currentValue < 0.0f) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
      ImGui::TextWrapped("ERROR: Current cannot be negative");
      ImGui::PopStyleColor();
    }
    else if (currentValue > 0.500f) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
      ImGui::TextWrapped("WARNING: High current (>0.500 mA) - Use with caution!");
      ImGui::PopStyleColor();
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
      ImGui::TextWrapped("SET_LASER_CURRENT parameters are valid.");
      ImGui::PopStyleColor();
    }
  }
  catch (const std::exception&) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    ImGui::TextWrapped("ERROR: Invalid current value format");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string> SetLaserCurrentRenderer::ExtractLaserCurrentParameters(MachineBlock* block) {
  std::string current, laserName;
  for (const auto& param : block->parameters) {
    if (param.name == "current_ma") current = param.value;
    else if (param.name == "laser_name") laserName = param.value;
  }
  return { current, laserName };
}

void SetLaserCurrentRenderer::RenderTestButton(const std::string& current, const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Set Laser Current", ImVec2(-1, 0))) {
    if (machineOps) {
      float currentValue = std::stof(current);
      printf("[TEST] Setting laser current: %s mA%s\n",
        current.c_str(),
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->SetLaserCurrent(currentValue);
    }
    else {
      printf("[TEST] Would set laser current: %s mA%s\n",
        current.c_str(),
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test setting laser current to %s mA", current.c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// LASER ON RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void LaserOnRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("LASER ON Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
  ImGui::TextWrapped("Turns the laser ON.");
  ImGui::TextWrapped("⚠️ Ensure current is set and TEC is stable first!");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  std::string laserName = ExtractLaserName(block);
  if (!laserName.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Target Laser: %s", laserName.c_str());
    ImGui::PopStyleColor();
  }
}

void LaserOnRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Laser Control Actions:");

  std::string laserName = ExtractLaserName(block);
  RenderTestButton(laserName, machineOps);

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
  ImGui::TextWrapped("🚨 DANGER: Laser radiation when ON!");
  ImGui::PopStyleColor();
}

void LaserOnRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("LASER_ON block is ready to execute.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("💡 Tip: Use SET_LASER_CURRENT before LASER_ON");
  ImGui::PopStyleColor();
}

std::string LaserOnRenderer::ExtractLaserName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "laser_name") {
      return param.value;
    }
  }
  return "";
}

void LaserOnRenderer::RenderTestButton(const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Laser ON", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Turning laser ON%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->LaserOn(laserName);
    }
    else {
      printf("[TEST] Would turn laser ON%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test turning laser ON%s",
      laserName.empty() ? "" : (" for " + laserName).c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// LASER OFF RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void LaserOffRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("LASER OFF Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
  ImGui::TextWrapped("Turns the laser OFF safely.");
  ImGui::TextWrapped("✅ Safe operation - stops laser emission.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  std::string laserName = ExtractLaserName(block);
  if (!laserName.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Target Laser: %s", laserName.c_str());
    ImGui::PopStyleColor();
  }
}

void LaserOffRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Laser Control Actions:");

  std::string laserName = ExtractLaserName(block);
  RenderTestButton(laserName, machineOps);

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("✅ Safe operation - turns laser OFF");
  ImGui::PopStyleColor();
}

void LaserOffRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("LASER_OFF block is ready to execute.");
  ImGui::PopStyleColor();
}

std::string LaserOffRenderer::ExtractLaserName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "laser_name") {
      return param.value;
    }
  }
  return "";
}

void LaserOffRenderer::RenderTestButton(const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Laser OFF", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Turning laser OFF%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->LaserOff(laserName);
    }
    else {
      printf("[TEST] Would turn laser OFF%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test turning laser OFF%s",
      laserName.empty() ? "" : (" for " + laserName).c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// SET TEC TEMPERATURE RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void SetTECTemperatureRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("SET TEC TEMPERATURE Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.6f, 1.0f, 1.0f)); // Blue
  ImGui::TextWrapped("Sets the TEC (Thermoelectric Cooler) target temperature.");
  ImGui::TextWrapped("Typical range: 15°C - 35°C");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  auto [temperature, laserName] = ExtractTECTemperatureParameters(block);
  if (!temperature.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Target Temperature: %s°C", temperature.c_str());
    if (!laserName.empty()) {
      ImGui::Text("Laser/TEC: %s", laserName.c_str());
    }
    ImGui::PopStyleColor();
  }
}

void SetTECTemperatureRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("TEC Temperature Actions:");

  auto [temperature, laserName] = ExtractTECTemperatureParameters(block);

  if (!temperature.empty()) {
    RenderTestButton(temperature, laserName, machineOps);
  }
  else {
    ImGui::TextWrapped("Set target temperature to enable test functionality.");
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("💡 Note: Temperature stabilization may take time");
  ImGui::PopStyleColor();
}

void SetTECTemperatureRenderer::RenderValidation(MachineBlock* block) {
  auto [temperature, laserName] = ExtractTECTemperatureParameters(block);

  if (temperature.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Target temperature must be specified");
    ImGui::PopStyleColor();
    return;
  }

  try {
    float tempValue = std::stof(temperature);

    if (tempValue < 10.0f || tempValue > 50.0f) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
      ImGui::TextWrapped("WARNING: Temperature outside typical range (10-50°C)");
      ImGui::PopStyleColor();
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
      ImGui::TextWrapped("SET_TEC_TEMPERATURE parameters are valid.");
      ImGui::PopStyleColor();
    }
  }
  catch (const std::exception&) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    ImGui::TextWrapped("ERROR: Invalid temperature value format");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string> SetTECTemperatureRenderer::ExtractTECTemperatureParameters(MachineBlock* block) {
  std::string temperature, laserName;
  for (const auto& param : block->parameters) {
    if (param.name == "temperature_c") temperature = param.value;
    else if (param.name == "laser_name") laserName = param.value;
  }
  return { temperature, laserName };
}

void SetTECTemperatureRenderer::RenderTestButton(const std::string& temperature, const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test Set TEC Temperature", ImVec2(-1, 0))) {
    if (machineOps) {
      float tempValue = std::stof(temperature);
      printf("[TEST] Setting TEC temperature: %s°C%s\n",
        temperature.c_str(),
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->SetTECTemperature(tempValue);
    }
    else {
      printf("[TEST] Would set TEC temperature: %s°C%s\n",
        temperature.c_str(),
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test setting TEC temperature to %s°C", temperature.c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// TEC ON RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void TECOnRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("TEC ON Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f)); // Light blue
  ImGui::TextWrapped("Turns the TEC (Thermoelectric Cooler) ON.");
  ImGui::TextWrapped("✅ Required before laser operation for temperature stability.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  std::string laserName = ExtractLaserName(block);
  if (!laserName.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Target Laser/TEC: %s", laserName.c_str());
    ImGui::PopStyleColor();
  }
}

void TECOnRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("TEC Control Actions:");

  std::string laserName = ExtractLaserName(block);
  RenderTestButton(laserName, machineOps);

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("💡 Best Practice: Turn TEC ON → Set Temperature → Wait for Stability");
  ImGui::PopStyleColor();
}

void TECOnRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("TEC_ON block is ready to execute.");
  ImGui::PopStyleColor();
}

std::string TECOnRenderer::ExtractLaserName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "laser_name") {
      return param.value;
    }
  }
  return "";
}

void TECOnRenderer::RenderTestButton(const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test TEC ON", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Turning TEC ON%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->TECOn(laserName);
    }
    else {
      printf("[TEST] Would turn TEC ON%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test turning TEC ON%s",
      laserName.empty() ? "" : (" for " + laserName).c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// TEC OFF RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void TECOffRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("TEC OFF Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f)); // Dark blue
  ImGui::TextWrapped("Turns the TEC (Thermoelectric Cooler) OFF.");
  ImGui::TextWrapped("⚠️ Use after turning laser OFF to save power.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  std::string laserName = ExtractLaserName(block);
  if (!laserName.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Target Laser/TEC: %s", laserName.c_str());
    ImGui::PopStyleColor();
  }
}

void TECOffRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("TEC Control Actions:");

  std::string laserName = ExtractLaserName(block);
  RenderTestButton(laserName, machineOps);

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("💡 Recommended: Turn Laser OFF before TEC OFF");
  ImGui::PopStyleColor();
}

void TECOffRenderer::RenderValidation(MachineBlock* block) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  ImGui::TextWrapped("TEC_OFF block is ready to execute.");
  ImGui::PopStyleColor();
}

std::string TECOffRenderer::ExtractLaserName(MachineBlock* block) {
  for (const auto& param : block->parameters) {
    if (param.name == "laser_name") {
      return param.value;
    }
  }
  return "";
}

void TECOffRenderer::RenderTestButton(const std::string& laserName, MachineOperations* machineOps) {
  if (ImGui::Button("Test TEC OFF", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Turning TEC OFF%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
      machineOps->TECOff(laserName);
    }
    else {
      printf("[TEST] Would turn TEC OFF%s\n",
        laserName.empty() ? "" : (" for " + laserName).c_str());
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test turning TEC OFF%s",
      laserName.empty() ? "" : (" for " + laserName).c_str());
  }
}