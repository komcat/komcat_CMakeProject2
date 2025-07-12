// UIConfigVisualizer_Input.cpp - Input handling functionality
#include "UIConfigVisualizer.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

void UIConfigVisualizer::HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  // Get current mouse position
  ImVec2 mousePos = ImGui::GetIO().MousePos;

  // Handle zooming with mouse wheel
  if (m_isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
    HandleZooming();
  }

  // Handle panning with middle mouse button
  if (m_isCanvasHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    HandlePanning();
  }

  // Handle node dragging with left mouse button
  if (m_isCanvasHovered) {
    HandleNodeDragging(canvasPos);
    HandleNodeSelection(canvasPos);
  }
  else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
    // Handle the case where mouse is released outside the canvas
    m_isDragging = false;
    m_draggedNodeId.clear();
  }
}

void UIConfigVisualizer::HandleZooming() {
  ImVec2 mousePos = ImGui::GetIO().MousePos;
  float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
  float prevZoom = m_zoomLevel;
  m_zoomLevel = std::max(0.3f, std::min(m_zoomLevel + zoomDelta, 3.0f));

  // Adjust pan to zoom toward mouse position
  if (m_zoomLevel != prevZoom) {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 mouseGraphPos = CanvasToGraph(mousePos, canvasPos);
    float zoomRatio = m_zoomLevel / prevZoom;
    ImVec2 newMouseGraphPos = ImVec2(mouseGraphPos.x * zoomRatio, mouseGraphPos.y * zoomRatio);
    m_panOffset.x += (mouseGraphPos.x - newMouseGraphPos.x);
    m_panOffset.y += (mouseGraphPos.y - newMouseGraphPos.y);
  }
}

void UIConfigVisualizer::HandlePanning() {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
    ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
    m_panOffset.x += dragDelta.x / m_zoomLevel;
    m_panOffset.y += dragDelta.y / m_zoomLevel;
  }
}

void UIConfigVisualizer::HandleNodeDragging(const ImVec2& canvasPos) {
  ImVec2 mousePos = ImGui::GetIO().MousePos;

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_isDragging) {
    // Find the node under the cursor
    std::string nodeId = GetNodeAtPosition(mousePos, canvasPos);

    if (!nodeId.empty()) {
      // Start potential dragging - but don't commit to drag yet
      m_draggedNodeId = nodeId;
      m_dragStartPos = mousePos;
      m_lastMousePos = mousePos;

      m_logger->LogInfo("Mouse down on node: " + nodeId);
    }
  }
  else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_draggedNodeId.empty()) {
    // Check if we've moved enough to start dragging
    ImVec2 dragDelta = ImVec2(mousePos.x - m_dragStartPos.x, mousePos.y - m_dragStartPos.y);
    float dragDistance = sqrt(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);

    if (!m_isDragging && dragDistance > 5.0f) {
      // Start dragging only after moving more than 5 pixels
      m_isDragging = true;
      m_logger->LogInfo("Started dragging node: " + m_draggedNodeId);
    }

    if (m_isDragging) {
      // Continue dragging
      ImVec2 frameDelta = ImVec2(mousePos.x - m_lastMousePos.x, mousePos.y - m_lastMousePos.y);

      // Apply the delta to the node position
      auto graphOpt = configManager.GetGraph(m_activeGraph);
      if (graphOpt.has_value()) {
        const auto& graph = graphOpt.value().get();

        // Find the dragged node
        const Node* draggedNode = nullptr;
        for (const auto& node : graph.Nodes) {
          if (node.Id == m_draggedNodeId) {
            draggedNode = &node;
            break;
          }
        }

        if (draggedNode) {
          // Calculate new position
          ImVec2 nodePos = GetNodePosition(*draggedNode);
          ImVec2 newNodePos = ImVec2(
            nodePos.x + frameDelta.x / m_zoomLevel,
            nodePos.y + frameDelta.y / m_zoomLevel
          );

          // Update position in the graph
          Graph updatedGraph = graph;
          for (Node& node : updatedGraph.Nodes) {
            if (node.Id == m_draggedNodeId) {
              node.X = static_cast<int>(newNodePos.x);
              node.Y = static_cast<int>(newNodePos.y);
              configManager.UpdateGraph(m_activeGraph, updatedGraph);
              break;
            }
          }
        }
      }

      // Update last mouse position for next frame
      m_lastMousePos = mousePos;
    }
  }
  else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_draggedNodeId.empty()) {
    // Mouse released
    if (m_isDragging) {
      // End dragging - save final position
      auto graphOpt = configManager.GetGraph(m_activeGraph);
      if (graphOpt.has_value()) {
        const auto& graph = graphOpt.value().get();

        const Node* draggedNode = nullptr;
        for (const auto& node : graph.Nodes) {
          if (node.Id == m_draggedNodeId) {
            draggedNode = &node;
            break;
          }
        }

        if (draggedNode) {
          ImVec2 nodePos = GetNodePosition(*draggedNode);

          // Save changes permanently
          SaveNodePosition(m_draggedNodeId, nodePos);
          m_logger->LogInfo("Saved position for node: " + m_draggedNodeId +
            " at X:" + std::to_string(static_cast<int>(nodePos.x)) +
            ", Y:" + std::to_string(static_cast<int>(nodePos.y)));
        }
      }

      // Clear drag state
      m_isDragging = false;
      m_draggedNodeId.clear();
    }
    else {
      // This was a click, not a drag - handle selection
      if (!m_draggedNodeId.empty()) {
        m_selectedNodeId = m_draggedNodeId;
        m_showNodeActions = true;
        m_logger->LogInfo("Selected node: " + m_draggedNodeId);
      }

      // Clear drag state
      m_draggedNodeId.clear();
    }
  }
}

void UIConfigVisualizer::HandleNodeSelection(const ImVec2& canvasPos) {
  ImVec2 mousePos = ImGui::GetIO().MousePos;

  // Handle left-click for node selection (only when clicking on empty space)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    // Find the node under the cursor
    std::string nodeId = GetNodeAtPosition(mousePos, canvasPos);

    if (nodeId.empty()) {
      // Clicked on empty space - deselect
      if (!m_selectedNodeId.empty()) {
        m_logger->LogInfo("Deselected node: " + m_selectedNodeId);
        m_selectedNodeId.clear();
        m_showNodeActions = false;
      }
    }
    // Node selection is now handled in HandleNodeDragging when mouse is released without dragging
  }
}