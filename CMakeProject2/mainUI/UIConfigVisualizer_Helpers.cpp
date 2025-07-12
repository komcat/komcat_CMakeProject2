// UIConfigVisualizer_Helpers.cpp - Helper functions implementation
#include "UIConfigVisualizer.h"
#include "imgui.h"
#include <cmath>

ImVec2 UIConfigVisualizer::GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const {
  // Convert from graph coordinates to canvas coordinates
  return ImVec2(
    canvasPos.x + (pos.x + m_panOffset.x) * m_zoomLevel,
    canvasPos.y + (pos.y + m_panOffset.y) * m_zoomLevel
  );
}

ImVec2 UIConfigVisualizer::CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const {
  // Convert from canvas coordinates to graph coordinates
  return ImVec2(
    (pos.x - canvasPos.x) / m_zoomLevel - m_panOffset.x,
    (pos.y - canvasPos.y) / m_zoomLevel - m_panOffset.y
  );
}

ImVec2 UIConfigVisualizer::GetNodePosition(const Node& node) const {
  // Get node position from its X and Y properties
  return ImVec2(static_cast<float>(node.X), static_cast<float>(node.Y));
}

void UIConfigVisualizer::SaveNodePosition(const std::string& nodeId, const ImVec2& newPos) {
  // Update graph with new node position and save to config
  auto graphOpt = configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  Graph updatedGraph = graphOpt.value().get();

  for (Node& node : updatedGraph.Nodes) {
    if (node.Id == nodeId) {
      node.X = static_cast<int>(newPos.x);
      node.Y = static_cast<int>(newPos.y);
      break;
    }
  }

  // Update the graph in the config manager
  try {
    configManager.UpdateGraph(m_activeGraph, updatedGraph);
    configManager.SaveConfig();
  }
  catch (const std::exception& e) {
    m_logger->LogError("Failed to save node position: " + std::string(e.what()));
  }
}

void UIConfigVisualizer::DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, bool isBidirectional) {
  // Draw the main line
  drawList->AddLine(start, end, color, thickness);

  // Calculate arrowhead direction
  ImVec2 dir = ImVec2(end.x - start.x, end.y - start.y);
  float length = sqrt(dir.x * dir.x + dir.y * dir.y);
  if (length < 1e-6f) return; // Skip if points are too close

  // Normalize direction
  dir.x /= length;
  dir.y /= length;

  // Calculate perpendicular vector
  ImVec2 perp = ImVec2(-dir.y, dir.x);

  // Calculate arrowhead points for end arrow
  ImVec2 endArrowP1 = ImVec2(
    end.x - dir.x * ARROW_SIZE + perp.x * ARROW_SIZE * 0.5f,
    end.y - dir.y * ARROW_SIZE + perp.y * ARROW_SIZE * 0.5f
  );

  ImVec2 endArrowP2 = ImVec2(
    end.x - dir.x * ARROW_SIZE - perp.x * ARROW_SIZE * 0.5f,
    end.y - dir.y * ARROW_SIZE - perp.y * ARROW_SIZE * 0.5f
  );

  // Draw arrowhead at end
  drawList->AddTriangleFilled(end, endArrowP1, endArrowP2, color);

  // If bidirectional, draw arrowhead at start as well
  if (isBidirectional) {
    ImVec2 startArrowP1 = ImVec2(
      start.x + dir.x * ARROW_SIZE + perp.x * ARROW_SIZE * 0.5f,
      start.y + dir.y * ARROW_SIZE + perp.y * ARROW_SIZE * 0.5f
    );

    ImVec2 startArrowP2 = ImVec2(
      start.x + dir.x * ARROW_SIZE - perp.x * ARROW_SIZE * 0.5f,
      start.y + dir.y * ARROW_SIZE - perp.y * ARROW_SIZE * 0.5f
    );

    // Draw arrowhead at start
    drawList->AddTriangleFilled(start, startArrowP1, startArrowP2, color);
  }
}

std::string UIConfigVisualizer::GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos) {
  auto graphOpt = configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return "";

  const auto& graph = graphOpt.value().get();

  // Convert mouse position to graph coordinates
  ImVec2 graphPos = CanvasToGraph(pos, canvasPos);

  // Check each node
  for (const auto& node : graph.Nodes) {
    ImVec2 nodePos = GetNodePosition(node);

    // Check if mouse position is inside the node rectangle
    if (graphPos.x >= nodePos.x - NODE_WIDTH / 2 / m_zoomLevel &&
      graphPos.x <= nodePos.x + NODE_WIDTH / 2 / m_zoomLevel &&
      graphPos.y >= nodePos.y - NODE_HEIGHT / 2 / m_zoomLevel &&
      graphPos.y <= nodePos.y + NODE_HEIGHT / 2 / m_zoomLevel) {
      return node.Id;
    }
  }

  return ""; // No node at this position
}