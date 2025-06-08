// MachineBlockUI.cpp
#include "MachineBlockUI.h"
#include "BlockSequenceConverter.h"  // Your new converter class
#include "include/SequenceStep.h"           // For sequence execution
#include "include/machine_operations.h"     // For MachineOperations
#include <functional>               // For std::function
#include <thread>                   // For std::thread
#include <algorithm>                // For std::transform
#include <iostream>                 // For printf (if not already included)


MachineBlockUI::MachineBlockUI() {
  InitializePalette();
}

void MachineBlockUI::InitializePalette() {
  m_paletteBlocks.clear();

  // Create palette blocks with START and END prioritized
  m_paletteBlocks.emplace_back(0, BlockType::START, "START", START_COLOR);
  m_paletteBlocks.emplace_back(0, BlockType::MOVE_NODE, "Move Node", MOVE_NODE_COLOR);
  m_paletteBlocks.emplace_back(0, BlockType::WAIT, "Wait", WAIT_COLOR);
  m_paletteBlocks.emplace_back(0, BlockType::SET_OUTPUT, "Set Output", SET_OUTPUT_COLOR);
  m_paletteBlocks.emplace_back(0, BlockType::CLEAR_OUTPUT, "Clear Output", CLEAR_OUTPUT_COLOR);
  m_paletteBlocks.emplace_back(0, BlockType::END, "END", END_COLOR);
}

void MachineBlockUI::RenderUI() {
  if (!m_showWindow) return;

  // Main window with specific size and position
  ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
  ImGui::Begin("Machine Block Programming", &m_showWindow, ImGuiWindowFlags_NoScrollbar);

  // Program validation status
  bool isValid = ValidateProgram();
  if (!isValid) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Text("⚠ Program Invalid: ");
    ImGui::SameLine();
    if (CountBlocksOfType(BlockType::START) == 0) {
      ImGui::Text("Missing START block. ");
    }
    if (CountBlocksOfType(BlockType::END) == 0) {
      ImGui::Text("Missing END block. ");
    }
    if (CountBlocksOfType(BlockType::START) > 1) {
      ImGui::Text("Multiple START blocks found. ");
    }
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Text("✓ Program Valid - Ready to Execute");
    ImGui::PopStyleColor();
  }

  // Calculate panel sizes
  ImVec2 windowSize = ImGui::GetContentRegionAvail();
  float middlePanelWidth = windowSize.x - m_leftPanelWidth - m_rightPanelWidth - 20.0f;

  // Left Panel - Palette
  ImGui::BeginChild("PalettePanel", ImVec2(m_leftPanelWidth, windowSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Program Canvas
  ImGui::BeginChild("ProgramCanvas", ImVec2(middlePanelWidth, windowSize.y), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Properties
  ImGui::BeginChild("PropertiesPanel", ImVec2(m_rightPanelWidth, windowSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();

  ImGui::End();
}

void MachineBlockUI::RenderLeftPanel() {
  ImGui::Text("Block Palette");
  ImGui::Separator();

  ImGui::TextWrapped("Essential blocks for program flow:");
  ImGui::Spacing();

  // Instructions for START/END usage
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
  ImGui::TextWrapped("• START: Every program needs exactly one START block");
  ImGui::PopStyleColor();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
  ImGui::TextWrapped("• END: Every program should end with an END block");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::Separator();

  // Render palette blocks
  for (size_t i = 0; i < m_paletteBlocks.size(); i++) {
    RenderPaletteBlock(m_paletteBlocks[i], static_cast<int>(i));
    ImGui::Spacing();
  }

  ImGui::Separator();

  // Program controls
  if (ImGui::Button("Clear All", ImVec2(-1, 0))) {
    ClearAll();
  }

  ImGui::Spacing();

  // Quick Start button
  if (ImGui::Button("Quick Start", ImVec2(-1, 0))) {
    QuickStart();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Adds START and END blocks to get you started");
  }

  ImGui::Spacing();

  if (ImGui::Button("Save Program", ImVec2(-1, 0))) {
    SaveProgram();
  }

  if (ImGui::Button("Load Program", ImVec2(-1, 0))) {
    LoadProgram();
  }
}

void MachineBlockUI::RenderPaletteBlock(const MachineBlock& block, int index) {
  ImVec2 buttonSize(m_leftPanelWidth - 20, 50);

  // Create a colored button for the palette block
  ImGui::PushStyleColor(ImGuiCol_Button, block.color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(block.color));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(block.color));

  std::string buttonLabel = block.label + "##palette" + std::to_string(index);

  if (ImGui::Button(buttonLabel.c_str(), buttonSize)) {
    // Add block to program at center of canvas
    ImVec2 centerPos(200, 100 + static_cast<float>(m_programBlocks.size()) * 80);
    AddBlockToProgram(block.type, centerPos);
  }

  // Handle drag from palette
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    if (!m_isDragging) {
      // Start dragging from palette
      m_isDragging = true;
    }
  }

  ImGui::PopStyleColor(3);

  // Special highlighting for START/END blocks
  if (block.type == BlockType::START || block.type == BlockType::END) {
    ImGui::SameLine();
    if (block.type == BlockType::START) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "⭐");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "🛑");
    }
  }

  // Tooltip
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Drag to canvas or click to add %s block", block.label.c_str());
  }
}

void MachineBlockUI::RenderMiddlePanel() {
  ImGui::Text("Program Canvas");
  ImGui::SameLine();
  ImGui::Text("| Blocks: %zu", m_programBlocks.size());
  ImGui::SameLine();
  ImGui::Text("| Connections: %zu", m_connections.size());

  if (m_selectedBlock) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "| Selected: %s", m_selectedBlock->label.c_str());
  }

  if (m_isConnecting) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "| CONNECTING...");
  }

  ImGui::Separator();

  // Canvas controls
  if (ImGui::Button("Reset View")) {
    m_canvasOffset = ImVec2(0, 0);
    m_canvasZoom = 1.0f;
  }
  ImGui::SameLine();

  if (ImGui::Button("Execute Program")) {
    ExecuteProgram();
  }
  ImGui::SameLine();

  // Program validation button
  if (ValidateProgram()) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
    ImGui::Button("✓ Valid");
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
    ImGui::Button("⚠ Invalid");
    ImGui::PopStyleColor();
  }

  if (m_isConnecting) {
    ImGui::SameLine();
    if (ImGui::Button("Cancel Connect")) {
      CancelConnection();
    }
  }

  if (m_selectedBlock) {
    ImGui::SameLine();
    if (ImGui::Button("Delete Block")) {
      DeleteSelectedBlock();
    }
  }

  // Canvas area
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();

  // Draw canvas background
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
    CANVAS_BG_COLOR);

  // Draw grid
  RenderGrid(canvasPos, canvasSize);

  // Handle canvas interactions
  bool isCanvasHovered = ImGui::IsWindowHovered() &&
    ImGui::IsMouseHoveringRect(canvasPos,
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

  // Pan with middle mouse button
  if (isCanvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    ImVec2 delta = ImGui::GetIO().MouseDelta;
    m_canvasOffset.x += delta.x / m_canvasZoom;
    m_canvasOffset.y += delta.y / m_canvasZoom;
  }

  // Zoom with mouse wheel
  if (isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
    float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
    m_canvasZoom = (std::max)(0.3f, (std::min)(m_canvasZoom + zoomDelta, 3.0f));
  }

  // Handle block selection and dragging
  if (isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    MachineBlock* clickedBlock = GetBlockAtPosition(mousePos, canvasPos);

    if (clickedBlock) {
      m_selectedBlock = clickedBlock;
      m_selectedBlock->selected = true;

      // Deselect other blocks
      for (auto& block : m_programBlocks) {
        if (block.get() != m_selectedBlock) {
          block->selected = false;
        }
      }

      // Start dragging
      m_isDragging = true;
      m_draggedBlock = clickedBlock;
      ImVec2 worldPos = CanvasToWorld(canvasPos, mousePos);
      m_dragOffset = ImVec2(worldPos.x - clickedBlock->position.x, worldPos.y - clickedBlock->position.y);
    }
    else {
      // Deselect all blocks
      m_selectedBlock = nullptr;
      for (auto& block : m_programBlocks) {
        block->selected = false;
      }
    }
  }

  // Handle block dragging
  if (m_isDragging && m_draggedBlock && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    ImVec2 worldPos = CanvasToWorld(canvasPos, mousePos);
    m_draggedBlock->position = ImVec2(worldPos.x - m_dragOffset.x, worldPos.y - m_dragOffset.y);
  }

  // Stop dragging
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    m_isDragging = false;
    m_draggedBlock = nullptr;
  }

  // Right-click context menu
  if (isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    MachineBlock* clickedBlock = GetBlockAtPosition(mousePos, canvasPos);

    if (clickedBlock) {
      ImGui::OpenPopup("BlockContextMenu");
      m_selectedBlock = clickedBlock;
    }
  }

  // Block context menu
  if (ImGui::BeginPopup("BlockContextMenu")) {
    if (m_selectedBlock) {
      ImGui::Text("Block: %s", m_selectedBlock->label.c_str());
      ImGui::Separator();

      if (ImGui::MenuItem("Delete Block")) {
        DeleteSelectedBlock();
      }

      if (CanBlockProvideOutput(*m_selectedBlock) && ImGui::MenuItem("Start Connection")) {
        StartConnection(m_selectedBlock, GetBlockOutputPoint(*m_selectedBlock, canvasPos));
      }
    }
    ImGui::EndPopup();
  }

  // Render connections
  RenderConnections(canvasPos);

  // Render blocks
  for (const auto& block : m_programBlocks) {
    RenderProgramBlock(*block, canvasPos);
  }

  // Handle connection completion
  if (m_isConnecting && isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    MachineBlock* targetBlock = GetBlockAtPosition(mousePos, canvasPos);

    if (targetBlock && targetBlock != m_connectionStart && CanBlockAcceptInput(*targetBlock)) {
      CompleteConnection(targetBlock);
    }
    else {
      CancelConnection();
    }
  }

  // Draw connection preview
  if (m_isConnecting) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    drawList->AddLine(m_connectionStartPos, mousePos, IM_COL32(255, 255, 0, 200), 3.0f);
  }
}

void MachineBlockUI::RenderProgramBlock(const MachineBlock& block, const ImVec2& canvasPos) {
  ImVec2 screenPos = WorldToCanvas(canvasPos, block.position);
  ImVec2 blockSize(BLOCK_WIDTH * m_canvasZoom, BLOCK_HEIGHT * m_canvasZoom);

  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Block background
  ImU32 blockColor = block.color;
  ImU32 textColor = IM_COL32(255, 255, 255, 255); // Default white text

  if (block.selected) {
    blockColor = IM_COL32(255, 255, 0, 255); // Yellow when selected
    textColor = IM_COL32(0, 0, 0, 255);      // Black text for selected blocks
  }

  drawList->AddRectFilled(screenPos,
    ImVec2(screenPos.x + blockSize.x, screenPos.y + blockSize.y),
    blockColor, BLOCK_ROUNDING * m_canvasZoom);

  // Block border
  drawList->AddRect(screenPos,
    ImVec2(screenPos.x + blockSize.x, screenPos.y + blockSize.y),
    IM_COL32(255, 255, 255, 150), BLOCK_ROUNDING * m_canvasZoom, 0, 2.0f);

  // Block text with appropriate color
  ImVec2 textSize = ImGui::CalcTextSize(block.label.c_str());
  ImVec2 textPos = ImVec2(
    screenPos.x + (blockSize.x - textSize.x) * 0.5f,
    screenPos.y + (blockSize.y - textSize.y) * 0.5f
  );

  drawList->AddText(textPos, textColor, block.label.c_str());

  // Connection points
  if (CanBlockAcceptInput(block)) {
    // Input connection point (left side)
    ImVec2 inputPoint = GetBlockInputPoint(block, canvasPos);
    drawList->AddCircleFilled(inputPoint, CONNECTOR_RADIUS * m_canvasZoom,
      IM_COL32(100, 100, 255, 255));
  }

  if (CanBlockProvideOutput(block)) {
    // Output connection point (right side)
    ImVec2 outputPoint = GetBlockOutputPoint(block, canvasPos);
    drawList->AddCircleFilled(outputPoint, CONNECTOR_RADIUS * m_canvasZoom,
      IM_COL32(255, 100, 100, 255));
  }

  // Special indicators for START/END blocks
  if (block.type == BlockType::START) {
    drawList->AddText(ImVec2(screenPos.x + 5, screenPos.y + 5),
      textColor, "⭐");
  }
  else if (block.type == BlockType::END) {
    drawList->AddText(ImVec2(screenPos.x + 5, screenPos.y + 5),
      textColor, "🛑");
  }
}

void MachineBlockUI::RenderConnections(const ImVec2& canvasPos) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  for (const auto& connection : m_connections) {
    // Find the blocks
    MachineBlock* fromBlock = nullptr;
    MachineBlock* toBlock = nullptr;

    for (const auto& block : m_programBlocks) {
      if (block->id == connection.fromBlockId) {
        fromBlock = block.get();
      }
      if (block->id == connection.toBlockId) {
        toBlock = block.get();
      }
    }

    if (fromBlock && toBlock) {
      ImVec2 startPos = GetBlockOutputPoint(*fromBlock, canvasPos);
      ImVec2 endPos = GetBlockInputPoint(*toBlock, canvasPos);

      // Draw connection line with bezier curve
      ImVec2 cp1 = ImVec2(startPos.x + 50 * m_canvasZoom, startPos.y);
      ImVec2 cp2 = ImVec2(endPos.x - 50 * m_canvasZoom, endPos.y);

      drawList->AddBezierCubic(startPos, cp1, cp2, endPos,
        IM_COL32(255, 255, 255, 200), 3.0f * m_canvasZoom);

      // Arrow head
      ImVec2 dir = ImVec2(endPos.x - cp2.x, endPos.y - cp2.y);
      float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
      if (len > 0) {
        dir.x /= len;
        dir.y /= len;

        ImVec2 arrowP1 = ImVec2(endPos.x - 10 * dir.x - 5 * dir.y, endPos.y - 10 * dir.y + 5 * dir.x);
        ImVec2 arrowP2 = ImVec2(endPos.x - 10 * dir.x + 5 * dir.y, endPos.y - 10 * dir.y - 5 * dir.x);

        drawList->AddTriangleFilled(endPos, arrowP1, arrowP2, IM_COL32(255, 255, 255, 200));
      }
    }
  }
}

void MachineBlockUI::RenderGrid(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  float gridStep = 20.0f * m_canvasZoom;

  for (float x = fmodf(m_canvasOffset.x * m_canvasZoom, gridStep); x < canvasSize.x; x += gridStep) {
    drawList->AddLine(
      ImVec2(canvasPos.x + x, canvasPos.y),
      ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
      GRID_COLOR);
  }

  for (float y = fmodf(m_canvasOffset.y * m_canvasZoom, gridStep); y < canvasSize.y; y += gridStep) {
    drawList->AddLine(
      ImVec2(canvasPos.x, canvasPos.y + y),
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
      GRID_COLOR);
  }
}

void MachineBlockUI::RenderRightPanel() {
  ImGui::Text("Properties");
  ImGui::Separator();

  // Add execution status at the top of right panel
  RenderExecutionStatus();

  ImGui::Spacing();
  ImGui::Separator();

  if (m_selectedBlock) {
    ImGui::Text("Block: %s", m_selectedBlock->label.c_str());
    ImGui::Text("Type: %s", BlockTypeToString(m_selectedBlock->type).c_str());
    ImGui::Text("ID: %d", m_selectedBlock->id);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Parameters:");

    // Render parameters
    for (auto& param : m_selectedBlock->parameters) {
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
  }
  else {
    ImGui::TextWrapped("Select a block to view and edit its properties.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Program Info:");
    ImGui::Text("Total Blocks: %zu", m_programBlocks.size());
    ImGui::Text("START Blocks: %d", CountBlocksOfType(BlockType::START));
    ImGui::Text("END Blocks: %d", CountBlocksOfType(BlockType::END));
    ImGui::Text("Connections: %zu", m_connections.size());

    ImGui::Spacing();
    if (ValidateProgram()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Program is valid");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠ Program has issues");
    }
  }
}
// Canvas utility methods
ImVec2 MachineBlockUI::WorldToCanvas(const ImVec2& canvasPos, const ImVec2& worldPos) const {
  return ImVec2(
    canvasPos.x + (worldPos.x + m_canvasOffset.x) * m_canvasZoom,
    canvasPos.y + (worldPos.y + m_canvasOffset.y) * m_canvasZoom
  );
}

ImVec2 MachineBlockUI::CanvasToWorld(const ImVec2& canvasPos, const ImVec2& canvasPos_) const {
  return ImVec2(
    (canvasPos_.x - canvasPos.x) / m_canvasZoom - m_canvasOffset.x,
    (canvasPos_.y - canvasPos.y) / m_canvasZoom - m_canvasOffset.y
  );
}

// Block management methods
void MachineBlockUI::AddBlockToProgram(BlockType type, const ImVec2& position) {
  // Prevent multiple START blocks
  if (type == BlockType::START && CountBlocksOfType(BlockType::START) > 0) {
    printf("❌ Only one START block allowed per program!\n");
    return;
  }

  auto newBlock = std::make_unique<MachineBlock>(m_nextBlockId++, type, BlockTypeToString(type), GetBlockColor(type));
  newBlock->position = position;

  InitializeBlockParameters(*newBlock);

  m_programBlocks.push_back(std::move(newBlock));

  printf("✅ Added %s block (ID: %d)\n", BlockTypeToString(type).c_str(), m_nextBlockId - 1);
}

void MachineBlockUI::InitializeBlockParameters(MachineBlock& block) {
  block.parameters.clear();

  switch (block.type) {
  case BlockType::START:
    block.parameters.push_back({ "program_name", "MyProgram", "string", "Name of this program" });
    block.parameters.push_back({ "description", "Program description", "string", "What this program does" });
    block.parameters.push_back({ "author", "User", "string", "Program author" });
    break;

  case BlockType::END:
    block.parameters.push_back({ "cleanup", "true", "bool", "Perform cleanup operations" });
    block.parameters.push_back({ "return_home", "false", "bool", "Return to home position" });
    block.parameters.push_back({ "save_log", "true", "bool", "Save execution log" });
    break;

  case BlockType::MOVE_NODE:
    block.parameters.push_back({ "device_name", "gantry-main", "string", "Name of the device to move" });
    block.parameters.push_back({ "graph_name", "Process_Flow", "string", "Name of the motion graph" });
    block.parameters.push_back({ "node_id", "node_4027", "string", "Target node ID" });
    block.parameters.push_back({ "blocking", "true", "bool", "Wait for completion" });
    break;

  case BlockType::WAIT:
    block.parameters.push_back({ "milliseconds", "1000", "int", "Time to wait in milliseconds" });
    block.parameters.push_back({ "description", "Pause", "string", "Purpose of this wait" });
    break;

  case BlockType::SET_OUTPUT:
    block.parameters.push_back({ "device_name", "IOBottom", "string", "IO device name" });
    block.parameters.push_back({ "pin", "0", "int", "Output pin number" });
    block.parameters.push_back({ "state", "true", "bool", "Output state (on/off)" });
    block.parameters.push_back({ "delay_ms", "200", "int", "Delay after setting (ms)" });
    break;

  case BlockType::CLEAR_OUTPUT:
    block.parameters.push_back({ "device_name", "IOBottom", "string", "IO device name" });
    block.parameters.push_back({ "pin", "0", "int", "Output pin number to clear" });
    block.parameters.push_back({ "delay_ms", "100", "int", "Delay after clearing (ms)" });
    break;
  }
}

void MachineBlockUI::DeleteSelectedBlock() {
  if (!m_selectedBlock) return;

  int blockId = m_selectedBlock->id;

  printf("🗑️ Deleting block: %s (ID: %d)\n", m_selectedBlock->label.c_str(), blockId);

  // First, safely remove all connections involving this block
  auto connectionsToRemove = std::vector<BlockConnection>();
  for (const auto& conn : m_connections) {
    if (conn.fromBlockId == blockId || conn.toBlockId == blockId) {
      connectionsToRemove.push_back(conn);
    }
  }

  // Remove connections and update affected blocks
  for (const auto& conn : connectionsToRemove) {
    printf("   Removing connection: %d → %d\n", conn.fromBlockId, conn.toBlockId);
    DeleteConnection(conn.fromBlockId, conn.toBlockId);
  }

  // Double-check: Clean up any remaining references in all blocks
  for (auto& block : m_programBlocks) {
    if (block.get() == m_selectedBlock) continue; // Skip the block we're deleting

    // Remove from input connections
    auto inputIt = std::find(block->inputConnections.begin(), block->inputConnections.end(), blockId);
    if (inputIt != block->inputConnections.end()) {
      block->inputConnections.erase(inputIt);
      printf("   Cleaned input reference from block %d\n", block->id);
    }

    // Remove from output connections
    auto outputIt = std::find(block->outputConnections.begin(), block->outputConnections.end(), blockId);
    if (outputIt != block->outputConnections.end()) {
      block->outputConnections.erase(outputIt);
      printf("   Cleaned output reference from block %d\n", block->id);
    }
  }

  // Cancel any ongoing connection if it involves this block
  if (m_isConnecting && m_connectionStart && m_connectionStart->id == blockId) {
    printf("   Cancelling ongoing connection\n");
    CancelConnection();
  }

  // Remove the block itself
  m_programBlocks.erase(
    std::remove_if(m_programBlocks.begin(), m_programBlocks.end(),
      [this](const std::unique_ptr<MachineBlock>& block) {
    return block.get() == m_selectedBlock;
  }),
    m_programBlocks.end()
  );

  // Clear selection
  m_selectedBlock = nullptr;

  printf("✅ Block deleted successfully. Remaining blocks: %zu, connections: %zu\n",
    m_programBlocks.size(), m_connections.size());
}

MachineBlock* MachineBlockUI::GetBlockAtPosition(const ImVec2& pos, const ImVec2& canvasPos) {
  ImVec2 worldPos = CanvasToWorld(canvasPos, pos);

  // Check blocks in reverse order (top to bottom rendering)
  for (auto it = m_programBlocks.rbegin(); it != m_programBlocks.rend(); ++it) {
    const auto& block = *it;
    ImVec2 blockSize(BLOCK_WIDTH, BLOCK_HEIGHT);

    if (worldPos.x >= block->position.x && worldPos.x <= block->position.x + blockSize.x &&
      worldPos.y >= block->position.y && worldPos.y <= block->position.y + blockSize.y) {
      return block.get();
    }
  }

  return nullptr;
}

// Connection methods
ImVec2 MachineBlockUI::GetBlockInputPoint(const MachineBlock& block, const ImVec2& canvasPos) const {
  ImVec2 blockScreenPos = WorldToCanvas(canvasPos, block.position);
  return ImVec2(
    blockScreenPos.x,
    blockScreenPos.y + (BLOCK_HEIGHT * m_canvasZoom) * 0.5f
  );
}

ImVec2 MachineBlockUI::GetBlockOutputPoint(const MachineBlock& block, const ImVec2& canvasPos) const {
  ImVec2 blockScreenPos = WorldToCanvas(canvasPos, block.position);
  return ImVec2(
    blockScreenPos.x + BLOCK_WIDTH * m_canvasZoom,
    blockScreenPos.y + (BLOCK_HEIGHT * m_canvasZoom) * 0.5f
  );
}

void MachineBlockUI::StartConnection(MachineBlock* fromBlock, const ImVec2& startPos) {
  if (!CanBlockProvideOutput(*fromBlock)) return;

  m_isConnecting = true;
  m_connectionStart = fromBlock;
  m_connectionStartPos = startPos;

  printf("🔗 Starting connection from %s (ID: %d)\n", fromBlock->label.c_str(), fromBlock->id);
}

void MachineBlockUI::CompleteConnection(MachineBlock* toBlock) {
  if (!m_isConnecting || !m_connectionStart || !toBlock) return;
  if (!CanBlockAcceptInput(*toBlock)) return;

  // Check if connection already exists
  for (const auto& conn : m_connections) {
    if (conn.fromBlockId == m_connectionStart->id && conn.toBlockId == toBlock->id) {
      printf("⚠️ Connection already exists!\n");
      CancelConnection();
      return;
    }
  }

  // Create new connection
  BlockConnection newConnection;
  newConnection.fromBlockId = m_connectionStart->id;
  newConnection.toBlockId = toBlock->id;
  m_connections.push_back(newConnection);

  // Update block connection lists
  m_connectionStart->outputConnections.push_back(toBlock->id);
  toBlock->inputConnections.push_back(m_connectionStart->id);

  printf("✅ Connected %s (ID: %d) → %s (ID: %d)\n",
    m_connectionStart->label.c_str(), m_connectionStart->id,
    toBlock->label.c_str(), toBlock->id);

  CancelConnection();
}

void MachineBlockUI::CancelConnection() {
  m_isConnecting = false;
  m_connectionStart = nullptr;
}

void MachineBlockUI::DeleteConnection(int fromBlockId, int toBlockId) {
  // Remove from connections vector
  m_connections.erase(
    std::remove_if(m_connections.begin(), m_connections.end(),
      [fromBlockId, toBlockId](const BlockConnection& conn) {
    return conn.fromBlockId == fromBlockId && conn.toBlockId == toBlockId;
  }),
    m_connections.end()
  );

  // Remove from blocks' connection lists
  for (auto& block : m_programBlocks) {
    if (block->id == fromBlockId) {
      // Remove from output connections
      block->outputConnections.erase(
        std::remove(block->outputConnections.begin(), block->outputConnections.end(), toBlockId),
        block->outputConnections.end()
      );
    }
    if (block->id == toBlockId) {
      // Remove from input connections
      block->inputConnections.erase(
        std::remove(block->inputConnections.begin(), block->inputConnections.end(), fromBlockId),
        block->inputConnections.end()
      );
    }
  }
}

bool MachineBlockUI::CanBlockAcceptInput(const MachineBlock& block) const {
  // START blocks cannot accept input
  return block.type != BlockType::START;
}

bool MachineBlockUI::CanBlockProvideOutput(const MachineBlock& block) const {
  // END blocks cannot provide output
  return block.type != BlockType::END;
}

// Program validation and execution
bool MachineBlockUI::ValidateProgram() const {
  int startCount = CountBlocksOfType(BlockType::START);
  int endCount = CountBlocksOfType(BlockType::END);

  // Must have exactly one START block and at least one END block
  return (startCount == 1 && endCount >= 1);
}

int MachineBlockUI::CountBlocksOfType(BlockType type) const {
  int count = 0;
  for (const auto& block : m_programBlocks) {
    if (block->type == type) {
      count++;
    }
  }
  return count;
}

MachineBlock* MachineBlockUI::FindStartBlock() {
  for (const auto& block : m_programBlocks) {
    if (block->type == BlockType::START) {
      return block.get();
    }
  }
  return nullptr;
}

std::vector<MachineBlock*> MachineBlockUI::GetExecutionOrder() {
  std::vector<MachineBlock*> executionOrder;
  MachineBlock* currentBlock = FindStartBlock();

  if (!currentBlock) {
    printf("⚠️ No START block found for execution\n");
    return executionOrder;
  }

  std::set<int> visited; // Prevent infinite loops
  int maxSteps = static_cast<int>(m_programBlocks.size()) * 2; // Safety limit
  int steps = 0;

  while (currentBlock && visited.find(currentBlock->id) == visited.end() && steps < maxSteps) {
    visited.insert(currentBlock->id);
    executionOrder.push_back(currentBlock);
    steps++;

    if (currentBlock->type == BlockType::END) {
      break;
    }

    // Find next block (follow first output connection)
    MachineBlock* nextBlock = nullptr;
    if (!currentBlock->outputConnections.empty()) {
      int nextId = currentBlock->outputConnections[0];

      // Safely find the next block
      for (const auto& block : m_programBlocks) {
        if (block->id == nextId) {
          nextBlock = block.get();
          break;
        }
      }

      if (!nextBlock) {
        printf("⚠️ Warning: Connected block ID %d not found! Connection may be stale.\n", nextId);
        break;
      }
    }
    else {
      printf("⚠️ Warning: Block %s (ID: %d) has no output connections\n",
        currentBlock->label.c_str(), currentBlock->id);
      break;
    }

    currentBlock = nextBlock;
  }

  if (steps >= maxSteps) {
    printf("⚠️ Warning: Execution stopped due to safety limit (possible infinite loop)\n");
  }

  return executionOrder;
}

// Update the existing ExecuteProgram method to redirect to sequence execution
void MachineBlockUI::ExecuteProgram() {
  if (m_machineOps) {
    // Use the new sequence-based execution
    ExecuteProgramAsSequence();
  }
  else {
    // Fall back to the original debug-only execution
    ExecuteProgramDebugOnly();
  }
}

// Utility methods
std::string MachineBlockUI::BlockTypeToString(BlockType type) const {
  switch (type) {
  case BlockType::START: return "START";
  case BlockType::END: return "END";
  case BlockType::MOVE_NODE: return "Move Node";
  case BlockType::WAIT: return "Wait";
  case BlockType::SET_OUTPUT: return "Set Output";
  case BlockType::CLEAR_OUTPUT: return "Clear Output";
  default: return "Unknown";
  }
}

ImU32 MachineBlockUI::GetBlockColor(BlockType type) const {
  switch (type) {
  case BlockType::START: return START_COLOR;
  case BlockType::END: return END_COLOR;
  case BlockType::MOVE_NODE: return MOVE_NODE_COLOR;
  case BlockType::WAIT: return WAIT_COLOR;
  case BlockType::SET_OUTPUT: return SET_OUTPUT_COLOR;
  case BlockType::CLEAR_OUTPUT: return CLEAR_OUTPUT_COLOR;
  default: return IM_COL32(128, 128, 128, 255);
  }
}

// Quick actions
void MachineBlockUI::QuickStart() {
  // Add START block if none exists
  if (CountBlocksOfType(BlockType::START) == 0) {
    AddBlockToProgram(BlockType::START, ImVec2(50, 50));
  }

  // Add END block if none exists
  if (CountBlocksOfType(BlockType::END) == 0) {
    AddBlockToProgram(BlockType::END, ImVec2(50, 200));
  }

  printf("🚀 Quick Start: Added essential START/END blocks\n");
}

void MachineBlockUI::ClearAll() {
  m_programBlocks.clear();
  m_connections.clear();
  m_selectedBlock = nullptr;
  m_isConnecting = false;
  m_connectionStart = nullptr;
  m_isDragging = false;
  m_draggedBlock = nullptr;

  printf("🧹 Cleared all blocks and connections\n");
}

// File operations (placeholder implementations)
void MachineBlockUI::SaveProgram() {
  if (!ValidateProgram()) {
    printf("⚠️ Warning: Saving invalid program\n");
  }

  printf("💾 Saving program with %zu blocks and %zu connections\n",
    m_programBlocks.size(), m_connections.size());
  // TODO: Implement actual file saving
}

void MachineBlockUI::LoadProgram() {
  printf("📁 Loading program...\n");
  // TODO: Implement actual file loading
}



void MachineBlockUI::ExecuteProgramAsSequence() {
  ExecuteProgramAsSequence([this](bool success) {
    m_isExecuting = false;
    m_executionStatus = success ? "Completed Successfully" : "Execution Failed";
    printf("%s\n", m_executionStatus.c_str());
  });
}

void MachineBlockUI::ExecuteProgramAsSequence(std::function<void(bool)> onComplete) {
  if (!m_machineOps) {
    printf("❌ Cannot execute: MachineOperations not set!\n");
    printf("   Call SetMachineOperations() first.\n");
    if (onComplete) onComplete(false);
    return;
  }

  if (m_isExecuting) {
    printf("⚠️ Execution already in progress!\n");
    if (onComplete) onComplete(false);
    return;
  }

  if (!ValidateProgram()) {
    printf("❌ Cannot execute: Program is invalid!\n");
    printf("   - Ensure you have exactly one START block\n");
    printf("   - Ensure you have at least one END block\n");
    printf("   - Connect blocks to create a valid flow\n");
    if (onComplete) onComplete(false);
    return;
  }

  auto executionOrder = GetExecutionOrder();
  if (executionOrder.empty()) {
    printf("❌ No execution path found! Make sure START block is connected.\n");
    if (onComplete) onComplete(false);
    return;
  }

  // Convert blocks to sequence operations
  BlockSequenceConverter converter(*m_machineOps);

  // Get program name from START block for sequence naming
  std::string programName = "Block Program";
  MachineBlock* startBlock = FindStartBlock();
  if (startBlock) {
    for (const auto& param : startBlock->parameters) {
      if (param.name == "program_name" && !param.value.empty()) {
        programName = param.value;
        break;
      }
    }
  }

  m_currentSequence = converter.ConvertBlocksToSequence(executionOrder, programName);

  if (!m_currentSequence) {
    printf("❌ Failed to convert blocks to sequence!\n");
    if (onComplete) onComplete(false);
    return;
  }

  // Set up execution
  m_isExecuting = true;
  m_executionStatus = "Executing...";

  printf("\n🚀 EXECUTING BLOCK PROGRAM AS SEQUENCE:\n");
  printf("========================================\n");
  printf("Program: %s\n", programName.c_str());
  printf("Blocks: %zu operations\n", executionOrder.size());
  printf("========================================\n");

  // Set completion callback for the sequence
  m_currentSequence->SetCompletionCallback([this, onComplete](bool success) {
    m_isExecuting = false;
    m_executionStatus = success ? "Completed Successfully" : "Execution Failed";

    printf("\n========================================\n");
    printf("%s\n", m_executionStatus.c_str());
    printf("========================================\n");

    if (onComplete) {
      onComplete(success);
    }
  });

  // Execute the sequence asynchronously
  std::thread executionThread([this]() {
    bool success = m_currentSequence->Execute();
    // The completion callback will be called automatically by SequenceStep
  });

  executionThread.detach(); // Let it run independently
}



// Rename the original method for debug purposes
void MachineBlockUI::ExecuteProgramDebugOnly() {
  if (!ValidateProgram()) {
    printf("❌ Cannot execute: Program is invalid!\n");
    printf("   - Ensure you have exactly one START block\n");
    printf("   - Ensure you have at least one END block\n");
    printf("   - Connect blocks to create a valid flow\n");
    return;
  }

  auto executionOrder = GetExecutionOrder();

  if (executionOrder.empty()) {
    printf("❌ No execution path found! Make sure START block is connected.\n");
    return;
  }

  printf("\n🚀 DEBUG MODE - SIMULATING PROGRAM EXECUTION:\n");
  printf("========================================\n");

  for (size_t i = 0; i < executionOrder.size(); i++) {
    MachineBlock* block = executionOrder[i];
    printf("%zu. [%s] %s (ID: %d)\n",
      i + 1,
      BlockTypeToString(block->type).c_str(),
      block->label.c_str(),
      block->id);

    // Print key parameters for debugging
    for (const auto& param : block->parameters) {
      if (param.name == "device_name" || param.name == "node_id" ||
        param.name == "milliseconds" || param.name == "program_name") {
        printf("   %s = %s\n", param.name.c_str(), param.value.c_str());
      }
    }
  }

  printf("========================================\n");
  printf("✅ Debug simulation completed! (%zu blocks)\n", executionOrder.size());
  printf("💡 To execute for real, call SetMachineOperations() first.\n\n");
}


void MachineBlockUI::RenderExecutionStatus() {
  if (ImGui::CollapsingHeader("Execution Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Status: %s", m_executionStatus.c_str());

    if (m_isExecuting) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(Running...)");
    }

    ImGui::Separator();

    // Execution buttons
    if (m_machineOps) {
      if (!m_isExecuting) {
        if (ImGui::Button("Execute Program")) {
          ExecuteProgramAsSequence();
        }
        ImGui::SameLine();
        if (ImGui::Button("Debug Simulate")) {
          ExecuteProgramDebugOnly();
        }
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Button("Executing...");
        ImGui::PopStyleColor();
      }
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
      ImGui::Button("Execute Program");
      ImGui::PopStyleColor();

      ImGui::SameLine();
      if (ImGui::Button("Debug Simulate")) {
        ExecuteProgramDebugOnly();
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Set MachineOperations to enable real execution");
      }
    }
  }
}

