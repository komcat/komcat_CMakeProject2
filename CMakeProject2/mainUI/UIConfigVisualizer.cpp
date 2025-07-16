#include "include/motions/MotionTypes.h"
#include "include/motions/MotionConfigManager.h"
#include "UIConfigVisualizer.h"
#include "NodePropertiesHandler.h"  // Add this include
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include "include/camera/CameraManager.h"  // Add this include for camera manager

// Add to top of UIConfigVisualizer.cpp (after includes):
extern CameraManager g_cameraManager;


UIConfigVisualizer::UIConfigVisualizer(MotionConfigManager& configManager)
	: configManager(configManager)
{
	m_logger = Logger::GetInstance();

	// Initialize action handlers (keeping existing signature)
	m_propertiesHandler = std::make_unique<NodePropertiesHandler>(configManager, m_logger);

	// Initialize camera feed display
	m_cameraFeedDisplay = std::make_unique<CameraFeedDisplay>();

	// Try to get first available camera from global manager
	auto cameraIds = g_cameraManager.GetCameraIds();
	if (!cameraIds.empty()) {
		m_selectedCameraId = cameraIds[0];
		InitializeCameraFeed();
	}
}



// UIConfigVisualizer.cpp - Add new method:

void UIConfigVisualizer::InitializeCameraFeed() {
	if (m_selectedCameraId.empty()) {
		return;
	}

	PylonCameraTest* camera = g_cameraManager.GetCamera(m_selectedCameraId);
	if (camera) {
		m_cameraFeedDisplay->SetPylonCameraSource(camera);
		m_cameraInitialized = true;
		m_logger->LogInfo("Camera feed initialized for: " + m_selectedCameraId);
	}
}


UIConfigVisualizer::~UIConfigVisualizer() {
	m_logger->LogInfo("UIConfigVisualizer destroyed");
}

void UIConfigVisualizer::RenderUI() {
	if (!showWindow) return;

	// Render graph controls at the top
	RenderGraphControls();

	ImGui::Separator();

	// Display instructions
	ImGui::Text("Drag nodes to reposition them. Positions will be saved automatically.");
	ImGui::Text("Use middle mouse button to pan, mouse wheel to zoom.");

	ImGui::Separator();

	// NEW LAYOUT: Left panel (25%) + Right canvas (75%)
	ImVec2 contentSize = ImGui::GetContentRegionAvail();
	float leftPanelWidth = contentSize.x * 0.25f;
	float canvasWidth = contentSize.x * 0.75f;

	// Left Panel - Node Actions and Information (25% width)
	ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, contentSize.y), true);
	RenderLeftPanel();
	ImGui::EndChild();

	ImGui::SameLine();

	// Right Panel - Graph Canvas (75% width)
	ImGui::BeginChild("RightPanel", ImVec2(canvasWidth, contentSize.y), false);
	RenderGraphCanvas();
	ImGui::EndChild();

	// Render properties dialog (this renders on top of everything)
	if (m_propertiesHandler) {
		m_propertiesHandler->RenderPropertiesDialog();
	}
}

void UIConfigVisualizer::RenderLeftPanel() {
	ImVec2 leftPanelSize = ImGui::GetContentRegionAvail();

	// Calculate camera canvas size first based on aspect ratio
	const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 1.25
	float availableWidth = leftPanelSize.x;
	float cameraCanvasHeight = availableWidth / CAMERA_ASPECT_RATIO + 80.0f; // +80 for dropdown and spacing

	// Node info gets the remaining space
	float nodeInfoHeight = leftPanelSize.y - cameraCanvasHeight - 10.0f; // 10px spacing between sections

	// Ensure minimum height for node info section
	if (nodeInfoHeight < 100.0f) {
		nodeInfoHeight = 100.0f;
		cameraCanvasHeight = leftPanelSize.y - nodeInfoHeight - 10.0f;
	}

	// === NODE INFORMATION SECTION (TOP - DYNAMIC HEIGHT) ===
	ImGui::BeginChild("NodeInfoSection", ImVec2(0, nodeInfoHeight), true);

	ImGui::Text("Node Information");
	ImGui::Separator();

	// Show node information and actions if a node is selected
	if (!m_selectedNodeId.empty()) {
		// Get node information for display
		auto graphOpt = configManager.GetGraph(m_activeGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();

			// Find the selected node
			const Node* selectedNode = nullptr;
			for (const auto& node : graph.Nodes) {
				if (node.Id == m_selectedNodeId) {
					selectedNode = &node;
					break;
				}
			}

			if (selectedNode) {
				// Display selected node information
				ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Selected Node:");
				ImGui::Text("ID: %s", selectedNode->Id.c_str());

				if (!selectedNode->Label.empty()) {
					ImGui::Text("Label: %s", selectedNode->Label.c_str());
				}

				ImGui::Text("Device: %s", selectedNode->Device.c_str());
				ImGui::Text("Position: %s", selectedNode->Position.c_str());
				ImGui::Text("Graph Pos: (%d, %d)", selectedNode->X, selectedNode->Y);

				// Show position coordinates directly in the panel
				ImGui::Separator();
				ImGui::Text("Position Coordinates:");

				// Get the position data if available
				if (!selectedNode->Device.empty() && !selectedNode->Position.empty()) {
					auto positionOpt = configManager.GetNamedPosition(selectedNode->Device, selectedNode->Position);
					if (positionOpt.has_value()) {
						const auto& position = positionOpt.value().get();

						// Display coordinates with proper formatting
						ImGui::Text("X: %.6f", position.x);
						ImGui::Text("Y: %.6f", position.y);
						ImGui::Text("Z: %.6f", position.z);

						// Show U, V, W for hex devices
						if (selectedNode->Device.find("hex") != std::string::npos) {
							ImGui::Text("U: %.6f", position.u);
							ImGui::Text("V: %.6f", position.v);
							ImGui::Text("W: %.6f", position.w);
						}
					}
					else {
						ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Position data not found");
					}
				}
				else {
					ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "No device/position assigned");
				}

				ImGui::Separator();
				ImGui::Text("Actions:");

				// Only the Move Device To Node button
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

				if (ImGui::Button("-> Move Device To Node", ImVec2(-1, 30))) {
					m_logger->LogInfo(">>> MoveDeviceToNode SELECTED for node: " + m_selectedNodeId +
						" (Device: " + selectedNode->Device +
						", Position: " + selectedNode->Position + ")");
				}

				ImGui::PopStyleColor(3);
			}
		}
	}
	else {
		// No node selected - show placeholder
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No node selected");
		ImGui::Spacing();
		ImGui::TextWrapped("Click on a node in the graph to view its information and coordinates.");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Graph Statistics:");

		// Show some graph statistics
		auto graphOpt = configManager.GetGraph(m_activeGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();
			ImGui::Text("Nodes: %zu", graph.Nodes.size());
			ImGui::Text("Edges: %zu", graph.Edges.size());
		}
	}

	ImGui::EndChild();

	// === CAMERA CANVAS SECTION (BOTTOM - FIXED POSITION) ===
	ImGui::Spacing();
	RenderCameraCanvas(cameraCanvasHeight);
}





// UIConfigVisualizer.cpp - Replace RenderCameraCanvas method:

void UIConfigVisualizer::RenderCameraCanvas(float height) {
	static bool cameraExpanded = true;

	if (ImGui::CollapsingHeader("Camera / Image Feed", ImGuiTreeNodeFlags_DefaultOpen)) {
		cameraExpanded = true;

		// Camera/Image source selection dropdown
		static int selectedSource = 0;
		const char* sources[] = {
			"Live Camera Feed",
			"Node Reference Image",
			"Saved Image 1",
			"Saved Image 2"
		};

		ImGui::Text("Source:");
		ImGui::SetNextItemWidth(-1);
		if (ImGui::Combo("##CameraSource", &selectedSource, sources, IM_ARRAYSIZE(sources))) {
			m_logger->LogInfo("Camera source changed to: " + std::string(sources[selectedSource]));
		}

		// Camera selection dropdown (only show when Live Camera Feed is selected)
		if (selectedSource == 0) {
			ImGui::Spacing();
			ImGui::Text("Camera:");

			auto cameraIds = g_cameraManager.GetCameraIds();
			if (!cameraIds.empty()) {
				// Find current selection index
				int currentCameraIndex = 0;
				for (size_t i = 0; i < cameraIds.size(); i++) {
					if (cameraIds[i] == m_selectedCameraId) {
						currentCameraIndex = (int)i;
						break;
					}
				}

				// Create camera selection array
				std::vector<const char*> cameraNames;
				for (const auto& id : cameraIds) {
					cameraNames.push_back(id.c_str());
				}

				ImGui::SetNextItemWidth(-1);
				if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
					m_selectedCameraId = cameraIds[currentCameraIndex];
					InitializeCameraFeed();
					m_logger->LogInfo("Camera selection changed to: " + m_selectedCameraId);
				}
			}
			else {
				ImGui::Text("No cameras available");
			}

			// Camera controls
			if (!m_selectedCameraId.empty()) {
				ImGui::Spacing();
				PylonCameraTest* camera = g_cameraManager.GetCamera(m_selectedCameraId);
				if (camera) {
					auto& pylonCamera = camera->GetCamera();

					// Connection status
					ImGui::Text("Status: %s", pylonCamera.IsConnected() ? "Connected" : "Disconnected");

					if (pylonCamera.IsConnected()) {
						ImGui::SameLine();
						ImGui::Text("| Grabbing: %s", pylonCamera.IsGrabbing() ? "Yes" : "No");

						// Start/Stop grabbing controls
						if (!pylonCamera.IsGrabbing()) {
							if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
								g_cameraManager.StartGrabbing(m_selectedCameraId);
								m_logger->LogInfo("Started grabbing for camera: " + m_selectedCameraId);
							}
						}
						else {
							if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
								g_cameraManager.StopGrabbing(m_selectedCameraId);
								m_logger->LogInfo("Stopped grabbing for camera: " + m_selectedCameraId);
							}
						}

						ImGui::SameLine();
						if (ImGui::Button("Connect/Reconnect", ImVec2(120, 25))) {
							g_cameraManager.ConnectCamera(m_selectedCameraId);
							m_logger->LogInfo("Reconnect camera: " + m_selectedCameraId);
						}
					}
					else {
						if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
							g_cameraManager.ConnectCamera(m_selectedCameraId);
							InitializeCameraFeed();
							m_logger->LogInfo("Connecting camera: " + m_selectedCameraId);
						}
					}
				}
			}
		}

		ImGui::Spacing();

		// Camera canvas area with fixed aspect ratio
		const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 1.25 (5:4)

		float availableWidth = ImGui::GetContentRegionAvail().x;
		float availableHeight = height - 120.0f; // Reserve more space for controls

		// Calculate canvas size maintaining aspect ratio
		ImVec2 canvasSize;
		canvasSize.x = availableWidth;
		canvasSize.y = availableWidth / CAMERA_ASPECT_RATIO;

		// Ensure minimum size
		if (canvasSize.x < 160.0f) {
			canvasSize.x = 160.0f;
			canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO;
		}

		// Create camera canvas
		ImGui::BeginChild("CameraCanvas", canvasSize, false, ImGuiWindowFlags_NoScrollbar);

		if (selectedSource == 0) { // Live Camera Feed
			if (m_cameraFeedDisplay && m_cameraInitialized) {
				// Render live camera feed
				m_cameraFeedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
			}
			else {
				// Show placeholder when no camera feed
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 canvasPos = ImGui::GetCursorScreenPos();
				ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

				// Background
				drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));
				drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

				// Status text
				std::string statusText = "Live Camera Feed\n(No camera initialized)";
				if (!m_selectedCameraId.empty()) {
					statusText += "\nCamera: " + m_selectedCameraId;
				}

				ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
				ImVec2 textPos = ImVec2(
					canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
					canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
				);
				drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), statusText.c_str());
			}
		}
		else {
			// Show placeholder for other sources (existing code)
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 canvasPos = ImGui::GetCursorScreenPos();
			ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

			drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));
			drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

			std::string placeholderText = std::string(sources[selectedSource]) + "\n(Coming Soon)\n" +
				"Resolution: " + std::to_string((int)canvasSize.x) + " x " + std::to_string((int)canvasSize.y) +
				"\nAspect Ratio: 5:4 (1280x1024)";

			ImVec2 textSize = ImGui::CalcTextSize(placeholderText.c_str());
			ImVec2 textPos = ImVec2(
				canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
				canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
			);
			drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), placeholderText.c_str());
		}

		ImGui::EndChild();
	}
	else {
		cameraExpanded = false;
	}
}

void UIConfigVisualizer::RenderGraphControls() {
	// Graph selection dropdown
	const auto& allGraphs = configManager.GetAllGraphs();
	if (ImGui::BeginCombo("Select Graph", m_activeGraph.c_str())) {
		for (const auto& [graphName, graph] : allGraphs) {
			bool isSelected = (m_activeGraph == graphName);
			if (ImGui::Selectable(graphName.c_str(), isSelected)) {
				SetActiveGraph(graphName);
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();

	// Zoom controls
	if (ImGui::Button("Zoom In")) {
		m_zoomLevel = (std::min)(m_zoomLevel * 1.2f, 3.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Zoom Out")) {
		m_zoomLevel = (std::max)(m_zoomLevel / 1.2f, 0.3f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset View")) {
		m_zoomLevel = 1.0f;
		m_panOffset = ImVec2(0, 0);
	}

	// Display current zoom level
	ImGui::SameLine();
	ImGui::Text("Zoom: %.1f%%", m_zoomLevel * 100.0f);
}

void UIConfigVisualizer::RenderGraphCanvas() {
	// Calculate canvas size - it should fill the remaining space in the right panel
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();

	// Ensure we have at least some space to draw
	if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
	if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

	// Create a child frame for the canvas
	ImGui::BeginChildFrame(ImGui::GetID("GraphCanvas"), canvasSize,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

	// Get the canvas position for coordinate calculations
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();

	// Track if the canvas is hovered
	m_isCanvasHovered = ImGui::IsWindowHovered();

	// Get the draw list for custom rendering
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Handle input first
	HandleInput(canvasPos, canvasSize);

	// Render the graph
	RenderBackground(drawList, canvasPos, canvasSize);

	// Only draw graph content if we have a selected graph
	if (!m_activeGraph.empty()) {
		RenderEdges(drawList, canvasPos);
		RenderNodes(drawList, canvasPos);
	}
	else {
		// Show message when no graph is selected
		ImVec2 textSize = ImGui::CalcTextSize("No graph selected");
		ImVec2 textPos = ImVec2(
			canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
			canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
		);
		drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), "No graph selected");
	}

	ImGui::EndChildFrame();
}

void UIConfigVisualizer::SetActiveGraph(const std::string& graphName) {
	if (m_activeGraph != graphName) {
		m_activeGraph = graphName;
		m_zoomLevel = 1.0f;
		m_panOffset = ImVec2(0, 0);
		// Clear selection when changing graphs
		m_selectedNodeId.clear();
		m_showNodeActions = false;
		m_logger->LogInfo("Active graph set to: " + graphName);
	}
}

void UIConfigVisualizer::ToggleWindow() {
	showWindow = !showWindow;
}

// Note: Other methods are implemented in separate files:
// - Graph rendering methods in UIConfigVisualizer_Graph.cpp
// - Input handling methods in UIConfigVisualizer_Input.cpp  
// - Helper methods in UIConfigVisualizer_Helpers.cpp