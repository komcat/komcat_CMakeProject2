// MotionGraphic.cpp
#include "include/ui/MotionGraphic.h"
#include "include/machine_operations.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>  // For std::set
#include <limits>  // For std::numeric_limits

MotionGraphic::MotionGraphic(MotionConfigManager& configManager,
  MotionControlLayer& motionLayer,
  MachineOperations& machineOps)
  : m_configManager(configManager)
  , m_motionLayer(motionLayer)
  , m_machineOps(machineOps)
  , m_logger(Logger::GetInstance())
{
  m_logger->LogInfo("MotionGraphic initialized");

  // Set a default active graph if available
  const auto& graphs = m_configManager.GetAllGraphs();
  if (!graphs.empty()) {
    m_activeGraph = graphs.begin()->first;
    m_logger->LogInfo("Default active graph set to: " + m_activeGraph);
  }

  // Start the position update thread
  m_threadRunning = true;
  m_updateThread = std::thread(&MotionGraphic::UpdatePositionsThreadFunc, this);
}

MotionGraphic::~MotionGraphic() {
  // Stop the update thread
  m_threadRunning = false;
  if (m_updateThread.joinable()) {
    m_updateThread.join();
  }
  m_logger->LogInfo("MotionGraphic shutdown");
}

void MotionGraphic::SetActiveGraph(const std::string& graphName) {
  if (m_activeGraph != graphName) {
    m_activeGraph = graphName;
    m_zoomLevel = 1.0f;
    m_panOffset = ImVec2(0, 0);
    m_logger->LogInfo("Active graph set to: " + graphName);
  }
}

void MotionGraphic::RenderUI() {
  if (!m_showWindow) return;

  // Begin the main window
  ImGui::Begin("Motion Graphic", &m_showWindow);

  // Top panel with controls
  ImGui::BeginChild("TopControlsPanel", ImVec2(0, 60), true);

  // Graph selection dropdown
  const auto& allGraphs = m_configManager.GetAllGraphs();
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

  // Add zoom controls
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
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    RefreshPositionData();
  }

  ImGui::EndChild();

  // Calculate layout sizes
  float mainPanelWidth = ImGui::GetContentRegionAvail().x * 0.75f;
  float sidePanelWidth = ImGui::GetContentRegionAvail().x - mainPanelWidth - 8.0f;

  // Main panel with graph visualization
  ImGui::BeginChild("GraphPanel", ImVec2(mainPanelWidth, 0), true);

  // Instructions
  ImGui::TextWrapped("Use middle mouse button to pan, mouse wheel to zoom. Click on nodes or controllers to select.");

  // Calculate canvas size and position
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
  if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

  // Create a child frame for the canvas with NoMove flag
  ImGui::BeginChildFrame(ImGui::GetID("CanvasFrame"), canvasSize,
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
    RenderControllerPositions(drawList, canvasPos);
  }

  ImGui::EndChildFrame();
  ImGui::EndChild();

  // Side panel with controller and node info
  ImGui::SameLine();
  ImGui::BeginChild("SidePanel", ImVec2(sidePanelWidth, 0), true);

  // Render controller panel
  RenderControllerPanel();

  ImGui::Separator();

  // Render node panel
  RenderNodePanel();

  ImGui::EndChild();

  ImGui::End();
}

// Coordinate conversion helpers
ImVec2 MotionGraphic::GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const {
  // Convert from graph coordinates to canvas coordinates
  return ImVec2(
    canvasPos.x + (pos.x + m_panOffset.x) * m_zoomLevel,
    canvasPos.y + (pos.y + m_panOffset.y) * m_zoomLevel
  );
}

ImVec2 MotionGraphic::CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const {
  // Convert from canvas coordinates to graph coordinates
  return ImVec2(
    (pos.x - canvasPos.x) / m_zoomLevel - m_panOffset.x,
    (pos.y - canvasPos.y) / m_zoomLevel - m_panOffset.y
  );
}

// Node position helpers
ImVec2 MotionGraphic::GetNodePosition(const Node& node) const {
  // Get node position from its X and Y properties
  return ImVec2(static_cast<float>(node.X), static_cast<float>(node.Y));
}

void MotionGraphic::RefreshPositionData() {
  m_logger->LogInfo("MotionGraphic: Refreshing position data");

  // Get list of available devices
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  // Update position for each device
  for (const auto& deviceName : deviceList) {
    ControllerState state;

    // Get the current position
    PositionStruct position;
    if (m_motionLayer.GetCurrentPosition(deviceName, position)) {
      state.position = position;
    }

    // Check if device is at a known node
    std::string currentNodeId;
    if (m_motionLayer.GetDeviceCurrentNode(m_activeGraph, deviceName, currentNodeId)) {
      state.currentNodeId = currentNodeId;
    }
    else {
      state.currentNodeId = "";
    }

    // Check if device is moving
    state.isMoving = m_machineOps.IsDeviceMoving(deviceName);

    // If device is moving and executing a path, get progress
    if (state.isMoving && m_motionLayer.IsExecuting()) {
      state.progress = m_motionLayer.GetPathProgress();
      state.targetNodeId = m_motionLayer.GetNextNodeId();
    }
    else {
      state.progress = 0.0;
      state.targetNodeId = "";
    }

    // Update the controller state
    {
      std::lock_guard<std::mutex> lock(m_positionMutex);
      m_controllerStates[deviceName] = state;
    }
  }
}

void MotionGraphic::HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  // Get current mouse position
  ImVec2 mousePos = ImGui::GetIO().MousePos;

  // Handle zooming with mouse wheel
  if (m_isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
    float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
    float prevZoom = m_zoomLevel;
    m_zoomLevel = (std::max)(0.3f, (std::min)(m_zoomLevel + zoomDelta, 3.0f));

    // Adjust pan to zoom toward mouse position
    if (m_zoomLevel != prevZoom) {
      ImVec2 mouseGraphPos = CanvasToGraph(mousePos, canvasPos);
      float zoomRatio = m_zoomLevel / prevZoom;
      ImVec2 newMouseGraphPos = ImVec2(mouseGraphPos.x * zoomRatio, mouseGraphPos.y * zoomRatio);
      m_panOffset.x += (mouseGraphPos.x - newMouseGraphPos.x);
      m_panOffset.y += (mouseGraphPos.y - newMouseGraphPos.y);
    }
  }

  // Handle panning with middle mouse button
  if (m_isCanvasHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
      ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
      m_panOffset.x += dragDelta.x / m_zoomLevel;
      m_panOffset.y += dragDelta.y / m_zoomLevel;
    }
  }

  // Handle node selection with left mouse button
  if (m_isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    // Find the node under the cursor
    std::string nodeId = GetNodeAtPosition(mousePos, canvasPos);

    if (!nodeId.empty()) {
      // Select this node
      m_selectedNode = nodeId;
      m_logger->LogInfo("Selected node: " + nodeId);
    }
  }
}

void MotionGraphic::RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
  // Draw the canvas background
  drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
    IM_COL32(30, 30, 30, 255));

  // Draw a grid
  float gridSize = 50.0f * m_zoomLevel;
  ImU32 gridColor = IM_COL32(50, 50, 50, 200);

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

void MotionGraphic::DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end,
  ImU32 color, float thickness, bool isBidirectional) {
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

ImVec2 MotionGraphic::RealWorldToGraphCoord(const PositionStruct& pos) const {
  // This function converts from real-world position coordinates to 2D graph coordinates
  // In this implementation, we simplify by just using X and Y, but in a real application
  // you might want to use some kind of projection of the 3D space onto the 2D graph

  // Check if the graph exists
  auto graphOpt = m_configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) {
    return ImVec2(0, 0); // Default value if graph doesn't exist
  }

  const auto& graph = graphOpt.value().get();

  // Find the closest node and use its graph coordinates with an offset
  float minDistance = (std::numeric_limits<float>::max)();
  ImVec2 closestNodePos(0, 0);
  bool foundNode = false;

  for (const auto& node : graph.Nodes) {
    // Skip nodes with different device (they might be in a different coordinate space)
    if (!node.Device.empty() && !node.Position.empty()) {
      auto posOpt = m_configManager.GetNamedPosition(node.Device, node.Position);
      if (posOpt.has_value()) {
        const auto& nodePos = posOpt.value().get();

        // Fixed code using doubles instead of floats:
        double dx = nodePos.x - pos.x;
        double dy = nodePos.y - pos.y;
        double dz = nodePos.z - pos.z;

        float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

        if (distance < minDistance) {
          minDistance = distance;
          closestNodePos = ImVec2(static_cast<float>(node.X), static_cast<float>(node.Y));
          foundNode = true;
        }
      }
    }
  }

  // If we didn't find any node with a valid position, just return 0,0
  if (!foundNode) {
    return ImVec2(0, 0);
  }

  // Return the graph position of the closest node
  return closestNodePos;
}

std::string MotionGraphic::GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos) {
  auto graphOpt = m_configManager.GetGraph(m_activeGraph);
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

  // No node at this position
  return "";
}

bool MotionGraphic::MoveToNode(const std::string& controllerName, const std::string& nodeId) {
  if (controllerName.empty() || nodeId.empty()) {
    m_logger->LogError("MotionGraphic: Invalid controller or node ID");
    return false;
  }

  m_logger->LogInfo("MotionGraphic: Moving controller " + controllerName + " to node " + nodeId);

  // Use MachineOperations to move the device to the node
  bool success = m_machineOps.MoveDeviceToNode(controllerName, m_activeGraph, nodeId, false);

  if (success) {
    // Update the controller state to show it's moving
    std::lock_guard<std::mutex> lock(m_positionMutex);

    if (m_controllerStates.find(controllerName) != m_controllerStates.end()) {
      m_controllerStates[controllerName].isMoving = true;
      m_controllerStates[controllerName].targetNodeId = nodeId;
      m_controllerStates[controllerName].progress = 0.0;
    }

    m_logger->LogInfo("MotionGraphic: Successfully started movement to node " + nodeId);
  }
  else {
    m_logger->LogError("MotionGraphic: Failed to start movement to node " + nodeId);
  }

  return success;
}

void MotionGraphic::UpdatePositionsThreadFunc() {
  m_logger->LogInfo("MotionGraphic: Position update thread started");

  while (m_threadRunning) {
    // Get list of available devices
    std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

    // Update position for each device
    for (const auto& deviceName : deviceList) {
      ControllerState state;

      // Get the current position
      PositionStruct position;
      if (m_motionLayer.GetCurrentPosition(deviceName, position)) {
        state.position = position;
      }

      // Check if device is at a known node
      std::string currentNodeId;
      if (m_motionLayer.GetDeviceCurrentNode(m_activeGraph, deviceName, currentNodeId)) {
        state.currentNodeId = currentNodeId;
      }
      else {
        state.currentNodeId = "";
      }

      // Check if device is moving
      state.isMoving = m_machineOps.IsDeviceMoving(deviceName);

      // If device is moving and executing a path, get progress
      if (state.isMoving && m_motionLayer.IsExecuting()) {
        state.progress = m_motionLayer.GetPathProgress();
        state.targetNodeId = m_motionLayer.GetNextNodeId();
      }
      else {
        state.progress = 0.0;
        state.targetNodeId = "";
      }

      // Update the controller state
      {
        std::lock_guard<std::mutex> lock(m_positionMutex);
        m_controllerStates[deviceName] = state;
      }
    }

    // Sleep for the update interval
    std::this_thread::sleep_for(m_updateInterval);
  }

  m_logger->LogInfo("MotionGraphic: Position update thread stopped");
}

// The RenderNodes, RenderEdges, RenderControllerPositions, RenderControllerPanel and RenderNodePanel methods
// will be included separately due to their complexity and for clarity.

// Add these implementations to your MotionGraphic.cpp file



// Implementation of RenderControllerPanel
void MotionGraphic::RenderControllerPanel() {
  ImGui::Text("Controllers");
  ImGui::Separator();

  // Get a list of enabled devices
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  if (deviceList.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No controllers available");
    return;
  }

  // Controller selection
  if (ImGui::BeginCombo("Select Controller", m_selectedController.empty() ?
    "Select a controller" : m_selectedController.c_str())) {

    for (const auto& device : deviceList) {
      bool isSelected = (m_selectedController == device);
      if (ImGui::Selectable(device.c_str(), isSelected)) {
        m_selectedController = device;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // If a controller is selected, show its details
  if (!m_selectedController.empty()) {
    ImGui::Spacing();

    // Get device information
    bool isConnected = m_machineOps.IsDeviceConnected(m_selectedController);

    // Display status
    ImGui::Text("Status: %s", isConnected ? "Connected" : "Disconnected");

    {
      std::lock_guard<std::mutex> lock(m_positionMutex);
      auto it = m_controllerStates.find(m_selectedController);
      if (it != m_controllerStates.end()) {
        const auto& state = it->second;

        // Display current position
        ImGui::Spacing();
        ImGui::Text("Current Position:");
        ImGui::Text("X: %.4f", state.position.x);
        ImGui::Text("Y: %.4f", state.position.y);
        ImGui::Text("Z: %.4f", state.position.z);

        // Only show rotation values if any are non-zero
        if (state.position.u != 0.0 || state.position.v != 0.0 || state.position.w != 0.0) {
          ImGui::Text("U: %.4f", state.position.u);
          ImGui::Text("V: %.4f", state.position.v);
          ImGui::Text("W: %.4f", state.position.w);
        }

        // Display current node and motion status
        ImGui::Spacing();
        if (!state.currentNodeId.empty()) {
          ImGui::Text("Current Node: %s", state.currentNodeId.c_str());
        }
        else {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Not at a known node");
        }

        if (state.isMoving) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Status: Moving");

          if (!state.targetNodeId.empty()) {
            ImGui::Text("Target Node: %s", state.targetNodeId.c_str());
            ImGui::ProgressBar(static_cast<float>(state.progress), ImVec2(-1, 0));
          }
        }
        else {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Status: Idle");
        }
      }
    }

    // Movement controls
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Movement Controls");

    // Move to selected node
    if (!m_selectedNode.empty()) {
      std::string buttonText = "Move to Node: " + m_selectedNode;

      if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 0))) {
        // Execute the move
        MoveToNode(m_selectedController, m_selectedNode);
      }
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Select a node to move to");
    }

    // Home button
    if (ImGui::Button("Home Device", ImVec2(-1, 0))) {
      // Find a node with "home" in its name or position for this device
      auto graphOpt = m_configManager.GetGraph(m_activeGraph);
      if (graphOpt.has_value()) {
        const auto& graph = graphOpt.value().get();

        for (const auto& node : graph.Nodes) {
          if (node.Device == m_selectedController &&
            (node.Position.find("home") != std::string::npos ||
              node.Label.find("home") != std::string::npos ||
              node.Id.find("home") != std::string::npos)) {

            MoveToNode(m_selectedController, node.Id);
            break;
          }
        }
      }
    }
  }
}

// Implementation of RenderNodePanel
void MotionGraphic::RenderNodePanel() {
  ImGui::Text("Node Information");
  ImGui::Separator();

  if (m_selectedNode.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No node selected");
    return;
  }

  // Get node information
  auto graphOpt = m_configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  const Node* selectedNode = nullptr;
  for (const auto& node : graph.Nodes) {
    if (node.Id == m_selectedNode) {
      selectedNode = &node;
      break;
    }
  }

  if (!selectedNode) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Selected node not found");
    return;
  }

  // Display node details
  ImGui::Text("ID: %s", selectedNode->Id.c_str());

  if (!selectedNode->Label.empty()) {
    ImGui::Text("Label: %s", selectedNode->Label.c_str());
  }

  ImGui::Text("Graph Position:");
  ImGui::Text("X: %d, Y: %d", selectedNode->X, selectedNode->Y);

  ImGui::Spacing();
  ImGui::Text("Device: %s", selectedNode->Device.c_str());
  ImGui::Text("Position: %s", selectedNode->Position.c_str());

  // Get the actual position coordinates from config if available
  if (!selectedNode->Position.empty() && !selectedNode->Device.empty()) {
    auto posOpt = m_configManager.GetNamedPosition(selectedNode->Device, selectedNode->Position);
    if (posOpt.has_value()) {
      const auto& pos = posOpt.value().get();

      ImGui::Spacing();
      ImGui::Text("Real World Coordinates:");
      ImGui::Text("X: %.4f", pos.x);
      ImGui::Text("Y: %.4f", pos.y);
      ImGui::Text("Z: %.4f", pos.z);

      // Only show rotation values if any are non-zero
      if (pos.u != 0.0 || pos.v != 0.0 || pos.w != 0.0) {
        ImGui::Text("U: %.4f", pos.u);
        ImGui::Text("V: %.4f", pos.v);
        ImGui::Text("W: %.4f", pos.w);
      }
    }
  }

  // Show connected nodes
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Connected Nodes");

  int connectedNodeCount = 0;

  for (const auto& edge : graph.Edges) {
    bool isSource = (edge.Source == m_selectedNode);
    bool isTarget = (edge.Target == m_selectedNode);
    bool isBidirectional = edge.Conditions.IsBidirectional;

    if (isSource || isTarget) {
      connectedNodeCount++;

      // Get the connected node ID
      std::string connectedNodeId = isSource ? edge.Target : edge.Source;

      // Get the connected node's label if possible
      std::string connectedNodeLabel;
      for (const auto& node : graph.Nodes) {
        if (node.Id == connectedNodeId) {
          connectedNodeLabel = node.Label;
          break;
        }
      }

      // Direction indicator
      std::string directionIndicator;
      if (isBidirectional) {
        directionIndicator = " = ";
      }
      else if (isSource) {
        directionIndicator = " < ";
      }
      else {
        directionIndicator = " > ";
      }

      // Create connection info text
      std::string connectionInfo = directionIndicator;
      if (!connectedNodeLabel.empty()) {
        connectionInfo += connectedNodeLabel + " (" + connectedNodeId + ")";
      }
      else {
        connectionInfo += connectedNodeId;
      }

      // Show connection with button to select that node
      if (ImGui::Button(connectionInfo.c_str(), ImVec2(-1, 0))) {
        m_selectedNode = connectedNodeId;
      }
    }
  }

  if (connectedNodeCount == 0) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No connected nodes");
  }
}

// Implementation of RenderControllerPositions
void MotionGraphic::RenderControllerPositions(ImDrawList* drawList, const ImVec2& canvasPos) {
  std::lock_guard<std::mutex> lock(m_positionMutex);

  // For each controller, render its position as a marker
  for (const auto& [deviceName, state] : m_controllerStates) {
    // Convert real-world position to graph coordinates
    ImVec2 graphPos = RealWorldToGraphCoord(state.position);

    // Skip if position is invalid
    if (std::isnan(graphPos.x) || std::isnan(graphPos.y) ||
      std::isinf(graphPos.x) || std::isinf(graphPos.y)) {
      continue;
    }

    // Convert to canvas coordinates
    ImVec2 canvasPos2D = GraphToCanvas(graphPos, canvasPos);

    // Draw a diamond marker for the controller position
    const float markerSize = MARKER_SIZE * m_zoomLevel;

    // Get color based on selection state
    ImU32 markerColor = (m_selectedController == deviceName) ?
      IM_COL32(255, 165, 0, 255) : // Orange if selected
      CONTROLLER_POSITION_COLOR;

    // Draw diamond marker
    drawList->AddQuad(
      ImVec2(canvasPos2D.x, canvasPos2D.y - markerSize),
      ImVec2(canvasPos2D.x + markerSize, canvasPos2D.y),
      ImVec2(canvasPos2D.x, canvasPos2D.y + markerSize),
      ImVec2(canvasPos2D.x - markerSize, canvasPos2D.y),
      markerColor,
      2.0f
    );

    // Draw label above the marker
    std::string label = deviceName;
    if (state.isMoving) {
      label += " (Moving)";
    }

    ImVec2 textSize = ImGui::CalcTextSize(label.c_str());

    // Draw a background box for the text
    drawList->AddRectFilled(
      ImVec2(canvasPos2D.x - textSize.x / 2 - 4, canvasPos2D.y - markerSize - textSize.y - 4),
      ImVec2(canvasPos2D.x + textSize.x / 2 + 4, canvasPos2D.y - markerSize - 2),
      IM_COL32(40, 40, 40, 200),
      3.0f
    );

    // Draw the device name
    drawList->AddText(
      ImVec2(canvasPos2D.x - textSize.x / 2, canvasPos2D.y - markerSize - textSize.y - 2),
      IM_COL32(220, 220, 220, 255),
      label.c_str()
    );

    // If device is moving and has a progress value, draw a progress bar
    if (state.isMoving && !state.targetNodeId.empty() && state.progress > 0.0) {
      const float progressBarWidth = 40.0f * m_zoomLevel;
      const float progressBarHeight = 4.0f * m_zoomLevel;
      const float progress = static_cast<float>(state.progress);

      ImVec2 barPos = ImVec2(canvasPos2D.x - progressBarWidth / 2, canvasPos2D.y + markerSize + 4);

      // Background
      drawList->AddRectFilled(
        barPos,
        ImVec2(barPos.x + progressBarWidth, barPos.y + progressBarHeight),
        IM_COL32(70, 70, 70, 200)
      );

      // Filled portion
      drawList->AddRectFilled(
        barPos,
        ImVec2(barPos.x + progressBarWidth * progress, barPos.y + progressBarHeight),
        IM_COL32(50, 220, 50, 255)
      );
    }
  }
}

// Implementation of RenderNodes
void MotionGraphic::RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos) {
  auto graphOpt = m_configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  // Lock for thread safety when accessing controller states
  std::lock_guard<std::mutex> lock(m_positionMutex);

  // Create maps to track current and target nodes for each device
  std::map<std::string, std::set<std::string>> currentNodesByDevice;
  std::map<std::string, std::set<std::string>> targetNodesByDevice;

  // Fill the maps by iterating through controller states
  for (const auto& [deviceName, state] : m_controllerStates) {
    if (!state.currentNodeId.empty()) {
      currentNodesByDevice[deviceName].insert(state.currentNodeId);
    }
    if (!state.targetNodeId.empty()) {
      targetNodesByDevice[deviceName].insert(state.targetNodeId);
    }
  }

  // Render each node
  for (const auto& node : graph.Nodes) {
    // Get node center position
    ImVec2 nodePos = GetNodePosition(node);

    // Convert to canvas coordinates
    ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);

    // Calculate node rectangle positions
    ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);
    ImVec2 nodeMax = ImVec2(canvasNodePos.x + NODE_WIDTH / 2, canvasNodePos.y + NODE_HEIGHT / 2);

    // Determine node color based on status
    ImU32 fillColor = NODE_COLOR;

    // Selected node
    if (m_selectedNode == node.Id) {
      fillColor = SELECTED_NODE_COLOR;
    }

    // Check if any controller is at this node
    bool isCurrentNode = false;
    bool isTargetNode = false;
    std::string deviceAtNode;

    // Check if this is a current node for any device
    for (const auto& [device, nodeIds] : currentNodesByDevice) {
      if (nodeIds.find(node.Id) != nodeIds.end()) {
        isCurrentNode = true;
        deviceAtNode = device;
        break;
      }
    }

    // Check if this is a target node for any device
    for (const auto& [device, nodeIds] : targetNodesByDevice) {
      if (nodeIds.find(node.Id) != nodeIds.end()) {
        isTargetNode = true;
        // Don't break, as we want to prioritize current node status
        if (deviceAtNode.empty()) {
          deviceAtNode = device;
        }
      }
    }

    // Current node takes precedence over target node
    if (isCurrentNode) {
      fillColor = CURRENT_NODE_COLOR;
    }
    else if (isTargetNode) {
      fillColor = TARGET_NODE_COLOR;
    }

    // Draw node background rectangle
    drawList->AddRectFilled(nodeMin, nodeMax, fillColor, NODE_ROUNDING);

    // Draw thicker border if this is the current or target node
    float borderThickness = (isCurrentNode || isTargetNode || m_selectedNode == node.Id) ? 2.0f : 1.0f;
    drawList->AddRect(nodeMin, nodeMax, NODE_BORDER_COLOR, NODE_ROUNDING, 0, borderThickness);

    // Node ID
    std::string nodeIdText = "ID: " + node.Id;
    ImVec2 idTextSize = ImGui::CalcTextSize(nodeIdText.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - idTextSize.x / 2, nodeMin.y + TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      nodeIdText.c_str()
    );

    // Node label with more prominence
    std::string nodeLabel = node.Label.empty() ? "No Label" : node.Label;
    ImVec2 textSize = ImGui::CalcTextSize(nodeLabel.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - textSize.x / 2, nodeMin.y + idTextSize.y + 2 * TEXT_PADDING),
      IM_COL32(255, 255, 255, 255), // Brighter white for label
      nodeLabel.c_str()
    );

    // Device info
    std::string deviceInfo = "Device: " + node.Device;
    ImVec2 deviceTextSize = ImGui::CalcTextSize(deviceInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - deviceTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + 3 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      deviceInfo.c_str()
    );

    // Position info
    std::string posInfo = "Pos: " + node.Position;
    ImVec2 posTextSize = ImGui::CalcTextSize(posInfo.c_str());
    drawList->AddText(
      ImVec2(canvasNodePos.x - posTextSize.x / 2, nodeMin.y + idTextSize.y + textSize.y + deviceTextSize.y + 4 * TEXT_PADDING),
      IM_COL32(200, 200, 200, 255),
      posInfo.c_str()
    );

    // If a device is at this node, show it
    if (!deviceAtNode.empty()) {
      std::string statusText = isCurrentNode ? "Current: " : "Target: ";
      statusText += deviceAtNode;
      ImVec2 statusTextSize = ImGui::CalcTextSize(statusText.c_str());
      ImU32 statusColor = isCurrentNode ? CURRENT_NODE_COLOR : TARGET_NODE_COLOR;

      drawList->AddText(
        ImVec2(canvasNodePos.x - statusTextSize.x / 2,
          nodeMin.y + idTextSize.y + textSize.y + deviceTextSize.y + posTextSize.y + 5 * TEXT_PADDING),
        statusColor,
        statusText.c_str()
      );
    }
  }
}

// Implementation of RenderEdges
void MotionGraphic::RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos) {
  auto graphOpt = m_configManager.GetGraph(m_activeGraph);
  if (!graphOpt.has_value()) return;

  const auto& graph = graphOpt.value().get();

  // Create a map of nodes by ID for quick lookup
  std::map<std::string, const Node*> nodeMap;
  for (const auto& node : graph.Nodes) {
    nodeMap[node.Id] = &node;
  }

  // First, render all regular edges
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

    // Calculate positions at the edge of each node rectangle
    float halfNodeWidth = NODE_WIDTH / 2.0f;
    float halfNodeHeight = NODE_HEIGHT / 2.0f;

    // For source: move from center toward edge in the direction of the target
    ImVec2 sourceEdge;
    if (std::abs(dir.x * halfNodeHeight) > std::abs(dir.y * halfNodeWidth)) {
      // Intersect with left or right edge
      sourceEdge.x = canvasSourcePos.x + (dir.x > 0 ? halfNodeWidth : -halfNodeWidth);
      sourceEdge.y = canvasSourcePos.y + dir.y * (halfNodeWidth / std::abs(dir.x));
    }
    else {
      // Intersect with top or bottom edge
      sourceEdge.x = canvasSourcePos.x + dir.x * (halfNodeHeight / std::abs(dir.y));
      sourceEdge.y = canvasSourcePos.y + (dir.y > 0 ? halfNodeHeight : -halfNodeHeight);
    }

    // For target: move from center toward edge in the direction of the source
    ImVec2 targetEdge;
    if (std::abs(-dir.x * halfNodeHeight) > std::abs(-dir.y * halfNodeWidth)) {
      // Intersect with left or right edge
      targetEdge.x = canvasTargetPos.x + (-dir.x > 0 ? halfNodeWidth : -halfNodeWidth);
      targetEdge.y = canvasTargetPos.y + -dir.y * (halfNodeWidth / std::abs(dir.x));
    }
    else {
      // Intersect with top or bottom edge
      targetEdge.x = canvasTargetPos.x + -dir.x * (halfNodeHeight / std::abs(dir.y));
      targetEdge.y = canvasTargetPos.y + (-dir.y > 0 ? halfNodeHeight : -halfNodeHeight);
    }

    // Check if edge is bidirectional
    bool isBidirectional = edge.Conditions.IsBidirectional;

    // Use green color for bidirectional edges, otherwise use regular edge color
    ImU32 edgeColor = isBidirectional ? BIDIRECTIONAL_EDGE_COLOR : EDGE_COLOR;

    // Draw the edge
    DrawArrow(drawList, sourceEdge, targetEdge, edgeColor, EDGE_THICKNESS, isBidirectional);

    // Add edge label at midpoint
    ImVec2 midpoint = ImVec2((sourceEdge.x + targetEdge.x) * 0.5f, (sourceEdge.y + targetEdge.y) * 0.5f);
    std::string edgeLabel = edge.Label.empty() ? edge.Id : edge.Label;

    ImVec2 labelSize = ImGui::CalcTextSize(edgeLabel.c_str());

    // Draw background for better visibility
    drawList->AddRectFilled(
      ImVec2(midpoint.x - labelSize.x / 2 - 4, midpoint.y - labelSize.y / 2 - 2),
      ImVec2(midpoint.x + labelSize.x / 2 + 4, midpoint.y + labelSize.y / 2 + 2),
      IM_COL32(40, 40, 40, 200),
      3.0f
    );

    drawList->AddText(
      ImVec2(midpoint.x - labelSize.x / 2, midpoint.y - labelSize.y / 2),
      IM_COL32(220, 220, 220, 255),
      edgeLabel.c_str()
    );
  }
}