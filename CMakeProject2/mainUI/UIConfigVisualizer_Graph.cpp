// UIConfigVisualizer_Graph.cpp - Graph rendering functionality (updated to remove top panel)
#include "UIConfigVisualizer.h"
#include "imgui.h"
#include <cmath>
#include <map>
#include <vector>

void UIConfigVisualizer::RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
  // Draw the canvas background
  drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
    IM_COL32(40, 40, 40, 255));

  // Draw a grid
  float gridSize = 50.0f * m_zoomLevel;
  ImU32 gridColor = IM_COL32(60, 60, 60, 200);

  // Adjust grid offset based on panning
  float offsetX = fmodf(m_panOffset.x * m_zoomLevel, gridSize);
  float offsetY = fmodf(m_panOffset.y * m_zoomLevel, gridSize);

  // Draw vertical grid lines
  for (float x = offsetX; x < canvasSize.x; x += gridSize) {
    drawList->AddLine(
      ImVec2(canvasPos.x + x, canvasPos.y),
      ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
      gridColor
    );
  }

  // Draw horizontal grid lines
  for (float y = offsetY; y < canvasSize.y; y += gridSize) {
    drawList->AddLine(
      ImVec2(canvasPos.x, canvasPos.y + y),
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
      gridColor
    );
  }
}

void UIConfigVisualizer::RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos) {
  auto graphOpt = configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  // First, get current device positions if MachineOperations is available
  std::map<std::string, std::string> deviceCurrentNodes;
  if (m_machineOperations) {
    // Get all devices from current positions
    auto currentPositions = m_machineOperations->GetCurrentPositions();
    for (const auto& [deviceName, position] : currentPositions) {
      try {
        std::string currentNodeId = m_machineOperations->GetDeviceCurrentNode(deviceName, m_activeGraph);
        if (!currentNodeId.empty()) {
          deviceCurrentNodes[currentNodeId] = deviceName;
        }
      }
      catch (...) {
        // Ignore errors for individual devices
      }
    }
  }

  // Render each node as a rectangle
  for (const auto& node : graph.Nodes) {
    // Get node center position
    ImVec2 nodePos = GetNodePosition(node);

    // Convert to canvas coordinates
    ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);

    // Calculate node rectangle positions
    ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);
    ImVec2 nodeMax = ImVec2(canvasNodePos.x + NODE_WIDTH / 2, canvasNodePos.y + NODE_HEIGHT / 2);

    // Determine node color based on selection and dragging state
    ImU32 fillColor;
    if (m_draggedNodeId == node.Id) {
      fillColor = SELECTED_NODE_COLOR; // Being dragged
    }
    else if (m_selectedNodeId == node.Id) {
      fillColor = IM_COL32(100, 200, 100, 255); // Selected (green tint)
    }
    else {
      fillColor = NODE_COLOR; // Default
    }

    // Draw node background rectangle
    drawList->AddRectFilled(nodeMin, nodeMax, fillColor, NODE_ROUNDING);
    drawList->AddRect(nodeMin, nodeMax, NODE_BORDER_COLOR, NODE_ROUNDING, 0, 1.5f);

    // Add node ID
    std::string nodeIdText = "ID: " + node.Id;
    ImVec2 idTextSize = ImGui::CalcTextSize(nodeIdText.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - idTextSize.x / 2, nodeMin.y + TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      nodeIdText.c_str()
    );

    // Add node label with more prominence
    std::string nodeLabel = node.Label;
    ImVec2 textSize = ImGui::CalcTextSize(nodeLabel.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - textSize.x / 2, nodeMin.y + idTextSize.y + 2 * TEXT_PADDING),
      IM_COL32(255, 255, 255, 255), // Brighter white for label
      nodeLabel.c_str()
    );

    // Add device and position info
    std::string deviceInfo = "Device: " + node.Device;
    ImVec2 deviceTextSize = ImGui::CalcTextSize(deviceInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - deviceTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + 3 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      deviceInfo.c_str()
    );

    std::string posInfo = "Position: " + node.Position;
    ImVec2 posTextSize = ImGui::CalcTextSize(posInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - posTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + deviceTextSize.y + 4 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      posInfo.c_str()
    );

    // NEW: Draw device crosshair if a device is currently at this node
    auto deviceIt = deviceCurrentNodes.find(node.Id);
    if (deviceIt != deviceCurrentNodes.end()) {
      const std::string& deviceName = deviceIt->second;

      // Draw crosshair
      const float crosshairSize = 20.0f;
      const float crosshairThickness = 3.0f;
      const ImU32 crosshairColor = IM_COL32(255, 255, 0, 255); // Bright yellow

      // Horizontal line
      drawList->AddLine(
        ImVec2(canvasNodePos.x - crosshairSize, canvasNodePos.y),
        ImVec2(canvasNodePos.x + crosshairSize, canvasNodePos.y),
        crosshairColor, crosshairThickness
      );

      // Vertical line
      drawList->AddLine(
        ImVec2(canvasNodePos.x, canvasNodePos.y - crosshairSize),
        ImVec2(canvasNodePos.x, canvasNodePos.y + crosshairSize),
        crosshairColor, crosshairThickness
      );

      // Center dot
      drawList->AddCircleFilled(canvasNodePos, 4.0f, crosshairColor);

      // Device name label above the node
      ImVec2 deviceNameSize = ImGui::CalcTextSize(deviceName.c_str());
      ImVec2 deviceNamePos = ImVec2(
        canvasNodePos.x - deviceNameSize.x / 2,
        nodeMin.y - deviceNameSize.y - 10.0f // 10 pixels above node
      );

      // Draw background for device name
      ImVec2 bgMin = ImVec2(deviceNamePos.x - 4, deviceNamePos.y - 2);
      ImVec2 bgMax = ImVec2(deviceNamePos.x + deviceNameSize.x + 4, deviceNamePos.y + deviceNameSize.y + 2);
      drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 3.0f);
      drawList->AddRect(bgMin, bgMax, crosshairColor, 3.0f, 0, 1.5f);

      // Draw device name text
      drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());
    }
  }
}
void UIConfigVisualizer::RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos) {
  auto graphOpt = configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  // First create a map of nodes by ID for quick lookup
  std::map<std::string, const Node*> nodeMap;
  for (const auto& node : graph.Nodes) {
    nodeMap[node.Id] = &node;
  }

  // Draw edges as lines with arrows
  for (const auto& edge : graph.Edges) {
    // Get source and target nodes
    auto sourceIt = nodeMap.find(edge.Source);
    auto targetIt = nodeMap.find(edge.Target);

    if (sourceIt == nodeMap.end() || targetIt == nodeMap.end()) {
      continue;  // Skip if we can't find either node
    }

    // Get node positions
    ImVec2 sourcePos = GetNodePosition(*sourceIt->second);
    ImVec2 targetPos = GetNodePosition(*targetIt->second);

    // Convert to canvas coordinates
    ImVec2 canvasSourcePos = GraphToCanvas(sourcePos, canvasPos);
    ImVec2 canvasTargetPos = GraphToCanvas(targetPos, canvasPos);

    // Calculate direction vector
    ImVec2 dir = ImVec2(canvasTargetPos.x - canvasSourcePos.x, canvasTargetPos.y - canvasSourcePos.y);
    float length = sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length < 1e-6f) continue; // Skip if nodes are at the same position

    // Normalize direction vector
    dir.x /= length;
    dir.y /= length;

    // Calculate source and target positions at the edge of each node rectangle
    float halfSrcWidth = NODE_WIDTH / 2.0f;
    float halfSrcHeight = NODE_HEIGHT / 2.0f;
    float halfTgtWidth = NODE_WIDTH / 2.0f;
    float halfTgtHeight = NODE_HEIGHT / 2.0f;

    // For source: move from center toward edge in the direction of the target
    ImVec2 sourceEdge;
    if (std::abs(dir.x * halfSrcHeight) > std::abs(dir.y * halfSrcWidth)) {
      // Intersect with left or right edge
      sourceEdge.x = canvasSourcePos.x + (dir.x > 0 ? halfSrcWidth : -halfSrcWidth);
      sourceEdge.y = canvasSourcePos.y + dir.y * (halfSrcWidth / std::abs(dir.x));
    }
    else {
      // Intersect with top or bottom edge
      sourceEdge.x = canvasSourcePos.x + dir.x * (halfSrcHeight / std::abs(dir.y));
      sourceEdge.y = canvasSourcePos.y + (dir.y > 0 ? halfSrcHeight : -halfSrcHeight);
    }

    // For target: move from center toward edge in the direction of the source
    ImVec2 targetEdge;
    if (std::abs(-dir.x * halfTgtHeight) > std::abs(-dir.y * halfTgtWidth)) {
      // Intersect with left or right edge
      targetEdge.x = canvasTargetPos.x + (-dir.x > 0 ? halfTgtWidth : -halfTgtWidth);
      targetEdge.y = canvasTargetPos.y + -dir.y * (halfTgtWidth / std::abs(dir.x));
    }
    else {
      // Intersect with top or bottom edge
      targetEdge.x = canvasTargetPos.x + -dir.x * (halfTgtHeight / std::abs(dir.y));
      targetEdge.y = canvasTargetPos.y + (-dir.y > 0 ? halfTgtHeight : -halfTgtHeight);
    }

    // Check if edge is bidirectional
    bool isBidirectional = edge.Conditions.IsBidirectional;

    // Use green color for bidirectional edges, otherwise use regular edge color
    ImU32 edgeColor = isBidirectional ? BIDIRECTIONAL_EDGE_COLOR : EDGE_COLOR;

    // Draw the edge as a directed arrow
    DrawArrow(drawList, sourceEdge, targetEdge, edgeColor, EDGE_THICKNESS, isBidirectional);

    // Get source and target node names for improved label
    std::string sourceName = sourceIt->second->Label.empty() ?
      sourceIt->second->Id : sourceIt->second->Label;
    std::string targetName = targetIt->second->Label.empty() ?
      targetIt->second->Id : targetIt->second->Label;

    // Create improved edge label
    std::string edgeLabel = "ID: " + edge.Id + "\n" +
      "From: " + sourceName + "\n" +
      "To: " + targetName;

    if (!edge.Label.empty()) {
      edgeLabel += "\n" + edge.Label;
    }

    // Add edge label at midpoint with multiline support
    ImVec2 midpoint = ImVec2((sourceEdge.x + targetEdge.x) * 0.5f, (sourceEdge.y + targetEdge.y) * 0.5f);

    // Calculate total height based on number of lines
    int lineCount = 1;
    for (char c : edgeLabel) {
      if (c == '\n') lineCount++;
    }

    // Split into lines
    std::vector<std::string> lines;
    size_t pos = 0;
    size_t prevPos = 0;

    while ((pos = edgeLabel.find('\n', prevPos)) != std::string::npos) {
      lines.push_back(edgeLabel.substr(prevPos, pos - prevPos));
      prevPos = pos + 1;
    }
    lines.push_back(edgeLabel.substr(prevPos)); // Last line

    // Find the widest line for background sizing
    float maxWidth = 0;
    for (const auto& line : lines) {
      ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
      maxWidth = (std::max)(maxWidth, lineSize.x);
    }

    float lineHeight = ImGui::GetTextLineHeight();
    float totalHeight = lineHeight * lineCount;

    // Draw text background for better visibility
    drawList->AddRectFilled(
      ImVec2(midpoint.x - maxWidth / 2 - TEXT_PADDING, midpoint.y - totalHeight / 2 - TEXT_PADDING),
      ImVec2(midpoint.x + maxWidth / 2 + TEXT_PADDING, midpoint.y + totalHeight / 2 + TEXT_PADDING),
      IM_COL32(40, 40, 40, 200),
      3.0f
    );

    // Draw each line
    for (int i = 0; i < lines.size(); i++) {
      const auto& line = lines[i];
      ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
      float yPos = midpoint.y - totalHeight / 2 + i * lineHeight;

      drawList->AddText(
        ImVec2(midpoint.x - lineSize.x / 2, yPos),
        IM_COL32(220, 220, 220, 255),
        line.c_str()
      );
    }
  }
}

// REMOVED: RenderSelectedNodeActions() method
// The node actions are now handled in the left panel via RenderLeftPanel() in UIConfigVisualizer.cpp