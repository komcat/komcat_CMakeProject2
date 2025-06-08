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
  default:
    return std::make_unique<DefaultRenderer>();
  }
}