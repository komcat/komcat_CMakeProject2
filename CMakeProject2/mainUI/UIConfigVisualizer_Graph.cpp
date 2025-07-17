// UIConfigVisualizer_Graph.cpp - Graph rendering functionality (updated for 3D distance calculation)
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

// Enhanced RenderNodes with closest node crosshair tracking
void UIConfigVisualizer::RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos) {
  auto graphOpt = configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  // Render all nodes first (existing code)
  for (const auto& node : graph.Nodes) {
    // ... existing node rendering code (unchanged) ...

    // Get node center position
    ImVec2 nodePos = GetNodePosition(node);
    ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);
    ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);
    ImVec2 nodeMax = ImVec2(canvasNodePos.x + NODE_WIDTH / 2, canvasNodePos.y + NODE_HEIGHT / 2);

    // Determine node color based on selection and dragging state
    ImU32 fillColor;
    if (m_draggedNodeId == node.Id) {
      fillColor = SELECTED_NODE_COLOR;
    }
    else if (m_selectedNodeId == node.Id) {
      fillColor = IM_COL32(100, 200, 100, 255);
    }
    else {
      fillColor = NODE_COLOR;
    }

    // Draw node background rectangle
    drawList->AddRectFilled(nodeMin, nodeMax, fillColor, NODE_ROUNDING);
    drawList->AddRect(nodeMin, nodeMax, NODE_BORDER_COLOR, NODE_ROUNDING, 0, 1.5f);

    // Add node text (ID, label, device, position)
    std::string nodeIdText = "ID: " + node.Id;
    ImVec2 idTextSize = ImGui::CalcTextSize(nodeIdText.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - idTextSize.x / 2, nodeMin.y + TEXT_PADDING),
      IM_COL32(200, 200, 200, 255), nodeIdText.c_str()
    );

    std::string nodeLabel = node.Label;
    ImVec2 textSize = ImGui::CalcTextSize(nodeLabel.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - textSize.x / 2, nodeMin.y + idTextSize.y + 2 * TEXT_PADDING),
      IM_COL32(255, 255, 255, 255), nodeLabel.c_str()
    );

    std::string deviceInfo = "Device: " + node.Device;
    ImVec2 deviceTextSize = ImGui::CalcTextSize(deviceInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - deviceTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + 3 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255), deviceInfo.c_str()
    );

    std::string posInfo = "Position: " + node.Position;
    ImVec2 posTextSize = ImGui::CalcTextSize(posInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - posTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + deviceTextSize.y + 4 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255), posInfo.c_str()
    );
  }

  // NEW: Render device crosshairs with closest node logic
  if (m_machineOperations) {
    RenderDeviceCrosshairsWithClosestNode(drawList, canvasPos, graph);
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

// NEW: Enhanced crosshair rendering with closest node fallback
void UIConfigVisualizer::RenderDeviceCrosshairsWithClosestNode(ImDrawList* drawList, const ImVec2& canvasPos, const Graph& graph) {
  auto currentPositions = m_machineOperations->GetCurrentPositions();

  for (const auto& [deviceName, position] : currentPositions) {
    // Try to find exact node match first
    std::string currentNodeId;
    bool isAtExactNode = false;

    try {
      currentNodeId = m_machineOperations->GetDeviceCurrentNode(deviceName, m_activeGraph);
      isAtExactNode = !currentNodeId.empty();
    }
    catch (...) {
      // Handle error gracefully
    }

    if (isAtExactNode) {
      // Device is at a known node - show exact crosshair
      const Node* exactNode = FindNodeById(graph, currentNodeId, deviceName);
      if (exactNode) {
        ImVec2 nodePos = GetNodePosition(*exactNode);
        ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);
        DrawExactNodeCrosshair(drawList, canvasNodePos, deviceName);
      }
    }
    else {
      // Device is not at any node - find closest node and show offset crosshair
      const Node* closestNode = FindClosestNodeForDevice(graph, deviceName, position);
      if (closestNode) {
        DrawOffsetCrosshairWithGuideLine(drawList, canvasPos, *closestNode, deviceName, position);
      }
    }
  }
}

// Helper: Find node by ID and device
const Node* UIConfigVisualizer::FindNodeById(const Graph& graph, const std::string& nodeId, const std::string& deviceName) {
  for (const auto& node : graph.Nodes) {
    if (node.Id == nodeId && node.Device == deviceName) {
      return &node;
    }
  }
  return nullptr;
}

// Helper: Find closest node for a device based on 3D position coordinates
const Node* UIConfigVisualizer::FindClosestNodeForDevice(const Graph& graph, const std::string& deviceName, const PositionStruct& currentPos) {
  const Node* closestNode = nullptr;
  double minDistance = (std::numeric_limits<double>::max)();

  // Check if device has Z-axis capability
  bool deviceHasZ = DeviceHasZAxis(deviceName);

  for (const auto& node : graph.Nodes) {
    // Only consider nodes for this device
    if (node.Device != deviceName || node.Position.empty()) {
      continue;
    }

    // Get node's position coordinates
    auto positionOpt = configManager.GetNamedPosition(deviceName, node.Position);
    if (!positionOpt.has_value()) {
      continue;
    }

    const auto& nodePos = positionOpt.value().get();

    // Calculate 3D distance if device has Z-axis, otherwise use 2D distance
    double dx = currentPos.x - nodePos.x;
    double dy = currentPos.y - nodePos.y;
    double distance;

    if (deviceHasZ) {
      // Use 3D distance calculation (XYZ)
      double dz = currentPos.z - nodePos.z;
      distance = sqrt(dx * dx + dy * dy + dz * dz);
    }
    else {
      // Use 2D distance calculation (XY only)
      distance = sqrt(dx * dx + dy * dy);
    }

    if (distance < minDistance) {
      minDistance = distance;
      closestNode = &node;
    }
  }

  return closestNode;
}

// Draw crosshair when device is exactly at a node
void UIConfigVisualizer::DrawExactNodeCrosshair(ImDrawList* drawList, const ImVec2& canvasPos, const std::string& deviceName) {
  const float crosshairSize = 20.0f;
  const float crosshairThickness = 3.0f;
  const ImU32 crosshairColor = IM_COL32(255, 255, 0, 255); // Bright yellow - device is exactly at node

  // Draw crosshair
  drawList->AddLine(
    ImVec2(canvasPos.x - crosshairSize, canvasPos.y),
    ImVec2(canvasPos.x + crosshairSize, canvasPos.y),
    crosshairColor, crosshairThickness
  );
  drawList->AddLine(
    ImVec2(canvasPos.x, canvasPos.y - crosshairSize),
    ImVec2(canvasPos.x, canvasPos.y + crosshairSize),
    crosshairColor, crosshairThickness
  );
  drawList->AddCircleFilled(canvasPos, 4.0f, crosshairColor);

  // Device name label
  DrawDeviceNameLabel(drawList, canvasPos, deviceName, crosshairColor);
}

// Draw offset crosshair with guide line to closest node (using 3D distance calculation)
void UIConfigVisualizer::DrawOffsetCrosshairWithGuideLine(ImDrawList* drawList, const ImVec2& canvasPos,
  const Node& closestNode, const std::string& deviceName,
  const PositionStruct& currentPos) {
  // Get closest node canvas position
  ImVec2 nodePos = GetNodePosition(closestNode);
  ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);

  // Get node's real position coordinates
  auto positionOpt = configManager.GetNamedPosition(deviceName, closestNode.Position);
  if (!positionOpt.has_value()) {
    return;
  }
  const auto& nodeRealPos = positionOpt.value().get();

  // Calculate offset in real coordinates
  double offsetX = currentPos.x - nodeRealPos.x;  // mm
  double offsetY = currentPos.y - nodeRealPos.y;  // mm

  // Check if device has Z-axis capability for distance calculation
  bool deviceHasZ = DeviceHasZAxis(deviceName);
  double totalDistance;

  if (deviceHasZ) {
    // Calculate 3D distance including Z component
    double offsetZ = currentPos.z - nodeRealPos.z;  // mm
    totalDistance = sqrt(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
  }
  else {
    // Calculate 2D distance (XY only)
    totalDistance = sqrt(offsetX * offsetX + offsetY * offsetY);
  }

  // Convert XY offset to canvas coordinates (scale factor for mm to pixels)
  // Note: We only display XY offset on 2D canvas, even though distance calculation uses 3D
  const float scale = 50.0f; // 2 pixels per mm - adjust this as needed
  float canvasOffsetX = static_cast<float>(offsetX * scale * m_zoomLevel);
  float canvasOffsetY = static_cast<float>(offsetY * scale * m_zoomLevel);

  // Calculate crosshair position (node position + XY offset only for display)
  ImVec2 crosshairPos = ImVec2(
    canvasNodePos.x + canvasOffsetX,
    canvasNodePos.y + canvasOffsetY
  );

  // Draw guide line from node to crosshair
  const ImU32 guideLineColor = IM_COL32(128, 128, 128, 180); // Semi-transparent gray
  drawList->AddLine(canvasNodePos, crosshairPos, guideLineColor, 2.0f);

  // Draw distance text at midpoint of guide line
  ImVec2 midPoint = ImVec2(
    (canvasNodePos.x + crosshairPos.x) * 0.5f,
    (canvasNodePos.y + crosshairPos.y) * 0.5f
  );

  // Use the 3D total distance for the label
  std::string distanceText = "+" + std::to_string(static_cast<int>(totalDistance)) + "mm";
  ImVec2 distanceTextSize = ImGui::CalcTextSize(distanceText.c_str());

  // Background for distance text
  drawList->AddRectFilled(
    ImVec2(midPoint.x - distanceTextSize.x / 2 - 2, midPoint.y - distanceTextSize.y / 2 - 1),
    ImVec2(midPoint.x + distanceTextSize.x / 2 + 2, midPoint.y + distanceTextSize.y / 2 + 1),
    IM_COL32(0, 0, 0, 160), 2.0f
  );

  drawList->AddText(
    ImVec2(midPoint.x - distanceTextSize.x / 2, midPoint.y - distanceTextSize.y / 2),
    IM_COL32(200, 200, 200, 255), distanceText.c_str()
  );

  // Draw offset crosshair at calculated position
  const float crosshairSize = 15.0f; // Slightly smaller for offset crosshair
  const float crosshairThickness = 2.5f;
  const ImU32 crosshairColor = IM_COL32(255, 165, 0, 255); // Orange - device is offset from node

  drawList->AddLine(
    ImVec2(crosshairPos.x - crosshairSize, crosshairPos.y),
    ImVec2(crosshairPos.x + crosshairSize, crosshairPos.y),
    crosshairColor, crosshairThickness
  );
  drawList->AddLine(
    ImVec2(crosshairPos.x, crosshairPos.y - crosshairSize),
    ImVec2(crosshairPos.x, crosshairPos.y + crosshairSize),
    crosshairColor, crosshairThickness
  );
  drawList->AddCircleFilled(crosshairPos, 3.0f, crosshairColor);

  // Device name label
  DrawDeviceNameLabel(drawList, crosshairPos, deviceName, crosshairColor);
}

// Helper: Draw device name label with background
void UIConfigVisualizer::DrawDeviceNameLabel(ImDrawList* drawList, const ImVec2& pos, const std::string& deviceName, ImU32 borderColor) {
  ImVec2 deviceNameSize = ImGui::CalcTextSize(deviceName.c_str());
  ImVec2 deviceNamePos = ImVec2(
    pos.x - deviceNameSize.x / 2,
    pos.y - 25.0f - deviceNameSize.y // Position above crosshair
  );

  // Background
  ImVec2 bgMin = ImVec2(deviceNamePos.x - 4, deviceNamePos.y - 2);
  ImVec2 bgMax = ImVec2(deviceNamePos.x + deviceNameSize.x + 4, deviceNamePos.y + deviceNameSize.y + 2);
  drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 3.0f);
  drawList->AddRect(bgMin, bgMax, borderColor, 3.0f, 0, 1.5f);

  // Text
  drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());
}