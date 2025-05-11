// MotionGraphic.h
#pragma once

#include "include/motions/MotionConfigManager.h"
#include "include/motions/MotionTypes.h"
#include "include/motions/motion_control_layer.h"
#include "include/machine_operations.h"
#include "include/logger.h"
#include "imgui.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <set>  // Add this include for std::set

// Motion Graphic class for visualizing controller positions in relation to graph nodes
class MotionGraphic {
public:
  MotionGraphic(MotionConfigManager& configManager,
    MotionControlLayer& motionLayer,
    MachineOperations& machineOps);
  ~MotionGraphic();

  // Render the graph visualization with current controller positions
  void RenderUI();

  // Show/hide the visualizer window
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }

  // Set the active graph to visualize
  void SetActiveGraph(const std::string& graphName);

  // Refresh position data
  void RefreshPositionData();

private:
  // References to the config manager and machine operations
  MotionConfigManager& m_configManager;
  MotionControlLayer& m_motionLayer;
  MachineOperations& m_machineOps;
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  std::string m_activeGraph;
  float m_zoomLevel = 1.0f;
  ImVec2 m_panOffset = ImVec2(0, 0);
  bool m_isCanvasHovered = false;

  // Position update thread
  std::thread m_updateThread;
  std::mutex m_positionMutex;
  std::atomic<bool> m_threadRunning{ false };
  std::chrono::milliseconds m_updateInterval{ 200 }; // 5 Hz update rate

  // Node dragging state
  bool m_isDragging = false;
  std::string m_draggedNodeId;
  ImVec2 m_dragStartPos;
  ImVec2 m_lastMousePos;

  // Current positions of all controllers
  struct ControllerState {
    PositionStruct position;
    std::string currentNodeId;
    bool isMoving;
    std::string targetNodeId;
    double progress; // 0.0 to 1.0
  };
  std::map<std::string, ControllerState> m_controllerStates;

  // Selected controller and node for operations
  std::string m_selectedController;
  std::string m_selectedNode;

  // Rendering constants
  const float NODE_WIDTH = 160.0f;
  const float NODE_HEIGHT = 90.0f;
  const float NODE_ROUNDING = 5.0f;
  const ImU32 NODE_COLOR = IM_COL32(70, 70, 200, 255);
  const ImU32 NODE_BORDER_COLOR = IM_COL32(255, 255, 255, 255);
  const ImU32 SELECTED_NODE_COLOR = IM_COL32(120, 120, 255, 255);
  const ImU32 CURRENT_NODE_COLOR = IM_COL32(50, 200, 50, 255); // Green for current node
  const ImU32 TARGET_NODE_COLOR = IM_COL32(200, 50, 50, 255);  // Red for target node
  const ImU32 CONTROLLER_POSITION_COLOR = IM_COL32(255, 255, 0, 255); // Yellow for actual position
  const ImU32 EDGE_COLOR = IM_COL32(200, 200, 200, 255);
  const ImU32 EDGE_HOVER_COLOR = IM_COL32(250, 250, 100, 255);
  const ImU32 BIDIRECTIONAL_EDGE_COLOR = IM_COL32(50, 205, 50, 255);
  const ImU32 PATH_EDGE_COLOR = IM_COL32(255, 165, 0, 255); // Orange for planned path
  const ImU32 ARROW_COLOR = IM_COL32(220, 220, 220, 255);
  const float ARROW_SIZE = 10.0f;
  const float EDGE_THICKNESS = 2.0f;
  const float PATH_EDGE_THICKNESS = 3.0f;
  const float TEXT_PADDING = 5.0f;
  const float MARKER_SIZE = 8.0f;

  // Helper functions
  void RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
  void RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos);
  void RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos);
  void RenderControllerPositions(ImDrawList* drawList, const ImVec2& canvasPos);
  void RenderControllerPanel();
  void RenderNodePanel();
  void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);
  void UpdatePositionsThreadFunc();

  // Coordinate conversion helpers
  ImVec2 GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const;
  ImVec2 CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const;
  ImVec2 RealWorldToGraphCoord(const PositionStruct& pos) const;

  // Node position helpers
  ImVec2 GetNodePosition(const Node& node) const;

  // Draw arrow for an edge
  void DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end,
    ImU32 color, float thickness, bool isBidirectional = false);

  // Execute movement to a node
  bool MoveToNode(const std::string& controllerName, const std::string& nodeId);

  // Hit testing
  std::string GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos);
};