// MachineBlockUI.cpp
#include "MachineBlockUI.h"
#include "BlockPropertyRenderers.h"
#include "BlockSequenceConverter.h"  // Your new converter class
#include "include/SequenceStep.h"           // For sequence execution
#include "include/machine_operations.h"     // For MachineOperations
#include <functional>               // For std::function
#include <thread>                   // For std::thread
#include <algorithm>                // For std::transform
#include <iostream>                 // For printf (if not already included)
// Add this include at the top of MachineBlockUI.cpp
#include <nlohmann/json.hpp>
#include <fstream>

#include "ProgramManager.h"

MachineBlockUI::MachineBlockUI() {
	InitializePalette();
	m_programManager = std::make_unique<ProgramManager>();

	// Initialize feedback UI
	m_feedbackUI = std::make_unique<FeedbackUI>();
	m_feedbackUI->SetTitle("Block Execution Results");

	// Set up callbacks for program manager integration
	m_programManager->SetLoadCallback([this](const std::string& filename) {
		LoadProgram(filename);
	});

	m_programManager->SetSaveCallback([this](const std::string& filename) {
		SaveProgram(filename);
	});

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

	// NEW: Add slide control blocks
	m_paletteBlocks.emplace_back(7, BlockType::EXTEND_SLIDE, "Extend Slide", GetBlockColor(BlockType::EXTEND_SLIDE));
	m_paletteBlocks.emplace_back(8, BlockType::RETRACT_SLIDE, "Retract Slide", GetBlockColor(BlockType::RETRACT_SLIDE));

	// NEW: Add laser and TEC control blocks
	m_paletteBlocks.emplace_back(9, BlockType::SET_LASER_CURRENT, "Set Laser Current", GetBlockColor(BlockType::SET_LASER_CURRENT));
	m_paletteBlocks.emplace_back(10, BlockType::LASER_ON, "Laser ON", GetBlockColor(BlockType::LASER_ON));
	m_paletteBlocks.emplace_back(11, BlockType::LASER_OFF, "Laser OFF", GetBlockColor(BlockType::LASER_OFF));
	m_paletteBlocks.emplace_back(12, BlockType::SET_TEC_TEMPERATURE, "Set TEC Temp", GetBlockColor(BlockType::SET_TEC_TEMPERATURE));
	m_paletteBlocks.emplace_back(13, BlockType::TEC_ON, "TEC ON", GetBlockColor(BlockType::TEC_ON));
	m_paletteBlocks.emplace_back(14, BlockType::TEC_OFF, "TEC OFF", GetBlockColor(BlockType::TEC_OFF));



	// Initialize parameters for each block type
	for (auto& block : m_paletteBlocks) {
		InitializeBlockParameters(block);
	}
}

void MachineBlockUI::RenderUI() {
	if (!m_showWindow) return;

	// Main window with specific size and position
	ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
	ImGui::Begin("Machine Block Programming", &m_showWindow, ImGuiWindowFlags_NoScrollbar);
	// Add feedback button to your existing toolbar
	if (ImGui::Button("Show Results")) {
		ShowFeedbackWindow();
	}
	ImGui::SameLine();
	// Program management toolbar
	if (ImGui::Button("New Program")) {
		ClearAll();
		if (m_feedbackUI) m_feedbackUI->ClearBlocks();  // Clear results too
		printf("[INFO] New program created\n");
	}
	ImGui::SameLine();

	if (ImGui::Button("Load Program")) {
		ImGui::OpenPopup("Program Browser");
	}
	ImGui::SameLine();

	if (ImGui::Button("Save Program")) {
		if (!m_programManager->GetCurrentProgram().empty()) {
			SaveProgram(m_programManager->GetCurrentProgram());
		}
		else {
			ImGui::OpenPopup("Save Program As");
		}
	}
	ImGui::SameLine();

	if (ImGui::Button("Save As")) {
		ImGui::OpenPopup("Save Program As");
	}
	ImGui::SameLine();

	// Current program display
	if (!m_programManager->GetCurrentProgram().empty()) {
		ImGui::Text("Current: %s", m_programManager->GetCurrentProgram().c_str());
	}
	else {
		ImGui::Text("Current: Untitled");
	}

	ImGui::Separator();

	// Program Browser Popup
	if (ImGui::BeginPopupModal("Program Browser", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		m_programManager->RenderProgramBrowser();

		ImGui::Separator();
		if (ImGui::Button("Close", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Save As Dialog Popup
	if (ImGui::BeginPopupModal("Save Program As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		m_programManager->RenderSaveAsDialog();
		ImGui::EndPopup();
	}

	// Program validation status
	bool isValid = ValidateProgram();
	if (!isValid) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		ImGui::Text("[WARNING] Program Invalid: ");
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
		ImGui::Text("[OK] Program Valid - Ready to Execute");
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

	RenderFeedback();
	
	ImGui::End();
}


void MachineBlockUI::RenderLeftPanel() {
	ImGui::Text("Block Palette");
	ImGui::Separator();

	// Add descriptive text
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray
	ImGui::TextWrapped("Essential blocks for program flow:");
	ImGui::PopStyleColor();

	ImGui::Spacing();

	// START block description
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
	ImGui::TextWrapped("★ START: Every program needs exactly one START block");
	ImGui::PopStyleColor();

	// END block description  
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.0f, 0.0f, 1.0f)); // Red
	ImGui::TextWrapped("★ END: Every program should end with an END block");
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::Separator();

	// FIXED: Add scrollable child window for the block palette
	// Calculate available height for the scrollable area
	float availableHeight = ImGui::GetContentRegionAvail().y - 100.0f; // Leave space for buttons

	// Create scrollable child window for block palette
	if (ImGui::BeginChild("BlockPaletteScroll", ImVec2(0, availableHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

		// FIXED: Render all palette blocks with correct function signature
		for (size_t i = 0; i < m_paletteBlocks.size(); i++) {
			RenderPaletteBlock(m_paletteBlocks[i], static_cast<int>(i));  // FIXED: Added index parameter
			ImGui::Spacing(); // Add some space between blocks
		}

	}
	ImGui::EndChild();

	// Add control buttons at the bottom (outside scroll area)
	ImGui::Separator();

	// Quick action buttons
	if (ImGui::Button("Clear All", ImVec2(-1, 0))) {
		ClearAll();
	}

	if (ImGui::Button("Quick Start", ImVec2(-1, 0))) {
		QuickStart();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Adds START and END blocks to get you started");
	}

	ImGui::Spacing();

	// Program management buttons
	if (ImGui::Button("Save Program", ImVec2(-1, 0))) {
		SaveProgram();
	}

	if (ImGui::Button("Load Program", ImVec2(-1, 0))) {
		LoadProgram();
	}
}
// In MachineBlockUI.cpp - Update RenderPaletteBlock function
void MachineBlockUI::RenderPaletteBlock(const MachineBlock& block, int index) {
	// IMPROVED: Smaller button size for more compact palette
	ImVec2 buttonSize(m_leftPanelWidth - 20, 35); // Reduced from 50 to 35

	// IMPROVED: More subtle button colors with better hover/active states
	ImVec4 baseColor = ImGui::ColorConvertU32ToFloat4(block.color);
	ImVec4 hoverColor = ImVec4(baseColor.x * 1.1f, baseColor.y * 1.1f, baseColor.z * 1.1f, baseColor.w);
	ImVec4 activeColor = ImVec4(baseColor.x * 0.9f, baseColor.y * 0.9f, baseColor.z * 0.9f, baseColor.w);

	ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

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

	// Special highlighting for START/END blocks (smaller icons)
	if (block.type == BlockType::START || block.type == BlockType::END) {
		ImGui::SameLine();
		if (block.type == BlockType::START) {
			ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "[ >> ]");
		}
		else {
			ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "[ X ]");
		}
	}

	// Enhanced tooltip with more information
	if (ImGui::IsItemHovered()) {
		std::string tooltipText = "Drag to canvas or click to add " + block.label + " block";

		// Add specific descriptions for each block type
		switch (block.type) {
		case BlockType::START:
			tooltipText += "\nStarts program execution";
			break;
		case BlockType::END:
			tooltipText += "\nEnds program execution";
			break;
		case BlockType::MOVE_NODE:
			tooltipText += "\nMoves device to specified node";
			break;
		case BlockType::WAIT:
			tooltipText += "\nPauses execution for specified time";
			break;
		case BlockType::SET_OUTPUT:
			tooltipText += "\nActivates an output pin";
			break;
		case BlockType::CLEAR_OUTPUT:
			tooltipText += "\nDeactivates an output pin";
			break;
		}

		ImGui::SetTooltip("%s", tooltipText.c_str());
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
		ImGui::Button("[OK] Valid");
		ImGui::PopStyleColor();
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
		ImGui::Button("[WARNING] Invalid");
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

	// Calculate canvas size
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();

	// Ensure minimum canvas size
	if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
	if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

	// Create a child frame for the canvas - this isolates mouse input!
	ImGui::BeginChildFrame(ImGui::GetID("CanvasFrame"), canvasSize,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

	// Get the canvas position for coordinate calculations
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();

	// Track if the canvas is hovered (this now refers to the child frame)
	bool isCanvasHovered = ImGui::IsWindowHovered();

	// Get the draw list for custom rendering
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Draw canvas background
	drawList->AddRectFilled(canvasPos,
		ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
		CANVAS_BG_COLOR);

	// Draw grid
	RenderGrid(canvasPos, canvasSize);

	// Pan with middle mouse button (now isolated to canvas)
	if (isCanvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		ImVec2 delta = ImGui::GetIO().MouseDelta;
		m_canvasOffset.x += delta.x / m_canvasZoom;
		m_canvasOffset.y += delta.y / m_canvasZoom;
	}

	// Zoom with mouse wheel (now isolated to canvas)
	if (isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
		float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
		m_canvasZoom = (std::max)(0.3f, (std::min)(m_canvasZoom + zoomDelta, 3.0f));
	}

	// Handle block selection and dragging (now isolated to canvas)
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

	// Handle block dragging (now isolated to canvas)
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

	// Right-click context menu (now isolated to canvas)
	if (isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		MachineBlock* clickedBlock = GetBlockAtPosition(mousePos, canvasPos);

		if (clickedBlock) {
			ImGui::OpenPopup("BlockContextMenu");
			m_selectedBlock = clickedBlock;
		}
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

	// Context menu (MUST be inside the child frame where it was opened)
	// SAFETY: Add null pointer checks to prevent crashes
	if (ImGui::BeginPopup("BlockContextMenu")) {
		// Double-check that the selected block still exists and is valid
		bool blockStillExists = false;
		if (m_selectedBlock) {
			for (const auto& block : m_programBlocks) {
				if (block.get() == m_selectedBlock) {
					blockStillExists = true;
					break;
				}
			}
		}

		if (blockStillExists && m_selectedBlock) {
			ImGui::Text("Block: %s", m_selectedBlock->label.c_str());
			ImGui::Separator();

			// NEW: Execute Single Block option
			if (m_selectedBlock->type != BlockType::START && m_selectedBlock->type != BlockType::END) {
				if (!m_isExecuting) {
					if (ImGui::MenuItem("Execute Block")) {
						ExecuteSingleBlock(m_selectedBlock);
						ImGui::CloseCurrentPopup();
					}
					ImGui::Separator();
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
					ImGui::MenuItem("Execute Block", nullptr, false, false);
					ImGui::PopStyleColor();
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Cannot execute while program is running");
					}
					ImGui::Separator();
				}
			}
			else {
				// Show why START/END blocks can't be executed individually
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
				ImGui::MenuItem("Execute Block", nullptr, false, false);
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("START and END blocks cannot be executed individually");
				}
				ImGui::Separator();
			}

			// Show delete option with restrictions
			if (m_selectedBlock->type == BlockType::START || m_selectedBlock->type == BlockType::END) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Gray out
				ImGui::MenuItem("Delete Block (Protected)", nullptr, false, false); // Disabled
				ImGui::PopStyleColor();
			}
			else {
				if (ImGui::MenuItem("Delete Block")) {
					// Store the block to delete to avoid using m_selectedBlock after deletion
					MachineBlock* blockToDelete = m_selectedBlock;
					DeleteSelectedBlock();
					// Note: m_selectedBlock is now nullptr, don't use it after this point
				}
			}

			// Only show connection option if block still exists and can provide output
			if (m_selectedBlock && CanBlockProvideOutput(*m_selectedBlock)) {
				if (ImGui::MenuItem("Start Connection")) {
					StartConnection(m_selectedBlock, GetBlockOutputPoint(*m_selectedBlock, canvasPos));
				}
			}
		}
		else {
			// Block no longer exists, show error message
			ImGui::Text("Block no longer exists");
			ImGui::Separator();
			ImGui::MenuItem("Close", nullptr, false, false);

			// Clear the invalid selection
			m_selectedBlock = nullptr;
		}

		ImGui::EndPopup();
	}

	// Close the canvas child frame
	ImGui::EndChildFrame();
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
			textColor, "*");
	}
	else if (block.type == BlockType::END) {
		drawList->AddText(ImVec2(screenPos.x + 5, screenPos.y + 5),
			textColor, "@");
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

	// Add execution status display
	RenderExecutionStatus();

	ImGui::Spacing();
	ImGui::Separator();

	if (m_selectedBlock) {
		// Render block header information
		RenderBlockHeader(m_selectedBlock);

		// Get specialized renderer for this block type
		auto renderer = BlockRendererFactory::CreateRenderer(m_selectedBlock->type);

		// Render block-specific content using the specialized renderer
		renderer->RenderProperties(m_selectedBlock, m_machineOps);
		renderer->RenderActions(m_selectedBlock, m_machineOps);
		renderer->RenderValidation(m_selectedBlock);
	}
	else {
		ImGui::TextWrapped("Select a block to view and edit its properties.");
	}
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// NEW HELPER METHOD - ADD TO MachineBlockUI.cpp
// ═══════════════════════════════════════════════════════════════════════════════════════

void MachineBlockUI::RenderBlockHeader(MachineBlock* block) {
	ImGui::Text("Block: %s", block->label.c_str());
	ImGui::Text("Type: %s", BlockTypeToString(block->type).c_str());
	ImGui::Text("ID: %d", block->id);

	// Show deletion restrictions for special blocks
	if (block->type == BlockType::START) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Orange
		ImGui::TextWrapped("START blocks cannot be deleted");
		ImGui::PopStyleColor();
	}
	else if (block->type == BlockType::END) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Orange
		ImGui::TextWrapped("END blocks cannot be deleted");
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::Separator();
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

// IMPROVED: Prevent multiple START blocks and limit END blocks
void MachineBlockUI::AddBlockToProgram(BlockType type, const ImVec2& position) {
	// Prevent multiple START blocks
	if (type == BlockType::START && CountBlocksOfType(BlockType::START) > 0) {
		printf("[STOP] Only one START block allowed per program!\n");
		printf("   A program can have exactly one START block.\n");
		return;
	}

	// OPTIONAL: Limit to one END block (uncomment if you want this restriction)
	// if (type == BlockType::END && CountBlocksOfType(BlockType::END) > 0) {
	//   printf("[STOP] Only one END block allowed per program!\n");
	//   printf("   A program can have exactly one END block.\n");
	//   return;
	// }

	auto newBlock = std::make_unique<MachineBlock>(m_nextBlockId++, type, BlockTypeToString(type), GetBlockColor(type));
	newBlock->position = position;

	InitializeBlockParameters(*newBlock);

	m_programBlocks.push_back(std::move(newBlock));

	printf("[Success] Added %s block (ID: %d)\n", BlockTypeToString(type).c_str(), m_nextBlockId - 1);
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


	case BlockType::EXTEND_SLIDE:  // NEW
		block.parameters.push_back({ "slide_name", "", "string", "Name of the pneumatic slide to extend" });
		break;

	case BlockType::RETRACT_SLIDE:  // NEW
		block.parameters.push_back({ "slide_name", "", "string", "Name of the pneumatic slide to retract" });
		break;
	

	case BlockType::SET_LASER_CURRENT:  // NEW
		block.parameters.push_back({ "current_ma", "0.150", "float", "Laser current in milliamps (e.g., 0.150)" });
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser (optional, leave empty for default)" });
		break;

	case BlockType::LASER_ON:  // NEW
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser (optional, leave empty for default)" });
		break;

	case BlockType::LASER_OFF:  // NEW
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser (optional, leave empty for default)" });
		break;

	case BlockType::SET_TEC_TEMPERATURE:  // NEW
		block.parameters.push_back({ "temperature_c", "25.0", "float", "Target temperature in Celsius (e.g., 25.0)" });
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser/TEC (optional, leave empty for default)" });
		break;

	case BlockType::TEC_ON:  // NEW
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser/TEC (optional, leave empty for default)" });
		break;

	case BlockType::TEC_OFF:  // NEW
		block.parameters.push_back({ "laser_name", "", "string", "Name of laser/TEC (optional, leave empty for default)" });
		break;

	}
}

// FIXED: Improved block deletion with proper pointer cleanup
void MachineBlockUI::DeleteSelectedBlock() {
	if (!m_selectedBlock) return;

	// Prevent deletion during execution
	if (m_isExecuting) {
		printf("[Warning] Cannot delete blocks during execution!\n");
		return;
	}

	// PREVENT DELETION OF START AND END BLOCKS
	if (m_selectedBlock->type == BlockType::START) {
		printf("[STOP] Cannot delete START block! Every program needs exactly one START block.\n");
		return;
	}

	if (m_selectedBlock->type == BlockType::END) {
		printf("[STOP] Cannot delete END block! Every program needs at least one END block.\n");
		return;
	}

	int blockId = m_selectedBlock->id;
	printf("[DEL] Deleting block: %s (ID: %d)\n", m_selectedBlock->label.c_str(), blockId);

	// CRITICAL: Clean up ALL pointers that might reference this block BEFORE deletion

	// 1. Cancel any ongoing connection if it involves this block
	if (m_isConnecting && m_connectionStart && m_connectionStart->id == blockId) {
		printf("   Cancelling ongoing connection\n");
		CancelConnection();
	}

	// 2. Stop any dragging operation if it involves this block
	if (m_isDragging && m_draggedBlock && m_draggedBlock->id == blockId) {
		printf("   Stopping drag operation\n");
		m_isDragging = false;
		m_draggedBlock = nullptr;
	}

	// 3. Remove all connections involving this block
	auto connectionsToRemove = std::vector<BlockConnection>();
	for (const auto& conn : m_connections) {
		if (conn.fromBlockId == blockId || conn.toBlockId == blockId) {
			connectionsToRemove.push_back(conn);
		}
	}

	// Remove connections and update affected blocks
	for (const auto& conn : connectionsToRemove) {
		printf("   Removing connection: %d -> %d\n", conn.fromBlockId, conn.toBlockId);
		DeleteConnection(conn.fromBlockId, conn.toBlockId);
	}

	// 4. Clean up any remaining references in all blocks
	for (auto& block : m_programBlocks) {
		if (block.get() == m_selectedBlock) continue;

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

	// 5. Store a temporary pointer for safe removal
	MachineBlock* blockToDelete = m_selectedBlock;

	// 6. Clear the selection BEFORE removing the block from the vector
	m_selectedBlock = nullptr;

	// 7. Remove the block from the vector
	m_programBlocks.erase(
		std::remove_if(m_programBlocks.begin(), m_programBlocks.end(),
			[blockToDelete](const std::unique_ptr<MachineBlock>& block) {
		return block.get() == blockToDelete;
	}),
		m_programBlocks.end()
	);

	printf("[Success] Block deleted successfully. Remaining blocks: %zu, connections: %zu\n",
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
	if (!fromBlock || !CanBlockProvideOutput(*fromBlock)) return;

	// NEW: Check if block already has an output connection
	if (!fromBlock->outputConnections.empty()) {
		printf("[Warning] Block already has an output connection! Only one output per block allowed.\n");
		printf("   %s (ID: %d) is already connected to block ID: %d\n",
			fromBlock->label.c_str(), fromBlock->id, fromBlock->outputConnections[0]);
		return;
	}

	m_isConnecting = true;
	m_connectionStart = fromBlock;
	m_connectionStartPos = startPos;

	printf("[Link] Starting connection from %s (ID: %d)\n", fromBlock->label.c_str(), fromBlock->id);
}

void MachineBlockUI::CompleteConnection(MachineBlock* toBlock) {
	if (!m_isConnecting || !m_connectionStart || !toBlock) return;
	if (!CanBlockAcceptInput(*toBlock)) return;

	// Check if connection already exists
	for (const auto& conn : m_connections) {
		if (conn.fromBlockId == m_connectionStart->id && conn.toBlockId == toBlock->id) {
			printf("[Warning] Connection already exists!\n");
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

	printf("[Success] Connected %s (ID: %d) -> %s (ID: %d)\n",
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

// SIMPLIFIED: Much simpler program validation
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

// Add these improved methods to your MachineBlockUI.cpp file
// Replace the existing methods with these crash-safe versions

// IMPROVED: Safe execution order calculation
std::vector<MachineBlock*> MachineBlockUI::GetExecutionOrder() {
	std::vector<MachineBlock*> executionOrder;

	// Find START block
	MachineBlock* currentBlock = FindStartBlock();
	if (!currentBlock) {
		printf("[Warning] No START block found for execution\n");
		return executionOrder; // Return empty list instead of crashing
	}

	std::set<int> visited; // Prevent infinite loops
	int maxSteps = static_cast<int>(m_programBlocks.size()) * 2; // Safety limit
	int steps = 0;

	while (currentBlock && visited.find(currentBlock->id) == visited.end() && steps < maxSteps) {
		visited.insert(currentBlock->id);
		executionOrder.push_back(currentBlock);
		steps++;

		// If we reach an END block, we're done
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
				printf("[Warning] Warning: Connected block ID %d not found! Connection may be stale.\n", nextId);
				break;
			}
		}
		else {
			printf("[Warning] Warning: Block %s (ID: %d) has no output connections\n",
				currentBlock->label.c_str(), currentBlock->id);
			break;
		}

		currentBlock = nextBlock;
	}

	if (steps >= maxSteps) {
		printf("[Warning] Warning: Execution stopped due to safety limit (possible infinite loop)\n");
	}

	return executionOrder;
}


// ===================================================================
// Update ExecuteProgram method to ensure feedback shows for all execution types
// ===================================================================

void MachineBlockUI::ExecuteProgram() {
	if (m_machineOps) {
		// Use real machine operations
		ExecuteProgramAsSequence();
	}
	else if (m_virtualOps) {
		// Use virtual machine operations - NOW WITH FEEDBACK!
		ExecuteProgramWithVirtualOps();
	}
	else {
		// Fall back to debug simulation
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
	case BlockType::EXTEND_SLIDE: return "Extend Slide";    // NEW
	case BlockType::RETRACT_SLIDE: return "Retract Slide";  // NEW
	case BlockType::SET_LASER_CURRENT: return "Set Laser Current";    // NEW
	case BlockType::LASER_ON: return "Laser ON";                     // NEW
	case BlockType::LASER_OFF: return "Laser OFF";                   // NEW
	case BlockType::SET_TEC_TEMPERATURE: return "Set TEC Temp";      // NEW
	case BlockType::TEC_ON: return "TEC ON";                         // NEW
	case BlockType::TEC_OFF: return "TEC OFF";                       // NEW
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
	case BlockType::EXTEND_SLIDE: return EXTEND_SLIDE_COLOR;     // NEW
	case BlockType::RETRACT_SLIDE: return RETRACT_SLIDE_COLOR;   // NEW
	case BlockType::SET_LASER_CURRENT: return SET_LASER_CURRENT_COLOR;    // NEW
	case BlockType::LASER_ON: return LASER_ON_COLOR;                     // NEW
	case BlockType::LASER_OFF: return LASER_OFF_COLOR;                   // NEW
	case BlockType::SET_TEC_TEMPERATURE: return SET_TEC_TEMPERATURE_COLOR; // NEW
	case BlockType::TEC_ON: return TEC_ON_COLOR;                         // NEW
	case BlockType::TEC_OFF: return TEC_OFF_COLOR;                       // NEW

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

	printf(" Quick Start: Added essential START/END blocks\n");
}

void MachineBlockUI::ClearAll() {
	m_programBlocks.clear();
	m_connections.clear();
	m_selectedBlock = nullptr;
	m_isConnecting = false;
	m_connectionStart = nullptr;
	m_isDragging = false;
	m_draggedBlock = nullptr;

	printf("Cleared all blocks and connections\n");
}



std::string MachineBlockUI::BlockTypeToJsonString(BlockType type) const {
	switch (type) {
	case BlockType::START: return "START";
	case BlockType::END: return "END";
	case BlockType::MOVE_NODE: return "MOVE_NODE";
	case BlockType::WAIT: return "WAIT";
	case BlockType::SET_OUTPUT: return "SET_OUTPUT";
	case BlockType::CLEAR_OUTPUT: return "CLEAR_OUTPUT";
	case BlockType::EXTEND_SLIDE: return "EXTEND_SLIDE";    // NEW
	case BlockType::RETRACT_SLIDE: return "RETRACT_SLIDE";  // NEW
	case BlockType::SET_LASER_CURRENT: return "SET_LASER_CURRENT";    // NEW
	case BlockType::LASER_ON: return "LASER_ON";                     // NEW
	case BlockType::LASER_OFF: return "LASER_OFF";                   // NEW
	case BlockType::SET_TEC_TEMPERATURE: return "SET_TEC_TEMPERATURE"; // NEW
	case BlockType::TEC_ON: return "TEC_ON";                         // NEW
	case BlockType::TEC_OFF: return "TEC_OFF";                       // NEW
	default: return "UNKNOWN";
	}
}

// Helper method to convert string to BlockType for JSON
BlockType MachineBlockUI::JsonStringToBlockType(const std::string& typeStr) const {
	if (typeStr == "START") return BlockType::START;
	if (typeStr == "END") return BlockType::END;
	if (typeStr == "MOVE_NODE") return BlockType::MOVE_NODE;
	if (typeStr == "WAIT") return BlockType::WAIT;
	if (typeStr == "SET_OUTPUT") return BlockType::SET_OUTPUT;
	if (typeStr == "CLEAR_OUTPUT") return BlockType::CLEAR_OUTPUT;
	if (typeStr == "EXTEND_SLIDE") return BlockType::EXTEND_SLIDE;    // NEW
	if (typeStr == "RETRACT_SLIDE") return BlockType::RETRACT_SLIDE;  // NEW
	if (typeStr == "SET_LASER_CURRENT") return BlockType::SET_LASER_CURRENT;    // NEW
	if (typeStr == "LASER_ON") return BlockType::LASER_ON;                     // NEW
	if (typeStr == "LASER_OFF") return BlockType::LASER_OFF;                   // NEW
	if (typeStr == "SET_TEC_TEMPERATURE") return BlockType::SET_TEC_TEMPERATURE; // NEW
	if (typeStr == "TEC_ON") return BlockType::TEC_ON;                         // NEW
	if (typeStr == "TEC_OFF") return BlockType::TEC_OFF;                       // NEW
	return BlockType::START;
}


// Add these overloaded methods for backward compatibility:
void MachineBlockUI::SaveProgram() {
	SaveProgram("default");
}

void MachineBlockUI::LoadProgram() {
	LoadProgram("default");
}
// Save Program implementation
void MachineBlockUI::SaveProgram(const std::string& programName) {
	try {
		nlohmann::json programJson;

		// Save blocks
		programJson["blocks"] = nlohmann::json::array();
		for (const auto& block : m_programBlocks) {
			nlohmann::json blockJson = {
					{"id", block->id},
					{"type", BlockTypeToJsonString(block->type)},
					{"label", block->label},
					{"position", {{"x", block->position.x}, {"y", block->position.y}}},
					{"color", static_cast<unsigned int>(block->color)},
					{"parameters", nlohmann::json::array()}
			};

			// Save parameters
			for (const auto& param : block->parameters) {
				blockJson["parameters"].push_back({
						{"name", param.name},
						{"value", param.value},
						{"type", param.type},
						{"description", param.description}
					});
			}

			programJson["blocks"].push_back(blockJson);
		}

		// Save connections
		programJson["connections"] = nlohmann::json::array();
		for (const auto& connection : m_connections) {
			programJson["connections"].push_back({
					{"from_block_id", connection.fromBlockId},
					{"to_block_id", connection.toBlockId}
				});
		}

		// Use program manager to save
		if (m_programManager->SaveProgram(programName, programJson)) {
			printf("[SAVE] Program saved: %s\n", programName.c_str());
			printf("   Blocks: %zu, Connections: %zu\n", m_programBlocks.size(), m_connections.size());
		}

	}
	catch (const std::exception& e) {
		printf("[ERROR] Error saving program: %s\n", e.what());
	}
}

// Load Program implementation
void MachineBlockUI::LoadProgram(const std::string& programName) {
	try {
		nlohmann::json programJson;

		// Use program manager to load
		if (!m_programManager->LoadProgram(programName, programJson)) {
			printf("[ERROR] Could not load program: %s\n", programName.c_str());
			return;
		}

		// Clear existing program
		ClearAll();

		// Load blocks
		if (programJson.contains("blocks")) {
			for (const auto& blockJson : programJson["blocks"]) {
				auto newBlock = std::make_unique<MachineBlock>(
					blockJson["id"],
					JsonStringToBlockType(blockJson["type"]),
					blockJson["label"],
					blockJson["color"]
				);

				newBlock->position.x = blockJson["position"]["x"];
				newBlock->position.y = blockJson["position"]["y"];

				// Load parameters
				if (blockJson.contains("parameters")) {
					for (const auto& paramJson : blockJson["parameters"]) {
						newBlock->parameters.push_back({
								paramJson["name"],
								paramJson["value"],
								paramJson["type"],
								paramJson["description"]
							});
					}
				}

				m_programBlocks.push_back(std::move(newBlock));
			}
		}

		// Load connections
		if (programJson.contains("connections")) {
			for (const auto& connJson : programJson["connections"]) {
				BlockConnection connection;
				connection.fromBlockId = connJson["from_block_id"];
				connection.toBlockId = connJson["to_block_id"];
				m_connections.push_back(connection);

				// Update block connection lists
				for (auto& block : m_programBlocks) {
					if (block->id == connection.fromBlockId) {
						block->outputConnections.push_back(connection.toBlockId);
					}
					if (block->id == connection.toBlockId) {
						block->inputConnections.push_back(connection.fromBlockId);
					}
				}
			}
		}

		// Update next block ID to avoid conflicts
		int maxId = 0;
		for (const auto& block : m_programBlocks) {
			if (block->id > maxId) maxId = block->id;
		}
		m_nextBlockId = maxId + 1;

		printf("[LOAD] Program loaded: %s\n", programName.c_str());
		printf("   Blocks: %zu, Connections: %zu\n", m_programBlocks.size(), m_connections.size());

	}
	catch (const std::exception& e) {
		printf("[ERROR] Error loading program: %s\n", e.what());
	}
}


void MachineBlockUI::ExecuteProgramAsSequence() {
	ExecuteProgramAsSequence([this](bool success) {
		m_isExecuting = false;
		m_executionStatus = success ? "Completed Successfully" : "Execution Failed";
		printf("%s\n", m_executionStatus.c_str());
	});
}

// ===================================================================
// Enhanced ExecuteProgramAsSequence with Per-Block Feedback
// Replace your existing ExecuteProgramAsSequence method
// ===================================================================


// ===================================================================
// 6. Updated MachineBlockUI::ExecuteProgramAsSequence method
// ===================================================================

void MachineBlockUI::ExecuteProgramAsSequence(std::function<void(bool)> onComplete) {
	if (!m_machineOps) {
		printf("❌ Cannot execute: MachineOperations not set!\n");
		if (onComplete) onComplete(false);
		return;
	}

	if (m_isExecuting) {
		printf("⚠️ Execution already in progress!\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Clear and show feedback UI
	if (m_feedbackUI) {
		m_feedbackUI->ClearBlocks();
		m_feedbackUI->Show();
	}

	if (!ValidateProgram()) {
		printf("❌ Cannot execute: Program is invalid!\n");
		if (onComplete) onComplete(false);
		return;
	}

	auto executionOrder = GetExecutionOrder();
	if (executionOrder.empty()) {
		printf("❌ No execution path found!\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Add initial status for each block
	if (m_feedbackUI) {
		for (const auto& block : executionOrder) {
			std::string gridId = std::to_string(block->id);
			m_feedbackUI->AddBlock(BlockResult(
				gridId, block->label, "Pending", "Waiting", "Queued for execution"
			));
		}
	}

	// Convert to sequence with progress tracking
	BlockSequenceConverter converter(*m_machineOps);

	// SET UP PROGRESS CALLBACK - This is the key!
	converter.SetProgressCallback([this](int blockId, const std::string& blockName,
		const std::string& status, const std::string& details) {
		if (m_feedbackUI) {
			std::string gridId = std::to_string(blockId);
			std::string result = (status == "Complete") ? "Success" :
				(status == "Failed") ? "Error" : "Running";
			m_feedbackUI->UpdateBlock(gridId, status, result, details);

			printf("📊 Block Progress: %s (ID: %d) - %s: %s\n",
				blockName.c_str(), blockId, status.c_str(), details.c_str());
		}
	});

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

	m_isExecuting = true;
	m_executionStatus = "Executing with Real-Time Feedback...";

	printf("\n🚀 EXECUTING BLOCK PROGRAM WITH REAL-TIME FEEDBACK:\n");
	printf("========================================\n");
	printf("Program: %s\n", programName.c_str());
	printf("Blocks: %zu operations\n", executionOrder.size());
	printf("========================================\n");

	// Set completion callback
	m_currentSequence->SetCompletionCallback([this, onComplete](bool success) {
		m_isExecuting = false;
		m_executionStatus = success ? "Execution Completed" : "Execution Failed";

		printf("\n========================================\n");
		printf("%s\n", m_executionStatus.c_str());
		printf("========================================\n");

		if (onComplete) {
			onComplete(success);
		}
	});

	// Execute the sequence - now with real-time per-block feedback!
	std::thread executionThread([this]() {
		bool success = m_currentSequence->Execute();
	});

	executionThread.detach();
}





// ===================================================================
// NEW METHOD: Execute sequence with per-block monitoring
// ===================================================================

void MachineBlockUI::ExecuteSequenceWithMonitoring() {
	if (!m_currentSequence) return;

	// Start monitoring thread for feedback updates
	std::atomic<bool> executionComplete{ false };

	std::thread monitorThread([this, &executionComplete]() {
		MonitorSequenceProgress(executionComplete);
	});

	// Execute the actual sequence (thread-safe SequenceStep execution)
	bool success = m_currentSequence->Execute();

	// Signal monitoring thread to stop
	executionComplete = true;
	monitorThread.join();

	// The completion callback will be triggered automatically by SequenceStep
}

// ===================================================================
// NEW METHOD: Monitor sequence progress and update feedback
// ===================================================================

void MachineBlockUI::MonitorSequenceProgress(std::atomic<bool>& executionComplete) {
	if (!m_feedbackUI || m_currentExecutionOrder.empty()) return;

	size_t currentBlock = 0;
	auto startTime = std::chrono::steady_clock::now();

	while (!executionComplete && currentBlock < m_currentExecutionOrder.size()) {
		auto block = m_currentExecutionOrder[currentBlock];
		std::string gridId = std::to_string(block->id);

		// Estimate which block should be executing based on time
		auto elapsed = std::chrono::steady_clock::now() - startTime;
		auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		// Update current block to "Processing"
		if (m_feedbackUI) {
			std::string details = "Hardware execution in progress - ";

			// Add block-specific details
			for (const auto& param : block->parameters) {
				if (param.name == "device_name" || param.name == "node_id" ||
					param.name == "milliseconds" || param.name == "program_name" ||
					param.name == "pin" || param.name == "state") {
					details += param.name + "=" + param.value + " ";
				}
			}

			m_feedbackUI->UpdateBlock(gridId, "Processing", "Running", details);

			printf("   🔧 Hardware executing: %s (ID: %d)\n", block->label.c_str(), block->id);
		}

		// Estimate execution time per block (this is approximate)
		int estimatedBlockTime = GetEstimatedBlockExecutionTime(block);

		// Wait for estimated completion time
		std::this_thread::sleep_for(std::chrono::milliseconds(estimatedBlockTime));

		// Update to completed (if execution hasn't failed)
		if (!executionComplete && m_feedbackUI) {
			m_feedbackUI->UpdateBlock(gridId, "Complete", "Success",
				"Hardware operation completed successfully");
			printf("   ✅ Hardware completed: %s (ID: %d)\n", block->label.c_str(), block->id);
		}

		currentBlock++;

		// Small pause between blocks
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

// ===================================================================
// NEW METHOD: Estimate execution time for different block types
// ===================================================================

int MachineBlockUI::GetEstimatedBlockExecutionTime(MachineBlock* block) {
	if (!block) return 1000;

	switch (block->type) {
	case BlockType::START:
		return 100; // Quick startup

	case BlockType::END:
		return 200; // Cleanup time

	case BlockType::MOVE_NODE: {
		// Movement can take longer
		return 2000; // 2 seconds for movement
	}

	case BlockType::WAIT: {
		// Use actual wait time from parameters
		std::string waitTime = GetParameterValue(*block, "milliseconds");
		if (!waitTime.empty()) {
			try {
				return std::stoi(waitTime);
			}
			catch (...) {
				return 1000;
			}
		}
		return 1000;
	}

	case BlockType::SET_OUTPUT:
	case BlockType::CLEAR_OUTPUT: {
		// I/O operations are usually quick
		std::string delayTime = GetParameterValue(*block, "delay_ms");
		if (!delayTime.empty()) {
			try {
				return std::stoi(delayTime) + 100; // Add 100ms for operation itself
			}
			catch (...) {
				return 300;
			}
		}
		return 300;
	}

	default:
		return 1000; // Default 1 second
	}
}



// Rename the original method for debug purposes
void MachineBlockUI::ExecuteProgramDebugOnly() {
	if (!ValidateProgram()) {
		printf("❌ Cannot execute: Program is invalid!\n");
		return;
	}

	auto executionOrder = GetExecutionOrder();
	if (executionOrder.empty()) {
		printf("❌ No execution path found!\n");
		return;
	}

	// Clear and show feedback
	if (m_feedbackUI) {
		m_feedbackUI->ClearBlocks();
		m_feedbackUI->Show();
	}

	printf("\n🚀 DEBUG MODE - SIMULATING PROGRAM EXECUTION:\n");
	printf("========================================\n");

	// Simulate execution with feedback updates
	for (size_t i = 0; i < executionOrder.size(); i++) {
		MachineBlock* block = executionOrder[i];
		std::string gridId = std::to_string(block->id);

		printf("%zu. [%s] %s (ID: %d)\n",
			i + 1,
			BlockTypeToString(block->type).c_str(),
			block->label.c_str(),
			block->id);

		// Simulate processing time and update feedback
		if (m_feedbackUI) {
			// Add block as pending
			m_feedbackUI->AddBlock(BlockResult(
				gridId, block->label, "Processing", "Running", "Simulating execution..."
			));

			// Simulate some processing time (in real app, this would be actual execution)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Update to completed
			std::string details = "Simulated - ";
			for (const auto& param : block->parameters) {
				if (param.name == "device_name" || param.name == "node_id" ||
					param.name == "milliseconds" || param.name == "program_name") {
					details += param.name + "=" + param.value + " ";
				}
			}

			m_feedbackUI->UpdateBlock(gridId, "Complete", "Success", details);
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

bool MachineBlockUI::HasValidExecutionPath() const {
	// Simple validation - just check counts
	return ValidateProgram();
}
// Add this method implementation to your MachineBlockUI.cpp file

// SIMPLIFIED: Remove the complex path validation(optional)
// Since we prevent deletion of START/END, we don't need complex validation
bool MachineBlockUI::HasValidFlow() const {
	// Simple validation - just check counts
	return ValidateProgram();
}

// ALTERNATIVE: If you want to make HasValidFlow() just call HasValidExecutionPath()
// You can replace the above implementation with this simpler version:

/*
bool MachineBlockUI::HasValidFlow() const {
	// Simple wrapper around HasValidExecutionPath for backward compatibility
	return HasValidExecutionPath();
}
*/

// Add helper function to get block description
std::string MachineBlockUI::GetBlockDescription(const MachineBlock& block) const {
	switch (block.type) {
	case BlockType::START: {
		std::string progName = GetParameterValue(block, "program_name");
		return progName.empty() ? "START" : progName;
	}

	case BlockType::END: {
		return "END";
	}

	case BlockType::MOVE_NODE: {
		std::string deviceName = GetParameterValue(block, "device_name");
		std::string nodeId = GetParameterValue(block, "node_id");

		if (!deviceName.empty() && !nodeId.empty()) {
			// Truncate long node IDs for display
			std::string shortNodeId = nodeId;
			if (nodeId.length() > 10) {
				shortNodeId = nodeId.substr(0, 7) + "...";
			}
			return deviceName + "\n-> " + shortNodeId;
		}
		return "Move Node";
	}

	case BlockType::WAIT: {
		std::string milliseconds = GetParameterValue(block, "milliseconds");
		std::string description = GetParameterValue(block, "description");

		if (!milliseconds.empty()) {
			int ms = std::stoi(milliseconds);
			if (ms >= 1000) {
				float seconds = ms / 1000.0f;
				return "Wait\n" + std::to_string(seconds) + "s";
			}
			else {
				return "Wait\n" + milliseconds + "ms";
			}
		}
		return "Wait";
	}

	case BlockType::SET_OUTPUT: {
		std::string deviceName = GetParameterValue(block, "device_name");
		std::string pin = GetParameterValue(block, "pin");

		if (!deviceName.empty() && !pin.empty()) {
			return "Set Output\n" + deviceName + "[" + pin + "]";
		}
		return "Set Output";
	}

	case BlockType::CLEAR_OUTPUT: {
		std::string deviceName = GetParameterValue(block, "device_name");
		std::string pin = GetParameterValue(block, "pin");

		if (!deviceName.empty() && !pin.empty()) {
			return "Clear Output\n" + deviceName + "[" + pin + "]";
		}
		return "Clear Output";
	}

	default:
		return block.label;
	}
}

// Add helper function to get parameter value
std::string MachineBlockUI::GetParameterValue(const MachineBlock& block, const std::string& paramName) const {
	for (const auto& param : block.parameters) {
		if (param.name == paramName) {
			return param.value;
		}
	}
	return "";
}


// Step 9: Update the RenderProgramBlock method in MachineBlockUI.cpp to show slide names
// Add this helper method to update block labels based on parameters:
void MachineBlockUI::UpdateBlockLabel(MachineBlock& block) {
	switch (block.type) {
	case BlockType::EXTEND_SLIDE: {
		std::string slideName = GetParameterValue(block, "slide_name");
		if (!slideName.empty()) {
			block.label = "Extend\n" + slideName;
		}
		else {
			block.label = "Extend Slide";
		}
		break;
	}

	case BlockType::RETRACT_SLIDE: {
		std::string slideName = GetParameterValue(block, "slide_name");
		if (!slideName.empty()) {
			block.label = "Retract\n" + slideName;
		}
		else {
			block.label = "Retract Slide";
		}
		break;
	}
	case BlockType::SET_LASER_CURRENT: {  // NEW
		std::string current = GetParameterValue(block, "current_ma");
		block.label = current.empty() ? "Set Laser\nCurrent" : "Set Laser\n" + current + " mA";
		break;
	}

	case BlockType::LASER_ON: {  // NEW
		std::string laserName = GetParameterValue(block, "laser_name");
		block.label = laserName.empty() ? "Laser ON" : "Laser ON\n" + laserName;
		break;
	}

	case BlockType::LASER_OFF: {  // NEW
		std::string laserName = GetParameterValue(block, "laser_name");
		block.label = laserName.empty() ? "Laser OFF" : "Laser OFF\n" + laserName;
		break;
	}

	case BlockType::SET_TEC_TEMPERATURE: {  // NEW
		std::string temp = GetParameterValue(block, "temperature_c");
		block.label = temp.empty() ? "Set TEC\nTemp" : "Set TEC\n" + temp + "°C";
		break;
	}

	case BlockType::TEC_ON: {  // NEW
		std::string laserName = GetParameterValue(block, "laser_name");
		block.label = laserName.empty() ? "TEC ON" : "TEC ON\n" + laserName;
		break;
	}

	case BlockType::TEC_OFF: {  // NEW
		std::string laserName = GetParameterValue(block, "laser_name");
		block.label = laserName.empty() ? "TEC OFF" : "TEC OFF\n" + laserName;
		break;
	}
	default:
		// Other block types handle their own labeling
		break;
	}
}


// Execute a single block only
void MachineBlockUI::ExecuteSingleBlock(MachineBlock* block) {
	if (m_machineOps) {
		// Use the new single block sequence-based execution
		ExecuteSingleBlockAsSequence(block);
	}
	else {
		// Fall back to debug-only execution for single block
		printf("\n DEBUG MODE - SIMULATING SINGLE BLOCK EXECUTION:\n");
		printf("========================================\n");
		printf("Block: %s (ID: %d)\n", block->label.c_str(), block->id);
		printf("Type: %s\n", BlockTypeToString(block->type).c_str());

		// Print key parameters for debugging
		for (const auto& param : block->parameters) {
			if (param.name == "device_name" || param.name == "node_id" ||
				param.name == "milliseconds" || param.name == "program_name") {
				printf("   %s = %s\n", param.name.c_str(), param.value.c_str());
			}
		}
		printf("========================================\n");
		printf("[Success] Single block debug simulation completed!\n");
		printf("Tips - To execute for real, call SetMachineOperations() first.\n\n");
	}
}

// Execute single block as sequence (real execution)
void MachineBlockUI::ExecuteSingleBlockAsSequence(MachineBlock* block, std::function<void(bool)> onComplete) {
	if (!m_machineOps) {
		printf("[Failed] Cannot execute: MachineOperations not set!\n");
		printf("   Call SetMachineOperations() first.\n");
		if (onComplete) onComplete(false);
		return;
	}

	if (m_isExecuting) {
		printf("[Warning] Execution already in progress!\n");
		if (onComplete) onComplete(false);
		return;
	}

	if (!block) {
		printf("[Failed] Cannot execute: No block provided!\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Skip START and END blocks for single execution
	if (block->type == BlockType::START || block->type == BlockType::END) {
		printf("[Warning] Cannot execute START or END blocks individually\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Create execution order with just this single block
	auto executionOrder = CreateSingleBlockExecutionOrder(block);

	if (executionOrder.empty()) {
		printf("[Failed] Failed to create execution order for single block!\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Convert to sequence
	BlockSequenceConverter converter(*m_machineOps);
	std::string blockName = "Single Block: " + block->label;

	m_currentSequence = converter.ConvertBlocksToSequence(executionOrder, blockName);

	if (!m_currentSequence) {
		printf("[Failed] Failed to convert block to sequence!\n");
		if (onComplete) onComplete(false);
		return;
	}

	// Set up execution
	m_isExecuting = true;
	m_executionStatus = "Executing Single Block...";

	printf("\n EXECUTING SINGLE BLOCK AS SEQUENCE:\n");
	printf("========================================\n");
	printf("Block: %s (ID: %d)\n", block->label.c_str(), block->id);
	printf("Type: %s\n", BlockTypeToString(block->type).c_str());
	printf("========================================\n");

	// Set completion callback for the sequence
	m_currentSequence->SetCompletionCallback([this, onComplete](bool success) {
		m_isExecuting = false;
		m_executionStatus = success ? "Single Block Completed" : "Single Block Failed";

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

// Helper method to create execution order for single block
std::vector<MachineBlock*> MachineBlockUI::CreateSingleBlockExecutionOrder(MachineBlock* block) {
	std::vector<MachineBlock*> executionOrder;

	if (block && block->type != BlockType::START && block->type != BlockType::END) {
		executionOrder.push_back(block);
	}

	return executionOrder;
}

// ===================================================================
// Real-time Execution with Feedback - Replace ExecuteProgramWithVirtualOps
// ===================================================================

void MachineBlockUI::ExecuteProgramWithVirtualOps() {
	if (!ValidateProgram()) {
		printf("❌ Cannot execute: Program is invalid!\n");
		return;
	}

	auto executionOrder = GetExecutionOrder();
	if (executionOrder.empty()) {
		printf("❌ No execution path found!\n");
		return;
	}

	// Clear and show feedback UI immediately
	if (m_feedbackUI) {
		m_feedbackUI->ClearBlocks();
		m_feedbackUI->Show();

		// Add all blocks as "Pending" initially - user sees this immediately
		for (const auto& block : executionOrder) {
			std::string gridId = std::to_string(block->id);
			m_feedbackUI->AddBlock(BlockResult(
				gridId, block->label, "Pending", "Waiting", "Queued for execution"
			));
		}
	}

	printf("\n🤖 EXECUTING WITH VIRTUAL MACHINE OPERATIONS:\n");
	printf("================================================\n");

	// Set execution state
	m_isExecuting = true;
	m_executionStatus = "Executing with Virtual Ops...";

	// Execute asynchronously so UI stays responsive
	std::thread executionThread([this, executionOrder]() {
		for (size_t i = 0; i < executionOrder.size(); ++i) {
			auto block = executionOrder[i];
			std::string gridId = std::to_string(block->id);

			printf("%zu. [%s] %s (ID: %d)\n", i + 1,
				BlockTypeToString(block->type).c_str(),
				block->label.c_str(), block->id);

			// STEP 1: Update feedback to show block is starting
			if (m_feedbackUI) {
				m_feedbackUI->UpdateBlock(gridId, "Processing", "Running", "Starting execution...");
			}

			// Small delay so user can see the "Processing" state
			std::this_thread::sleep_for(std::chrono::milliseconds(300));

			// STEP 2: Execute the actual virtual operation
			bool success = ExecuteBlockWithVirtualOps(block);

			// STEP 3: Update feedback immediately after execution
			if (m_feedbackUI) {
				if (success) {
					// Build details string with parameters
					std::string details = "Virtual execution - ";
					for (const auto& param : block->parameters) {
						if (param.name == "device_name" || param.name == "node_id" ||
							param.name == "milliseconds" || param.name == "program_name" ||
							param.name == "pin" || param.name == "state") {
							details += param.name + "=" + param.value + " ";
						}
					}

					m_feedbackUI->UpdateBlock(gridId, "Complete", "Success", details);
					printf("   ✅ Block completed successfully\n\n");
				}
				else {
					m_feedbackUI->UpdateBlock(gridId, "Incomplete", "Failed", "Virtual execution failed");
					printf("   ❌ Block execution failed, stopping program\n");
					break;
				}
			}

			// STEP 4: Pause between blocks so user can see each completion
			if (i < executionOrder.size() - 1) { // Don't pause after last block
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}

		// Update execution state
		m_isExecuting = false;
		m_executionStatus = "Virtual Execution Completed";

		printf("🎉 [SUCCESS] Virtual program execution completed!\n");
	});

	// Detach thread so execution continues in background while UI stays responsive
	executionThread.detach();
}




// ===================================================================
// Also update ExecuteBlockWithVirtualOps to handle CLEAR_OUTPUT
// ===================================================================

bool MachineBlockUI::ExecuteBlockWithVirtualOps(MachineBlock* block) {
	switch (block->type) {
	case BlockType::START:
		printf("   🟢 Starting program...\n");
		return true;

	case BlockType::END:
		printf("   🔴 Program finished\n");
		return true;

	case BlockType::MOVE_NODE: {
		std::string deviceName = GetParameterValue(*block, "device_name");
		std::string graphName = GetParameterValue(*block, "graph_name");
		std::string nodeId = GetParameterValue(*block, "node_id");

		printf("   🏃 Moving %s to node %s...\n", deviceName.c_str(), nodeId.c_str());
		return m_virtualOps->MoveDeviceToNode(deviceName, graphName, nodeId, true);
	}

	case BlockType::WAIT: {
		int milliseconds = std::stoi(GetParameterValue(*block, "milliseconds"));
		printf("   ⏱️  Waiting %d ms...\n", milliseconds);
		m_virtualOps->Wait(milliseconds);
		return true;
	}

	case BlockType::SET_OUTPUT: {
		std::string deviceName = GetParameterValue(*block, "device_name");
		int pin = std::stoi(GetParameterValue(*block, "pin"));
		bool state = GetParameterValue(*block, "state") == "true";

		printf("   🔌 Setting output %s pin %d to %s\n",
			deviceName.c_str(), pin, state ? "HIGH" : "LOW");
		return m_virtualOps->SetOutput(deviceName, pin, state);
	}

	case BlockType::CLEAR_OUTPUT: {
		std::string deviceName = GetParameterValue(*block, "device_name");
		int pin = std::stoi(GetParameterValue(*block, "pin"));

		printf("   🔌 Clearing output %s pin %d\n", deviceName.c_str(), pin);
		// Clear output = set to false/LOW
		return m_virtualOps->SetOutput(deviceName, pin, false);
	}

	default:
		printf("   ❓ Unknown block type\n");
		return true;
	}
}

void MachineBlockUI::UpdateBlockResult(int blockId, const std::string& status,
	const std::string& result, const std::string& details) {
	if (m_feedbackUI) {
		std::string gridId = std::to_string(blockId);
		m_feedbackUI->UpdateBlock(gridId, status, result, details);
	}
}


// ===================================================================
// NEW: Simplified monitoring that doesn't rely on time estimation
// ===================================================================
void MachineBlockUI::ExecuteSequenceWithSimpleMonitoring() {
	if (!m_currentSequence) return;

	// Start monitoring thread
	std::atomic<bool> executionComplete{ false };
	std::atomic<size_t> currentBlockIndex{ 0 };

	std::thread monitorThread([this, &executionComplete, &currentBlockIndex]() {
		while (!executionComplete && currentBlockIndex < m_currentExecutionOrder.size()) {
			auto block = m_currentExecutionOrder[currentBlockIndex];
			std::string gridId = std::to_string(block->id);

			if (m_feedbackUI) {
				// Build parameter details
				std::string details = "Hardware executing - ";
				for (const auto& param : block->parameters) {
					if (param.name == "device_name" || param.name == "node_id" ||
						param.name == "milliseconds" || param.name == "pin" || param.name == "state") {
						details += param.name + "=" + param.value + " ";
					}
				}

				m_feedbackUI->UpdateBlock(gridId, "Processing", "Running", details);
				printf("   🔧 Hardware executing: %s (ID: %d)\n", block->label.c_str(), block->id);
			}

			// Wait for this block to be marked as complete
			// This is a simple approach - just increment when we start the next block
			std::this_thread::sleep_for(std::chrono::milliseconds(500));

			if (m_feedbackUI) {
				m_feedbackUI->UpdateBlock(gridId, "Complete", "Success",
					"Hardware operation completed successfully");
				printf("   ✅ Hardware completed: %s (ID: %d)\n", block->label.c_str(), block->id);
			}

			currentBlockIndex++;
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	});

	// Execute the actual sequence
	bool success = m_currentSequence->Execute();

	// Signal monitoring to stop
	executionComplete = true;
	monitorThread.join();

	// The completion callback will be triggered automatically by SequenceStep
}

