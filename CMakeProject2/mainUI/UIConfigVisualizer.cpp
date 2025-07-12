// UIConfigVisualizer.cpp - Main implementation
#include "UIConfigVisualizer.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

UIConfigVisualizer::UIConfigVisualizer(MotionConfigManager& configMgr)
  : configManager(configMgr)
  , m_logger(Logger::GetInstance()) {

  m_logger->LogInfo("UIConfigVisualizer initialized");

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

  // IMPORTANT: Render selected node actions BEFORE the canvas
  RenderSelectedNodeActions();

  // Render the main graph canvas
  RenderGraphCanvas();
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
  // Calculate canvas size and position
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

  // No longer need context menu popup - using selected node approach instead
}

void UIConfigVisualizer::SetActiveGraph(const std::string& graphName) {
  if (m_activeGraph != graphName) {
    m_activeGraph = graphName;
    m_zoomLevel = 1.0f;
    m_panOffset = ImVec2(0, 0);
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