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

  bool paramChanged = false; // Track if parameter was modified

  if (param.type == "bool") {
    bool value = param.value == "true";
    if (ImGui::Checkbox("##value", &value)) {
      param.value = value ? "true" : "false";
      paramChanged = true;
    }
  }
  else {
    char buffer[256];
    strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
      param.value = std::string(buffer);
      paramChanged = true;
    }
  }

  // NEW: If parameter changed, update the block label
  if (paramChanged) {
    // We need access to the parent block to update its label
    // This requires passing the block pointer to RenderParameter
    // For now, we'll handle this in the main UI update loop
    printf("DEBUG: Parameter %s changed to %s\n", param.name.c_str(), param.value.c_str());
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

  case BlockType::PROMPT:  // NEW
    return std::make_unique<PromptRenderer>();
    // ADD THESE MISSING CASES:
  case BlockType::MOVE_TO_POSITION:
    return std::make_unique<MoveToPositionRenderer>();
  case BlockType::MOVE_RELATIVE_AXIS:
    return std::make_unique<MoveRelativeAxisRenderer>();

    // NEW: Keithley renderer cases
  case BlockType::KEITHLEY_RESET:
    return std::make_unique<KeithleyResetRenderer>();
  case BlockType::KEITHLEY_SET_OUTPUT:
    return std::make_unique<KeithleySetOutputRenderer>();
  case BlockType::KEITHLEY_VOLTAGE_SOURCE:
    return std::make_unique<KeithleyVoltageSourceRenderer>();
  case BlockType::KEITHLEY_CURRENT_SOURCE:
    return std::make_unique<KeithleyCurrentSourceRenderer>();
  case BlockType::KEITHLEY_READ_VOLTAGE:
    return std::make_unique<KeithleyReadVoltageRenderer>();
  case BlockType::KEITHLEY_READ_CURRENT:
    return std::make_unique<KeithleyReadCurrentRenderer>();
  case BlockType::KEITHLEY_READ_RESISTANCE:
    return std::make_unique<KeithleyReadResistanceRenderer>();
  case BlockType::KEITHLEY_SEND_COMMAND:
    return std::make_unique<KeithleySendCommandRenderer>();
  case BlockType::SCAN_OPERATION:
    return std::make_unique<ScanOperationRenderer>();

  default:
    return std::make_unique<DefaultRenderer>();
  }
}

std::string GetParameterValue(const MachineBlock& block, const std::string& paramName) {
  for (const auto& param : block.parameters) {
    if (param.name == paramName) {
      return param.value;
    }
  }
  return "";
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
  ImGui::TextWrapped("[CAUTION] Safety: Ensure TEC is on and stable before setting high current!");
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
  ImGui::TextWrapped("[CAUTION] Ensure current is set and TEC is stable first!");
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
  ImGui::TextWrapped("[Yes] Safe operation - stops laser emission.");
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
  ImGui::TextWrapped("[Yes] Safe operation - turns laser OFF");
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
  ImGui::TextWrapped("[Yes] Required before laser operation for temperature stability.");
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
  ImGui::TextWrapped("[CAUTION] Use after turning laser OFF to save power.");
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


// Add to BlockPropertyRenderers.cpp:
void PromptRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("USER PROMPT Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Gold
  ImGui::TextWrapped("💭 Pauses program execution and waits for user confirmation.");
  ImGui::TextWrapped("[CAUTION] Program will STOP if user selects NO or CANCEL.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);
}

void PromptRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Prompt Actions:");

  auto [title, message] = ExtractPromptParameters(block);
  RenderPreviewButton(title, message);
}

void PromptRenderer::RenderValidation(MachineBlock* block) {
  auto [title, message] = ExtractPromptParameters(block);

  if (title.empty() || message.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Title and message should be specified for better user experience");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("PROMPT parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string> PromptRenderer::ExtractPromptParameters(MachineBlock* block) {
  std::string title, message;
  for (const auto& param : block->parameters) {
    if (param.name == "title") title = param.value;
    else if (param.name == "message") message = param.value;
  }
  return { title, message };
}

void PromptRenderer::RenderPreviewButton(const std::string& title, const std::string& message) {
  if (ImGui::Button("Preview Prompt", ImVec2(-1, 0))) {
    ImGui::OpenPopup("Prompt Preview");
  }

  if (ImGui::BeginPopupModal("Prompt Preview", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Title: %s", title.empty() ? "User Confirmation" : title.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("Message: %s", message.empty() ? "Do you want to continue?" : message.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Close Preview", ImVec2(-1, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Preview how the prompt will appear to users");
  }
}


void MoveToPositionRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("MOVE TO POSITION Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.7f, 1.0f, 1.0f)); // Light blue
  ImGui::TextWrapped("Moves a controller to a saved position by name.");
  ImGui::TextWrapped("💡 Use 'Save Current Position' to create named positions first.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  auto [controllerName, positionName, blocking] = ExtractMoveToPositionParameters(block);
  if (!controllerName.empty() && !positionName.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Controller: %s", controllerName.c_str());
    ImGui::Text("Target Position: %s", positionName.c_str());
    ImGui::Text("Blocking: %s", blocking ? "Yes" : "No");
    ImGui::PopStyleColor();
  }
}

void MoveToPositionRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Position Actions:");

  auto [controllerName, positionName, blocking] = ExtractMoveToPositionParameters(block);

  if (!controllerName.empty() && !positionName.empty()) {
    RenderTestButton(controllerName, positionName, blocking, machineOps);
  }
  else {
    ImGui::TextWrapped("Set controller name and position name to enable test functionality.");
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("💡 Tip: Create named positions using MOVE_NODE blocks first.");
  ImGui::PopStyleColor();
}

void MoveToPositionRenderer::RenderValidation(MachineBlock* block) {
  auto [controllerName, positionName, blocking] = ExtractMoveToPositionParameters(block);

  if (controllerName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Controller name must be specified");
    ImGui::PopStyleColor();
  }
  else if (positionName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Position name must be specified");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::TextWrapped("MOVE_TO_POSITION parameters are valid.");
    ImGui::PopStyleColor();
  }
}

std::tuple<std::string, std::string, bool> MoveToPositionRenderer::ExtractMoveToPositionParameters(MachineBlock* block) {
  std::string controllerName, positionName;
  bool blocking = true;

  for (const auto& param : block->parameters) {
    if (param.name == "controller_name") controllerName = param.value;
    else if (param.name == "position_name") positionName = param.value;
    else if (param.name == "blocking") blocking = (param.value == "true");
  }
  return { controllerName, positionName, blocking };
}

void MoveToPositionRenderer::RenderTestButton(const std::string& controllerName, const std::string& positionName,
  bool blocking, MachineOperations* machineOps) {
  if (ImGui::Button("Test Move to Position", ImVec2(-1, 0))) {
    if (machineOps) {
      printf("[TEST] Moving %s to position '%s' (blocking: %s)\n",
        controllerName.c_str(), positionName.c_str(), blocking ? "true" : "false");
      machineOps->MoveToPointName(controllerName, positionName, blocking);
    }
    else {
      printf("[TEST] Would move %s to position '%s' (blocking: %s)\n",
        controllerName.c_str(), positionName.c_str(), blocking ? "true" : "false");
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test moving %s to position '%s'", controllerName.c_str(), positionName.c_str());
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// MOVE RELATIVE AXIS RENDERER
// ═══════════════════════════════════════════════════════════════════════════════════════

void MoveRelativeAxisRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("MOVE RELATIVE AXIS Block Properties:");
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.4f, 1.0f, 1.0f)); // Light purple
  ImGui::TextWrapped("Moves a controller relative to its current position on a specific axis.");
  ImGui::TextWrapped("💡 Use positive values to move in + direction, negative for - direction.");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  RenderStandardParameters(block);

  auto [controllerName, axisName, distance, blocking] = ExtractMoveRelativeAxisParameters(block);
  if (!controllerName.empty() && !axisName.empty() && !distance.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    ImGui::Text("Controller: %s", controllerName.c_str());
    ImGui::Text("Axis: %s", axisName.c_str());
    ImGui::Text("Distance: %s mm", distance.c_str());
    ImGui::Text("Blocking: %s", blocking ? "Yes" : "No");
    ImGui::PopStyleColor();
  }
}

void MoveRelativeAxisRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Relative Movement Actions:");

  auto [controllerName, axisName, distance, blocking] = ExtractMoveRelativeAxisParameters(block);

  if (!controllerName.empty() && !axisName.empty() && !distance.empty()) {
    RenderTestButton(controllerName, axisName, distance, blocking, machineOps);
  }
  else {
    ImGui::TextWrapped("Set controller name, axis, and distance to enable test functionality.");
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
  ImGui::TextWrapped("[CAUTION] Safety: Small movements first! Start with 0.1mm to test.");
  ImGui::PopStyleColor();
}

void MoveRelativeAxisRenderer::RenderValidation(MachineBlock* block) {
  auto [controllerName, axisName, distance, blocking] = ExtractMoveRelativeAxisParameters(block);

  if (controllerName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Controller name must be specified");
    ImGui::PopStyleColor();
  }
  else if (axisName.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Axis name must be specified (X, Y, Z, U, V, W)");
    ImGui::PopStyleColor();
  }
  else if (distance.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
    ImGui::TextWrapped("WARNING: Distance must be specified");
    ImGui::PopStyleColor();
  }
  else {
    // Validate distance is a valid number
    try {
      float distValue = std::stof(distance);
      if (std::abs(distValue) > 100.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
        ImGui::TextWrapped("WARNING: Large movement (>100mm) - Use with caution!");
        ImGui::PopStyleColor();
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
        ImGui::TextWrapped("MOVE_RELATIVE_AXIS parameters are valid.");
        ImGui::PopStyleColor();
      }
    }
    catch (const std::exception&) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
      ImGui::TextWrapped("ERROR: Invalid distance format - must be a number");
      ImGui::PopStyleColor();
    }
  }
}

std::tuple<std::string, std::string, std::string, bool> MoveRelativeAxisRenderer::ExtractMoveRelativeAxisParameters(MachineBlock* block) {
  std::string controllerName, axisName, distance;
  bool blocking = true;

  for (const auto& param : block->parameters) {
    if (param.name == "controller_name") controllerName = param.value;
    else if (param.name == "axis_name") axisName = param.value;
    else if (param.name == "distance_mm") distance = param.value;
    else if (param.name == "blocking") blocking = (param.value == "true");
  }
  return { controllerName, axisName, distance, blocking };
}

void MoveRelativeAxisRenderer::RenderTestButton(const std::string& controllerName, const std::string& axisName,
  const std::string& distance, bool blocking, MachineOperations* machineOps) {
  if (ImGui::Button("Test Relative Move", ImVec2(-1, 0))) {
    if (machineOps) {
      float distValue = std::stof(distance);
      printf("[TEST] Moving %s relative on %s axis by %s mm (blocking: %s)\n",
        controllerName.c_str(), axisName.c_str(), distance.c_str(), blocking ? "true" : "false");
      machineOps->MoveRelative(controllerName, axisName, distValue, blocking);
    }
    else {
      printf("[TEST] Would move %s relative on %s axis by %s mm (blocking: %s)\n",
        controllerName.c_str(), axisName.c_str(), distance.c_str(), blocking ? "true" : "false");
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Test moving %s on %s axis by %s mm", controllerName.c_str(), axisName.c_str(), distance.c_str());
  }
}



// Keithley Reset Renderer Implementation
void KeithleyResetRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Reset Keithley 2400 Instrument");
  ImGui::Separator();

  // Client name parameter
  for (auto& param : block->parameters) {
    if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
      break;
    }
  }
}

void KeithleyResetRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  if (ImGui::Button("Test Reset")) {
    std::string clientName = GetParameterValue(*block, "client_name");
    bool success = machineOps->SMU_ResetInstrument(clientName);

    if (success) {
      ImGui::OpenPopup("Reset Success");
    }
    else {
      ImGui::OpenPopup("Reset Failed");
    }
  }

  // Popup modals
  if (ImGui::BeginPopupModal("Reset Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Keithley instrument reset successfully!");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Reset Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to reset Keithley instrument!");
    ImGui::Text("Check connection and try again.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void KeithleyResetRenderer::RenderValidation(MachineBlock* block) {
  // No specific validation needed for reset command
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Reset command is valid");
}

// Keithley Set Output Renderer Implementation
void KeithleySetOutputRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Keithley Output Control");
  ImGui::Separator();

  // Enable parameter
  for (auto& param : block->parameters) {
    if (param.name == "enable") {
      bool enable = (param.value == "true");
      if (ImGui::Checkbox("Enable Output", &enable)) {
        param.value = enable ? "true" : "false";
      }
    }
    else if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
    }
  }
}

void KeithleySetOutputRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  bool enable = (GetParameterValue(*block, "enable") == "true");
  std::string clientName = GetParameterValue(*block, "client_name");

  std::string buttonText = enable ? "Test Enable Output" : "Test Disable Output";
  if (ImGui::Button(buttonText.c_str())) {
    bool success = machineOps->SMU_SetOutput(enable, clientName);

    if (success) {
      ImGui::OpenPopup("Output Success");
    }
    else {
      ImGui::OpenPopup("Output Failed");
    }
  }

  // Popup modals
  if (ImGui::BeginPopupModal("Output Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Keithley output %s successfully!", enable ? "enabled" : "disabled");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Output Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to %s Keithley output!", enable ? "enable" : "disable");
    ImGui::Text("Check connection and try again.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void KeithleySetOutputRenderer::RenderValidation(MachineBlock* block) {
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Output control is valid");
}

// Keithley Voltage Source Renderer Implementation
void KeithleyVoltageSourceRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Keithley Voltage Source Setup");
  ImGui::Separator();

  for (auto& param : block->parameters) {
    if (param.name == "voltage") {
      float voltage = std::stof(param.value.empty() ? "0.0" : param.value);
      if (ImGui::InputFloat("Voltage (V)", &voltage, 0.1f, 1.0f, "%.3f")) {
        param.value = std::to_string(voltage);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output voltage in volts");
      }
    }
    else if (param.name == "compliance") {
      float compliance = std::stof(param.value.empty() ? "0.1" : param.value);
      if (ImGui::InputFloat("Current Compliance (A)", &compliance, 0.001f, 0.01f, "%.6f")) {
        param.value = std::to_string(compliance);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Current compliance limit in amperes");
      }
    }
    else if (param.name == "range") {
      char buffer[64];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Range", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Voltage range (AUTO, 20V, 200V)");
      }
    }
    else if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
    }
  }
}

void KeithleyVoltageSourceRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  if (ImGui::Button("Test Voltage Setup")) {
    std::string voltageStr = GetParameterValue(*block, "voltage");
    std::string complianceStr = GetParameterValue(*block, "compliance");
    std::string range = GetParameterValue(*block, "range");
    std::string clientName = GetParameterValue(*block, "client_name");

    try {
      double voltage = std::stod(voltageStr.empty() ? "0.0" : voltageStr);
      double compliance = std::stod(complianceStr.empty() ? "0.1" : complianceStr);

      bool success = machineOps->SMU_SetupVoltageSource(voltage, compliance, range, clientName);

      if (success) {
        ImGui::OpenPopup("Voltage Setup Success");
      }
      else {
        ImGui::OpenPopup("Voltage Setup Failed");
      }
    }
    catch (const std::exception& e) {
      ImGui::OpenPopup("Invalid Parameters");
    }
  }

  // Popup modals
  if (ImGui::BeginPopupModal("Voltage Setup Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Voltage source configured successfully!");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Voltage Setup Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to configure voltage source!");
    ImGui::Text("Check connection and parameters.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Invalid Parameters", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Invalid voltage or compliance values!");
    ImGui::Text("Please enter valid numbers.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void KeithleyVoltageSourceRenderer::RenderValidation(MachineBlock* block) {
  std::string voltageStr = GetParameterValue(*block, "voltage");
  std::string complianceStr = GetParameterValue(*block, "compliance");

  bool isValid = true;
  std::string errorMsg;

  if (voltageStr.empty()) {
    isValid = false;
    errorMsg = "Voltage value is required";
  }
  else {
    try {
      double voltage = std::stod(voltageStr);
      if (std::abs(voltage) > 200.0) {
        isValid = false;
        errorMsg = "Voltage exceeds ±200V limit";
      }
    }
    catch (const std::exception&) {
      isValid = false;
      errorMsg = "Invalid voltage format";
    }
  }

  if (isValid && !complianceStr.empty()) {
    try {
      double compliance = std::stod(complianceStr);
      if (compliance <= 0 || compliance > 1.0) {
        isValid = false;
        errorMsg = "Current compliance must be between 0 and 1A";
      }
    }
    catch (const std::exception&) {
      isValid = false;
      errorMsg = "Invalid compliance format";
    }
  }

  if (isValid) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Voltage source parameters are valid");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ %s", errorMsg.c_str());
  }
}

// Keithley Current Source Renderer Implementation
void KeithleyCurrentSourceRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Keithley Current Source Setup");
  ImGui::Separator();

  for (auto& param : block->parameters) {
    if (param.name == "current") {
      float current = std::stof(param.value.empty() ? "0.001" : param.value);
      if (ImGui::InputFloat("Current (A)", &current, 0.0001f, 0.001f, "%.6f")) {
        param.value = std::to_string(current);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output current in amperes");
      }
    }
    else if (param.name == "compliance") {
      float compliance = std::stof(param.value.empty() ? "10.0" : param.value);
      if (ImGui::InputFloat("Voltage Compliance (V)", &compliance, 0.1f, 1.0f, "%.3f")) {
        param.value = std::to_string(compliance);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Voltage compliance limit in volts");
      }
    }
    else if (param.name == "range") {
      char buffer[64];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Range", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Current range (AUTO, 1mA, 10mA, 100mA, 1A)");
      }
    }
    else if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
    }
  }
}

void KeithleyCurrentSourceRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  if (ImGui::Button("Test Current Setup")) {
    std::string currentStr = GetParameterValue(*block, "current");
    std::string complianceStr = GetParameterValue(*block, "compliance");
    std::string range = GetParameterValue(*block, "range");
    std::string clientName = GetParameterValue(*block, "client_name");

    try {
      double current = std::stod(currentStr.empty() ? "0.001" : currentStr);
      double compliance = std::stod(complianceStr.empty() ? "10.0" : complianceStr);

      bool success = machineOps->SMU_SetupCurrentSource(current, compliance, range, clientName);

      if (success) {
        ImGui::OpenPopup("Current Setup Success");
      }
      else {
        ImGui::OpenPopup("Current Setup Failed");
      }
    }
    catch (const std::exception& e) {
      ImGui::OpenPopup("Invalid Parameters");
    }
  }

  // Popup modals (similar to voltage source)
  if (ImGui::BeginPopupModal("Current Setup Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Current source configured successfully!");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Current Setup Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to configure current source!");
    ImGui::Text("Check connection and parameters.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Invalid Parameters", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Invalid current or compliance values!");
    ImGui::Text("Please enter valid numbers.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void KeithleyCurrentSourceRenderer::RenderValidation(MachineBlock* block) {
  std::string currentStr = GetParameterValue(*block, "current");
  std::string complianceStr = GetParameterValue(*block, "compliance");

  bool isValid = true;
  std::string errorMsg;

  if (currentStr.empty()) {
    isValid = false;
    errorMsg = "Current value is required";
  }
  else {
    try {
      double current = std::stod(currentStr);
      if (std::abs(current) > 1.0) {
        isValid = false;
        errorMsg = "Current exceeds ±1A limit";
      }
    }
    catch (const std::exception&) {
      isValid = false;
      errorMsg = "Invalid current format";
    }
  }

  if (isValid && !complianceStr.empty()) {
    try {
      double compliance = std::stod(complianceStr);
      if (compliance <= 0 || compliance > 200.0) {
        isValid = false;
        errorMsg = "Voltage compliance must be between 0 and 200V";
      }
    }
    catch (const std::exception&) {
      isValid = false;
      errorMsg = "Invalid compliance format";
    }
  }

  if (isValid) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Current source parameters are valid");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ %s", errorMsg.c_str());
  }
}

// Keithley Read Voltage Renderer Implementation
void KeithleyReadVoltageRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Read Keithley Voltage");
  ImGui::Separator();

  // Client name parameter
  for (auto& param : block->parameters) {
    if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
      break;
    }
  }
}

void KeithleyReadVoltageRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  static double lastVoltageReading = 0.0;
  static bool hasReading = false;

  if (ImGui::Button("Test Read Voltage")) {
    std::string clientName = GetParameterValue(*block, "client_name");
    bool success = machineOps->SMU_ReadVoltage(lastVoltageReading, clientName);
    hasReading = success;
  }

  if (hasReading) {
    ImGui::SameLine();
    ImGui::Text("Last Reading: %.6f V", lastVoltageReading);
  }
}

void KeithleyReadVoltageRenderer::RenderValidation(MachineBlock* block) {
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Voltage read command is valid");
}

// Keithley Read Current Renderer Implementation
void KeithleyReadCurrentRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Read Keithley Current");
  ImGui::Separator();

  // Client name parameter
  for (auto& param : block->parameters) {
    if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
      break;
    }
  }
}

void KeithleyReadCurrentRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  static double lastCurrentReading = 0.0;
  static bool hasReading = false;

  if (ImGui::Button("Test Read Current")) {
    std::string clientName = GetParameterValue(*block, "client_name");
    bool success = machineOps->SMU_ReadCurrent(lastCurrentReading, clientName);
    hasReading = success;
  }

  if (hasReading) {
    ImGui::SameLine();
    ImGui::Text("Last Reading: %.9f A", lastCurrentReading);
  }
}

void KeithleyReadCurrentRenderer::RenderValidation(MachineBlock* block) {
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Current read command is valid");
}

// Keithley Read Resistance Renderer Implementation
void KeithleyReadResistanceRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Read Keithley Resistance");
  ImGui::Separator();

  // Client name parameter
  for (auto& param : block->parameters) {
    if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
      break;
    }
  }
}

void KeithleyReadResistanceRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  static double lastResistanceReading = 0.0;
  static bool hasReading = false;

  if (ImGui::Button("Test Read Resistance")) {
    std::string clientName = GetParameterValue(*block, "client_name");
    bool success = machineOps->SMU_ReadResistance(lastResistanceReading, clientName);
    hasReading = success;
  }

  if (hasReading) {
    ImGui::SameLine();
    ImGui::Text("Last Reading: %.3f Ω", lastResistanceReading);
  }
}

void KeithleyReadResistanceRenderer::RenderValidation(MachineBlock* block) {
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Resistance read command is valid");
}

// Keithley Send Command Renderer Implementation
void KeithleySendCommandRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Text("Send Keithley SCPI Command");
  ImGui::Separator();

  for (auto& param : block->parameters) {
    if (param.name == "command") {
      char buffer[512];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("SCPI Command", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("SCPI command to send (e.g., *IDN?, :SOUR:VOLT 5.0)");
      }
    }
    else if (param.name == "client_name") {
      char buffer[256];
      strncpy(buffer, param.value.c_str(), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';

      if (ImGui::InputText("Client Name", buffer, sizeof(buffer))) {
        param.value = std::string(buffer);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optional Keithley client name (leave empty for default)");
      }
    }
  }
}

void KeithleySendCommandRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  if (!machineOps) return;

  if (ImGui::Button("Test Send Command")) {
    std::string command = GetParameterValue(*block, "command");
    std::string clientName = GetParameterValue(*block, "client_name");

    if (command.empty()) {
      ImGui::OpenPopup("Empty Command");
      return;
    }

    bool success = machineOps->SMU_SendCommand(command, clientName);

    if (success) {
      ImGui::OpenPopup("Command Success");
    }
    else {
      ImGui::OpenPopup("Command Failed");
    }
  }

  // Popup modals
  if (ImGui::BeginPopupModal("Command Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("SCPI command sent successfully!");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Command Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to send SCPI command!");
    ImGui::Text("Check connection and command syntax.");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Empty Command", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please enter a SCPI command first!");
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void KeithleySendCommandRenderer::RenderValidation(MachineBlock* block) {
  std::string command = GetParameterValue(*block, "command");

  if (command.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ SCPI command is required");
  }
  else if (command.length() > 256) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ Command too long (max 256 characters)");
  }
  else {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ SCPI command is valid");
  }
}


void ScanOperationRenderer::RenderProperties(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Scan Operation Configuration");
  ImGui::Separator();

  // Custom UI for scan-specific parameters
  for (auto& param : block->parameters) {
    if (param.name == "device_name") {
      ImGui::Text("Device Name:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(150);
      char buffer[256];
      strcpy_s(buffer, sizeof(buffer), param.value.c_str());
      if (ImGui::InputText("##device_name", buffer, sizeof(buffer))) {
        param.value = buffer;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Motion controller device (e.g., hex-left, hex-right)");
      }
    }
    else if (param.name == "data_channel") {
      ImGui::Text("Data Channel:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(150);
      char buffer[256];
      strcpy_s(buffer, sizeof(buffer), param.value.c_str());
      if (ImGui::InputText("##data_channel", buffer, sizeof(buffer))) {
        param.value = buffer;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Data source to monitor (e.g., GPIB-Current)");
      }
    }
    else if (param.name == "step_sizes_um") {
      ImGui::Text("Step Sizes (µm):");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      char buffer[256];
      strcpy_s(buffer, sizeof(buffer), param.value.c_str());
      if (ImGui::InputText("##step_sizes", buffer, sizeof(buffer))) {
        param.value = buffer;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Comma-separated step sizes in micrometers\nExample: 2,1,0.5 for multi-stage scanning");
      }
    }
    else if (param.name == "settling_time_ms") {
      ImGui::Text("Settling Time (ms):");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      int value = std::stoi(param.value.empty() ? "300" : param.value);
      if (ImGui::InputInt("##settling_time", &value)) {
        param.value = std::to_string(value);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Time to wait after each motion step");
      }
    }
    else if (param.name == "axes_to_scan") {
      ImGui::Text("Scan Axes:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      char buffer[256];
      strcpy_s(buffer, sizeof(buffer), param.value.c_str());
      if (ImGui::InputText("##axes", buffer, sizeof(buffer))) {
        param.value = buffer;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Comma-separated axes in scan order\nExample: Z,X,Y");
      }
    }
    else if (param.name == "timeout_minutes") {
      ImGui::Text("Timeout (min):");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      int value = std::stoi(param.value.empty() ? "30" : param.value);
      if (ImGui::InputInt("##timeout", &value)) {
        param.value = std::to_string(value);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum time to wait for scan completion");
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Show scan preview
  ScanParameters params = ExtractScanParameters(block);
  auto stepSizes = ParseStepSizes(params.stepSizesStr);
  auto axes = ParseAxes(params.axesStr);

  ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Scan Preview:");
  ImGui::Text("Device: %s", params.deviceName.c_str());
  ImGui::Text("Channel: %s", params.dataChannel.c_str());

  if (!stepSizes.empty()) {
    ImGui::Text("Steps: ");
    ImGui::SameLine();
    for (size_t i = 0; i < stepSizes.size(); ++i) {
      ImGui::SameLine();
      ImGui::Text("%.1fµm", stepSizes[i] * 1000);
      if (i < stepSizes.size() - 1) {
        ImGui::SameLine();
        ImGui::Text(" → ");
      }
    }
  }

  if (!axes.empty()) {
    ImGui::Text("Axes: ");
    for (size_t i = 0; i < axes.size(); ++i) {
      ImGui::SameLine();
      ImGui::Text("%s", axes[i].c_str());
      if (i < axes.size() - 1) {
        ImGui::SameLine();
        ImGui::Text(" → ");
      }
    }
  }
}

void ScanOperationRenderer::RenderActions(MachineBlock* block, MachineOperations* machineOps) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Scan Actions:");

  ScanParameters params = ExtractScanParameters(block);

  // Show current scan status
  RenderScanStatus(params, machineOps);

  ImGui::Spacing();

  // Test scan button
  if (ImGui::Button("Test Scan", ImVec2(100, 0))) {
    RenderTestButton(params, machineOps);
  }

  ImGui::SameLine();

  // Stop scan button
  if (ImGui::Button("Stop Scan", ImVec2(100, 0))) {
    if (machineOps && !params.deviceName.empty()) {
      bool success = machineOps->StopScan(params.deviceName);
      if (success) {
        printf("[Yes] Scan stopped successfully on %s\n", params.deviceName.c_str());
      }
      else {
        printf("[Fail] Failed to stop scan on %s\n", params.deviceName.c_str());
      }
    }
  }

  ImGui::SameLine();

  // Check device connection
  if (ImGui::Button("Check Device", ImVec2(100, 0))) {
    if (machineOps && !params.deviceName.empty()) {
      bool connected = machineOps->IsDeviceConnected(params.deviceName);
      printf("🔌 Device %s: %s\n", params.deviceName.c_str(),
        connected ? "Connected" : "Disconnected");
    }
  }
}

void ScanOperationRenderer::RenderValidation(MachineBlock* block) {
  ScanParameters params = ExtractScanParameters(block);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Validation:");

  bool hasErrors = false;

  // Validate device name
  if (params.deviceName.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Fail] Device name is required");
    hasErrors = true;
  }

  // Validate data channel
  if (params.dataChannel.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Fail] Data channel is required");
    hasErrors = true;
  }

  // Validate step sizes
  auto stepSizes = ParseStepSizes(params.stepSizesStr);
  if (stepSizes.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[CAUTION] Invalid step sizes, will use defaults");
  }

  // Validate axes
  auto axes = ParseAxes(params.axesStr);
  if (axes.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[CAUTION] Invalid axes, will use defaults (Z,X,Y)");
  }

  // Validate settling time
  if (params.settlingTimeMs < 50) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[CAUTION] Settling time very low (< 50ms)");
  }

  // Validate timeout
  if (params.timeoutMinutes < 1) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[CAUTION] Timeout very short (< 1 minute)");
  }

  if (!hasErrors) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[Yes] Configuration valid");
  }
}

// Helper method implementations
ScanOperationRenderer::ScanParameters ScanOperationRenderer::ExtractScanParameters(MachineBlock* block) {
  ScanParameters params;

  for (const auto& param : block->parameters) {
    if (param.name == "device_name") {
      params.deviceName = param.value;
    }
    else if (param.name == "data_channel") {
      params.dataChannel = param.value;
    }
    else if (param.name == "step_sizes_um") {
      params.stepSizesStr = param.value;
    }
    else if (param.name == "settling_time_ms") {
      params.settlingTimeMs = std::stoi(param.value.empty() ? "300" : param.value);
    }
    else if (param.name == "axes_to_scan") {
      params.axesStr = param.value;
    }
    else if (param.name == "timeout_minutes") {
      params.timeoutMinutes = std::stoi(param.value.empty() ? "30" : param.value);
    }
  }

  return params;
}

void ScanOperationRenderer::RenderTestButton(const ScanParameters& params, MachineOperations* machineOps) {
  if (!machineOps) {
    printf("[Fail] No machine operations available for testing\n");
    return;
  }

  if (params.deviceName.empty() || params.dataChannel.empty()) {
    printf("[Fail] Cannot test: Device name and data channel are required\n");
    return;
  }

  auto stepSizes = ParseStepSizes(params.stepSizesStr);
  auto axes = ParseAxes(params.axesStr);

  printf("🔍 Starting test scan on %s using %s...\n",
    params.deviceName.c_str(), params.dataChannel.c_str());

  bool success = machineOps->StartScan(
    params.deviceName,
    params.dataChannel,
    stepSizes,
    params.settlingTimeMs,
    axes
  );

  if (success) {
    printf("[Yes] Test scan started successfully\n");
  }
  else {
    printf("[Fail] Failed to start test scan\n");
  }
}

void ScanOperationRenderer::RenderScanStatus(const ScanParameters& params, MachineOperations* machineOps) {
  if (!machineOps || params.deviceName.empty()) {
    ImGui::Text("Status: No device specified");
    return;
  }

  bool isActive = machineOps->IsScanActive(params.deviceName);
  double progress = machineOps->GetScanProgress(params.deviceName);
  std::string status = machineOps->GetScanStatus(params.deviceName);

  if (isActive) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "🔍 SCANNING ACTIVE");
    ImGui::ProgressBar(progress / 100.0f, ImVec2(200, 0));
    ImGui::Text("Status: %s", status.c_str());
    ImGui::Text("Progress: %.1f%%", progress);
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏸️ No active scan");
    ImGui::Text("Status: %s", status.c_str());
  }
}

std::vector<double> ScanOperationRenderer::ParseStepSizes(const std::string& stepSizesStr) {
  std::vector<double> stepSizes;
  std::stringstream ss(stepSizesStr);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Trim whitespace
    item.erase(0, item.find_first_not_of(' '));
    item.erase(item.find_last_not_of(' ') + 1);

    if (!item.empty()) {
      try {
        double stepUm = std::stod(item);
        double stepMm = stepUm / 1000.0; // Convert to millimeters
        stepSizes.push_back(stepMm);
      }
      catch (const std::exception&) {
        // Skip invalid values
      }
    }
  }

  return stepSizes;
}

std::vector<std::string> ScanOperationRenderer::ParseAxes(const std::string& axesStr) {
  std::vector<std::string> axes;
  std::stringstream ss(axesStr);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Trim whitespace
    item.erase(0, item.find_first_not_of(' '));
    item.erase(item.find_last_not_of(' ') + 1);

    if (!item.empty()) {
      axes.push_back(item);
    }
  }

  return axes;  // [Yes] Now properly returns the parsed axes vector
}