#include "include/motions/MotionTypes.h"
#include "include/motions/MotionConfigManager.h"
#include "UIConfigVisualizer.h"
#include "NodePropertiesHandler.h"  // Add this include
#include "imgui.h"
#include <cmath>
#include <algorithm>

  UIConfigVisualizer::UIConfigVisualizer(MotionConfigManager & configMgr)
  : configManager(configMgr)
  , m_logger(Logger::GetInstance()) {

  m_logger->LogInfo("UIConfigVisualizer initialized");

  // Initialize action handlers
  m_propertiesHandler = std::make_unique<NodePropertiesHandler>(configManager, m_logger);

  // Set a default active graph if available
  const auto& graphs = configManager.GetAllGraphs();
  if (!graphs.empty()) {
    m_activeGraph = graphs.begin()->first;
    m_logger->LogInfo("Default active graph set to: " + m_activeGraph);
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




void UIConfigVisualizer::RenderCameraCanvas(float height) {
  // Collapsible header for camera section
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
    ImGui::SetNextItemWidth(-1); // Full width
    if (ImGui::Combo("##CameraSource", &selectedSource, sources, IM_ARRAYSIZE(sources))) {
      m_logger->LogInfo("Camera source changed to: " + std::string(sources[selectedSource]));
    }

    ImGui::Spacing();

    // Camera canvas area with fixed aspect ratio
    // Max resolution: 1280x1024, aspect ratio = 1.25 (5:4)
    const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 1.25

    float availableWidth = ImGui::GetContentRegionAvail().x;
    float availableHeight = height - 80.0f; // Reserve space for dropdown and text

    // Calculate canvas size maintaining aspect ratio - use full available width
    ImVec2 canvasSize;
    canvasSize.x = availableWidth;  // Use full width of left panel
    canvasSize.y = availableWidth / CAMERA_ASPECT_RATIO;  // Calculate height based on width

    // Note: Let the canvas use the calculated size even if it's tall
    // The collapsible header will handle scrolling if needed

    // Ensure minimum size
    if (canvasSize.x < 160.0f) { // Minimum width
      canvasSize.x = 160.0f;
      canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO; // 128px height
    }

    // Create camera canvas without border, no manual centering
    ImGui::BeginChild("CameraCanvas", canvasSize, false, ImGuiWindowFlags_NoScrollbar);

    // Get draw list and canvas position for custom drawing
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    // Draw placeholder background (gray)
    drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));

    // Draw aspect ratio indicator border (different color to show it's correct ratio)
    drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

    // Draw placeholder text with resolution info
    std::string placeholderText = std::string(sources[selectedSource]) + "\n(Coming Soon)\n" +
      "Resolution: " + std::to_string((int)canvasSize.x) + " x " + std::to_string((int)canvasSize.y) +
      "\nAspect Ratio: 5:4 (1280x1024)";

    // Split text into lines for proper centering
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : placeholderText) {
      if (c == '\n') {
        lines.push_back(currentLine);
        currentLine.clear();
      }
      else {
        currentLine += c;
      }
    }
    if (!currentLine.empty()) {
      lines.push_back(currentLine);
    }

    // Calculate total text height
    float lineHeight = ImGui::GetTextLineHeight();
    float totalTextHeight = lines.size() * lineHeight;

    // Draw each line centered
    for (size_t i = 0; i < lines.size(); i++) {
      ImVec2 textSize = ImGui::CalcTextSize(lines[i].c_str());
      ImVec2 textPos = ImVec2(
        canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
        canvasPos.y + (canvasSize.y - totalTextHeight) * 0.5f + i * lineHeight
      );

      drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), lines[i].c_str());
    }

    // TODO: This is where camera/image rendering will go
    // Future implementation:
    // - Maintain 1280x1024 aspect ratio
    // - Scale image to fit canvas while preserving aspect ratio
    // - Center image if canvas is larger than needed

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
    m_zoomLevel = std::min(m_zoomLevel * 1.2f, 3.0f);
  }
  ImGui::SameLine();
  if (ImGui::Button("Zoom Out")) {
    m_zoomLevel = std::max(m_zoomLevel / 1.2f, 0.3f);
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

void UIConfigVisualizer::SetActiveGraph(const std::string & graphName) {
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