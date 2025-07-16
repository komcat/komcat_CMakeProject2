// UIConfigVisualizer.h
#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include "CameraFeedDisplay.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/MotionTypes.h"
#include "include/logger.h"
#include "imgui.h"

// Forward declaration
class NodePropertiesHandler;

class UIConfigVisualizer {
public:
  UIConfigVisualizer(MotionConfigManager& configManager);
  ~UIConfigVisualizer();

  // Disable copy/move to avoid issues with incomplete types
  UIConfigVisualizer(const UIConfigVisualizer&) = delete;
  UIConfigVisualizer& operator=(const UIConfigVisualizer&) = delete;
  UIConfigVisualizer(UIConfigVisualizer&&) = delete;
  UIConfigVisualizer& operator=(UIConfigVisualizer&&) = delete;

  // UI rendering - no window wrapper, just content
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return showWindow; }

  // Set the active graph to visualize
  void SetActiveGraph(const std::string& graphName);
  void InitializeCameraFeed();
private:
  // Reference to the config manager and logger
  MotionConfigManager& configManager;
  Logger* m_logger;

  //camera
  std::unique_ptr<CameraFeedDisplay> m_cameraFeedDisplay;
  std::string m_selectedCameraId;
  bool m_cameraInitialized = false;

  // Action handlers
  std::unique_ptr<NodePropertiesHandler> m_propertiesHandler;

  // UI state
  bool showWindow = true;
  std::string m_activeGraph;
  float m_zoomLevel = 1.0f;
  ImVec2 m_panOffset = ImVec2(0, 0);
  bool m_isCanvasHovered = false;

  // Node selection and interaction state
  bool m_isDragging = false;
  std::string m_draggedNodeId;
  ImVec2 m_dragStartPos;
  ImVec2 m_lastMousePos;

  // Selected node state (replaces context menu)
  std::string m_selectedNodeId;
  bool m_showNodeActions = false;

  // Rendering constants
  static constexpr float NODE_WIDTH = 160.0f;
  static constexpr float NODE_HEIGHT = 80.0f;
  static constexpr float NODE_ROUNDING = 5.0f;
  static constexpr ImU32 NODE_COLOR = IM_COL32(70, 70, 200, 255);
  static constexpr ImU32 NODE_BORDER_COLOR = IM_COL32(255, 255, 255, 255);
  static constexpr ImU32 SELECTED_NODE_COLOR = IM_COL32(120, 120, 255, 255);
  static constexpr ImU32 EDGE_COLOR = IM_COL32(200, 200, 200, 255);
  static constexpr ImU32 EDGE_HOVER_COLOR = IM_COL32(250, 250, 100, 255);
  static constexpr ImU32 BIDIRECTIONAL_EDGE_COLOR = IM_COL32(50, 205, 50, 255);
  static constexpr ImU32 ARROW_COLOR = IM_COL32(220, 220, 220, 255);
  static constexpr float ARROW_SIZE = 10.0f;
  static constexpr float EDGE_THICKNESS = 2.0f;
  static constexpr float TEXT_PADDING = 5.0f;

  // Main rendering methods - implemented in UIConfigVisualizer_Graph.cpp
  void RenderGraphControls();
  void RenderGraphCanvas();
  void RenderLeftPanel();  // NEW: Render the permanent left panel
  void RenderCameraCanvas(float height);  // NEW: Render the camera/image canvas section
  void RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
  void RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos);
  void RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos);
  // REMOVED: RenderSelectedNodeActions() - now handled in RenderLeftPanel()

  // Input handling methods - implemented in UIConfigVisualizer_Input.cpp
  void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);
  void HandleZooming();
  void HandlePanning();
  void HandleNodeDragging(const ImVec2& canvasPos);
  void HandleNodeSelection(const ImVec2& canvasPos);

  // Helper methods - implemented in UIConfigVisualizer_Helpers.cpp
  ImVec2 GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const;
  ImVec2 CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const;
  ImVec2 GetNodePosition(const Node& node) const;
  void SaveNodePosition(const std::string& nodeId, const ImVec2& newPos);
  void DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, bool isBidirectional = false);
  std::string GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos);
};