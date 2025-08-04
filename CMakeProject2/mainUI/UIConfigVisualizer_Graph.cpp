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

    // Replace the crosshair section in UIConfigVisualizer_Graph.cpp RenderNodes function
    // This goes in place of the "NEW: Draw device crosshair if a device is currently at this node" section

    // NEW: Draw device crosshair if a device is currently at this node
    auto deviceIt = deviceCurrentNodes.find(node.Id);
    if (deviceIt != deviceCurrentNodes.end()) {
      const std::string& deviceName = deviceIt->second;

      // Get actual device position and node position for comparison
      bool showOffset = false;
      float offsetMM_X = 0.0f;
      float offsetMM_Y = 0.0f;
      ImVec2 actualCrosshairPos = canvasNodePos; // Default to node position


      // SEPARATE LOOP: Draw device crosshairs (after all nodes are rendered)
      if (m_machineOperations) {
        auto currentPositions = m_machineOperations->GetCurrentPositions();

        for (const auto& [deviceName, currentPos] : currentPositions) {
          std::string assignedNodeId = "";
          bool deviceAtExactNode = false;

          // First, try to get the exact node the device is at
          try {
            assignedNodeId = m_machineOperations->GetDeviceCurrentNode(deviceName, m_activeGraph);
            if (!assignedNodeId.empty()) {
              deviceAtExactNode = true;
            }
          }
          catch (...) {
            // GetDeviceCurrentNode failed, we'll find closest manually
          }

          // If device is not at an exact node, find the closest node manually
          if (assignedNodeId.empty()) {
            float closestDistance = FLT_MAX;
            const Node* closestNode = nullptr;

            for (const auto& node : graph.Nodes) {
              // Only consider nodes that have this device assigned
              if (node.Device != deviceName) continue;

              // Get the expected position for this node
              if (!node.Position.empty()) {
                auto expectedPosOpt = configManager.GetNamedPosition(node.Device, node.Position);
                if (expectedPosOpt.has_value()) {
                  const auto& expectedPos = expectedPosOpt.value().get();

                  // Calculate distance from current position to this node's expected position
                  float deltaX = static_cast<float>(currentPos.x - expectedPos.x);
                  float deltaY = static_cast<float>(currentPos.y - expectedPos.y);
                  float deltaZ = static_cast<float>(currentPos.z - expectedPos.z);
                  float distance = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

                  if (distance < closestDistance) {
                    closestDistance = distance;
                    closestNode = &node;
                    assignedNodeId = node.Id;
                  }
                }
              }
            }
          }

          // If we found a node (either exact or closest), draw the crosshair
          if (!assignedNodeId.empty()) {
            // Find the node object
            const Node* targetNode = nullptr;
            for (const auto& node : graph.Nodes) {
              if (node.Id == assignedNodeId) {
                targetNode = &node;
                break;
              }
            }

            if (!targetNode) continue;

            // Get node canvas position
            ImVec2 nodePos = GetNodePosition(*targetNode);
            ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);

            // Calculate offset from expected position
            bool showOffset = false;
            float offsetMM_X = 0.0f;
            float offsetMM_Y = 0.0f;
            ImVec2 actualCrosshairPos = canvasNodePos; // Default to node position

            if (!targetNode->Position.empty()) {
              auto expectedPosOpt = configManager.GetNamedPosition(targetNode->Device, targetNode->Position);
              if (expectedPosOpt.has_value()) {
                const auto& expectedPos = expectedPosOpt.value().get();

                // Calculate offset in millimeters
                offsetMM_X = static_cast<float>(currentPos.x - expectedPos.x);
                offsetMM_Y = static_cast<float>(currentPos.y - expectedPos.y);

                // Check if there's a significant offset (more than 0.0001mm tolerance)
                float totalOffsetMM = sqrt(offsetMM_X * offsetMM_X + offsetMM_Y * offsetMM_Y);
                if (totalOffsetMM > 0.0001f) {  // 0.1 micron tolerance
                  showOffset = true;

                  // Calculate pixel offset with max 2mm = 100 pixels scale
                  const float maxOffsetMM = 2.0f;
                  const float maxOffsetPixels = 100.0f;
                  float scaleFactor = maxOffsetPixels / maxOffsetMM;

                  // Clamp offset to maximum
                  float clampedOffsetX = (std::max)(-maxOffsetMM, (std::min)(maxOffsetMM, offsetMM_X));
                  float clampedOffsetY = (std::max)(-maxOffsetMM, (std::min)(maxOffsetMM, offsetMM_Y));

                  // Apply offset to crosshair position
                  actualCrosshairPos = ImVec2(
                    canvasNodePos.x + clampedOffsetX * scaleFactor,
                    canvasNodePos.y + clampedOffsetY * scaleFactor
                  );
                }
              }
            }

            // Draw crosshair at actual position (offset if needed)
            const float crosshairSize = 20.0f;
            const float crosshairThickness = 3.0f;

            // Color coding: Green if at exact node, Orange if offset, Red if at wrong node
            ImU32 crosshairColor;
            if (deviceAtExactNode) {
              crosshairColor = IM_COL32(0, 255, 0, 255); // Green - at exact node
            }
            else if (showOffset) {
              crosshairColor = IM_COL32(255, 165, 0, 255); // Orange - offset from assigned node  
            }
            else {
              crosshairColor = IM_COL32(255, 255, 0, 255); // Yellow - at assigned node
            }

            // If device is at wrong node (closest node), use red color
            if (!deviceAtExactNode && targetNode->Device == deviceName) {
              crosshairColor = IM_COL32(255, 100, 100, 255); // Red - at wrong node but closest
            }

            // Horizontal line
            drawList->AddLine(
              ImVec2(actualCrosshairPos.x - crosshairSize, actualCrosshairPos.y),
              ImVec2(actualCrosshairPos.x + crosshairSize, actualCrosshairPos.y),
              crosshairColor, crosshairThickness
            );

            // Vertical line
            drawList->AddLine(
              ImVec2(actualCrosshairPos.x, actualCrosshairPos.y - crosshairSize),
              ImVec2(actualCrosshairPos.x, actualCrosshairPos.y + crosshairSize),
              crosshairColor, crosshairThickness
            );

            // Center dot
            drawList->AddCircleFilled(actualCrosshairPos, 4.0f, crosshairColor);

            // If there's an offset, draw a line connecting the expected and actual positions
            if (showOffset) {
              const ImU32 connectionLineColor = IM_COL32(255, 100, 100, 150); // Semi-transparent red
              drawList->AddLine(canvasNodePos, actualCrosshairPos, connectionLineColor, 2.0f);

              // Draw small circle at expected position
              drawList->AddCircle(canvasNodePos, 6.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
            }

            // Calculate node rectangle for text positioning
            ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);

            // Device name label above the node (or offset position)
            ImVec2 deviceNameSize = ImGui::CalcTextSize(deviceName.c_str());

            // Prepare the device text with offset information
            std::string deviceText = deviceName;
            std::string offsetText = "";

            if (showOffset) {
              // Calculate total distance
              float totalOffsetMM = sqrt(offsetMM_X * offsetMM_X + offsetMM_Y * offsetMM_Y);

              // Create direction arrows
              std::string xArrow = "";
              std::string yArrow = "";

              if (abs(offsetMM_X) > 0.0001f) {  // 0.1 micron threshold
                xArrow = (offsetMM_X > 0) ? "X→" : "X←";
              }
              if (abs(offsetMM_Y) > 0.0001f) {  // 0.1 micron threshold
                yArrow = (offsetMM_Y > 0) ? "Y↑" : "Y↓";
              }

              // Format offset text: "0.1mm X← Y↑"
              char offsetBuffer[64];
              snprintf(offsetBuffer, sizeof(offsetBuffer), "%.3fmm %s%s",
                totalOffsetMM, xArrow.c_str(), yArrow.c_str());
              offsetText = offsetBuffer;
            }

            // Add node info if this is closest node (not exact)
            if (!deviceAtExactNode) {
              offsetText = "→" + targetNode->Id + (offsetText.empty() ? "" : " " + offsetText);
            }

            // Calculate positions for both lines of text
            ImVec2 offsetTextSize = (!offsetText.empty()) ? ImGui::CalcTextSize(offsetText.c_str()) : ImVec2(0, 0);
            float maxTextWidth = (std::max)(deviceNameSize.x, offsetTextSize.x);

            ImVec2 deviceNamePos = ImVec2(
              actualCrosshairPos.x - deviceNameSize.x / 2,
              nodeMin.y - deviceNameSize.y - (!offsetText.empty() ? offsetTextSize.y + 5 : 0) - 15.0f
            );

            ImVec2 offsetTextPos = ImVec2(
              actualCrosshairPos.x - offsetTextSize.x / 2,
              deviceNamePos.y + deviceNameSize.y + 3.0f
            );

            // Draw background for device name and offset text
            if (!offsetText.empty()) {
              // Background for both lines
              ImVec2 bgMin = ImVec2(
                actualCrosshairPos.x - maxTextWidth / 2 - 6,
                deviceNamePos.y - 3
              );
              ImVec2 bgMax = ImVec2(
                actualCrosshairPos.x + maxTextWidth / 2 + 6,
                offsetTextPos.y + offsetTextSize.y + 3
              );

              drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 200), 4.0f);
              drawList->AddRect(bgMin, bgMax, crosshairColor, 4.0f, 0, 1.5f);

              // Draw device name
              drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());

              // Draw offset/node information in orange/red
              drawList->AddText(offsetTextPos, IM_COL32(255, 150, 100, 255), offsetText.c_str());
            }
            else {
              // Background for device name only
              ImVec2 bgMin = ImVec2(deviceNamePos.x - 4, deviceNamePos.y - 2);
              ImVec2 bgMax = ImVec2(deviceNamePos.x + deviceNameSize.x + 4, deviceNamePos.y + deviceNameSize.y + 2);

              drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 3.0f);
              drawList->AddRect(bgMin, bgMax, crosshairColor, 3.0f, 0, 1.5f);

              // Draw device name
              drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());
            }
          }
        }
      }
      // Draw crosshair at actual position (offset if needed)
      const float crosshairSize = 20.0f;
      const float crosshairThickness = 3.0f;
      const ImU32 crosshairColor = showOffset ? IM_COL32(255, 165, 0, 255) : IM_COL32(255, 255, 0, 255); // Orange if offset, yellow if exact

      // Horizontal line
      drawList->AddLine(
        ImVec2(actualCrosshairPos.x - crosshairSize, actualCrosshairPos.y),
        ImVec2(actualCrosshairPos.x + crosshairSize, actualCrosshairPos.y),
        crosshairColor, crosshairThickness
      );

      // Vertical line
      drawList->AddLine(
        ImVec2(actualCrosshairPos.x, actualCrosshairPos.y - crosshairSize),
        ImVec2(actualCrosshairPos.x, actualCrosshairPos.y + crosshairSize),
        crosshairColor, crosshairThickness
      );

      // Center dot
      drawList->AddCircleFilled(actualCrosshairPos, 4.0f, crosshairColor);

      // If there's an offset, draw a line connecting the expected and actual positions
      if (showOffset) {
        const ImU32 connectionLineColor = IM_COL32(255, 100, 100, 150); // Semi-transparent red
        drawList->AddLine(canvasNodePos, actualCrosshairPos, connectionLineColor, 2.0f);

        // Draw small circle at expected position
        drawList->AddCircle(canvasNodePos, 6.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
      }

      // Device name label above the node (or offset position)
      ImVec2 deviceNameSize = ImGui::CalcTextSize(deviceName.c_str());

      // Prepare the device text with offset information
      std::string deviceText = deviceName;
      std::string offsetText = "";

      if (showOffset) {
        // Calculate total distance
        float totalOffsetMM = sqrt(offsetMM_X * offsetMM_X + offsetMM_Y * offsetMM_Y);

        // Create direction arrows
        std::string xArrow = "";
        std::string yArrow = "";

        if (abs(offsetMM_X) > 0.0001f) {  // 0.1 micron threshold
          xArrow = (offsetMM_X > 0) ? "X→" : "X←";
        }
        if (abs(offsetMM_Y) > 0.0001f) {  // 0.1 micron threshold
          yArrow = (offsetMM_Y > 0) ? "Y↑" : "Y↓";
        }

        // Format offset text: "0.1mm X← Y↑"
        char offsetBuffer[64];
        snprintf(offsetBuffer, sizeof(offsetBuffer), "%.3fmm %s%s",
          totalOffsetMM, xArrow.c_str(), yArrow.c_str());
        offsetText = offsetBuffer;
      }

      // Calculate positions for both lines of text
      ImVec2 offsetTextSize = (!offsetText.empty()) ? ImGui::CalcTextSize(reinterpret_cast<const char*>(offsetText.c_str())) : ImVec2(0, 0);
      float maxTextWidth = (std::max)(deviceNameSize.x, offsetTextSize.x);

      ImVec2 deviceNamePos = ImVec2(
        actualCrosshairPos.x - deviceNameSize.x / 2,
        nodeMin.y - deviceNameSize.y - (showOffset ? offsetTextSize.y + 5 : 0) - 15.0f
      );

      ImVec2 offsetTextPos = ImVec2(
        actualCrosshairPos.x - offsetTextSize.x / 2,
        deviceNamePos.y + deviceNameSize.y + 3.0f
      );

      // Draw background for device name and offset text
      if (showOffset && !offsetText.empty()) {
        // Background for both lines
        ImVec2 bgMin = ImVec2(
          actualCrosshairPos.x - maxTextWidth / 2 - 6,
          deviceNamePos.y - 3
        );
        ImVec2 bgMax = ImVec2(
          actualCrosshairPos.x + maxTextWidth / 2 + 6,
          offsetTextPos.y + offsetTextSize.y + 3
        );

        drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 200), 4.0f);
        drawList->AddRect(bgMin, bgMax, crosshairColor, 4.0f, 0, 1.5f);

        // Draw device name
        drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());

        // Draw offset information in orange/red
        drawList->AddText(offsetTextPos, IM_COL32(255, 150, 100, 255), offsetText.c_str());
      }
      else {
        // Background for device name only
        ImVec2 bgMin = ImVec2(deviceNamePos.x - 4, deviceNamePos.y - 2);
        ImVec2 bgMax = ImVec2(deviceNamePos.x + deviceNameSize.x + 4, deviceNamePos.y + deviceNameSize.y + 2);

        drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 3.0f);
        drawList->AddRect(bgMin, bgMax, crosshairColor, 3.0f, 0, 1.5f);

        // Draw device name
        drawList->AddText(deviceNamePos, IM_COL32(255, 255, 255, 255), deviceName.c_str());
      }
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
      maxWidth = ((std::max))(maxWidth, lineSize.x);
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