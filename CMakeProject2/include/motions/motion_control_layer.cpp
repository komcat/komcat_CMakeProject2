// motion_control_layer.cpp
#include "include/motions/motion_control_layer.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <chrono>

MotionControlLayer::MotionControlLayer(MotionConfigManager& configManager,
  PIControllerManager& piControllerManager,
  ACSControllerManager& acsControllerManager)
  : m_configManager(configManager),
  m_piControllerManager(piControllerManager),
  m_acsControllerManager(acsControllerManager),
  m_isExecuting(false),
  m_cancelRequested(false),
  m_threadRunning(false),
  m_currentNodeIndex(0)
{
  m_logger = Logger::GetInstance();

  // Start the execution thread
  m_threadRunning = true;
  m_executionThread = std::thread(&MotionControlLayer::ExecutionThreadFunc, this);

  m_logger->LogInfo("MotionControlLayer: Initialized");
}

MotionControlLayer::~MotionControlLayer() {
  // Signal the thread to stop and wait for it to finish
  m_threadRunning = false;
  m_cancelRequested = true;

  // Notify the condition variable to wake up the thread
  m_cv.notify_one();

  if (m_executionThread.joinable()) {
    m_executionThread.join();
  }

  m_logger->LogInfo("MotionControlLayer: Shutdown complete");
}

bool MotionControlLayer::PlanPath(const std::string& graphName, const std::string& startNodeId, const std::string& endNodeId) {
  if (m_isExecuting) {
    m_logger->LogWarning("MotionControlLayer: Cannot plan path while executing");
    return false;
  }

  // Clear any existing path
  m_plannedPath.clear();
  m_currentNodeIndex = 0;
  m_currentGraphName = graphName;

  // Detailed logging of path planning attempt
  m_logger->LogInfo("Planning path in graph: " + graphName);
  m_logger->LogInfo("Start Node: " + startNodeId);
  m_logger->LogInfo("End Node: " + endNodeId);

  // Fetch the graph configuration
  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    m_logger->LogError("Graph not found: " + graphName);
    return false;
  }

  const auto& graph = graphOpt.value().get();

  // Log all edges in the graph
  m_logger->LogInfo("Graph Edges:");
  for (const auto& edge : graph.Edges) {
    m_logger->LogInfo("Edge: " + edge.Source + " -> " + edge.Target +
      " (Bidirectional: " +
      (edge.Conditions.IsBidirectional ? "Yes" : "No") + ")");
  }

  // Use the config manager to find a path between nodes
  m_plannedPath = m_configManager.FindPath(graphName, startNodeId, endNodeId);

  if (m_plannedPath.empty()) {
    m_logger->LogError("MotionControlLayer: No path found from " + startNodeId + " to " + endNodeId);
    return false;
  }

  std::stringstream ss;
  ss << "MotionControlLayer: Path planned with " << m_plannedPath.size() << " nodes: ";
  for (size_t i = 0; i < m_plannedPath.size(); i++) {
    ss << m_plannedPath[i].get().Id;
    if (i < m_plannedPath.size() - 1) {
      ss << " -> ";
    }
  }
  m_logger->LogInfo(ss.str());

  return true;
}




bool MotionControlLayer::ExecutePath(bool blocking) {
  if (m_isExecuting) {
    m_logger->LogWarning("MotionControlLayer: Already executing a path");
    return false;
  }

  if (m_plannedPath.empty()) {
    m_logger->LogWarning("MotionControlLayer: No path to execute");
    return false;
  }

  // Reset execution state
  m_currentNodeIndex = 0;
  m_isExecuting = true;
  m_cancelRequested = false;

  // Notify the execution thread to start processing
  m_cv.notify_one();

  m_logger->LogInfo("MotionControlLayer: Starting execution of path with " +
    std::to_string(m_plannedPath.size()) + " nodes");

  // If blocking is requested, wait until execution is complete
  if (blocking) {
    while (m_isExecuting && !m_cancelRequested) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return !m_cancelRequested;
  }

  return true;
}

void MotionControlLayer::CancelExecution() {
  if (!m_isExecuting) {
    return;
  }

  m_logger->LogInfo("MotionControlLayer: Cancelling execution");
  m_cancelRequested = true;

  // Wait for execution to finish
  while (m_isExecuting) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  m_logger->LogInfo("MotionControlLayer: Execution cancelled");
}

void MotionControlLayer::SetPathCompletionCallback(CompletionCallback callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_pathCompletionCallback = callback;
}

void MotionControlLayer::SetSequenceCompletionCallback(CompletionCallback callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_sequenceCompletionCallback = callback;
}

std::string MotionControlLayer::GetCurrentNodeId() const {
  if (m_plannedPath.empty() || m_currentNodeIndex >= m_plannedPath.size()) {
    return "";
  }
  return m_plannedPath[m_currentNodeIndex].get().Id;
}

std::string MotionControlLayer::GetNextNodeId() const {
  if (m_plannedPath.empty() || m_currentNodeIndex + 1 >= m_plannedPath.size()) {
    return "";
  }
  return m_plannedPath[m_currentNodeIndex + 1].get().Id;
}

double MotionControlLayer::GetPathProgress() const {
  if (m_plannedPath.empty()) {
    return 0.0;
  }
  if (m_plannedPath.size() == 1) {
    return m_isExecuting ? 0.5 : 1.0;
  }
  return static_cast<double>(m_currentNodeIndex) / static_cast<double>(m_plannedPath.size() - 1);
}

void MotionControlLayer::ExecutionThreadFunc() {
  m_logger->LogInfo("MotionControlLayer: Execution thread started");

  while (m_threadRunning) {
    // Wait for execution signal or shutdown
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]() { return m_isExecuting || !m_threadRunning; });

    // Exit if shutting down
    if (!m_threadRunning) {
      break;
    }

    // If we're not supposed to be executing, continue waiting
    if (!m_isExecuting) {
      continue;
    }

    lock.unlock();

    // Log execution start
    m_logger->LogInfo("MotionControlLayer: Starting execution of path with " +
      std::to_string(m_plannedPath.size()) + " nodes in graph " +
      m_currentGraphName);

    // Execute the path
    bool success = true;
    auto startTime = std::chrono::steady_clock::now();

    for (size_t i = 0; i < m_plannedPath.size() && !m_cancelRequested; i++) {
      m_currentNodeIndex = i;
      const Node& currentNode = m_plannedPath[i].get();

      // Validate transition to next node if not the first node
      if (i > 0) {
        const Node& prevNode = m_plannedPath[i - 1].get();
        if (!ValidateNodeTransition(prevNode, currentNode)) {
          m_logger->LogError("MotionControlLayer: Invalid transition from " +
            prevNode.Id + " to " + currentNode.Id);
          success = false;
          break;
        }
      }

      // Log node execution start with detailed information
      std::stringstream nodeInfoLog;
      nodeInfoLog << "MotionControlLayer: Executing node " << i + 1 << "/" << m_plannedPath.size()
        << ": " << currentNode.Id;
      if (!currentNode.Label.empty()) {
        nodeInfoLog << " (Label: " << currentNode.Label << ")";
      }
      nodeInfoLog << " - Device: " << currentNode.Device
        << ", Position: " << currentNode.Position;

      m_logger->LogInfo(nodeInfoLog.str());

      // Execute node movement with our new polling-based approach
      if (!MoveToNode(currentNode)) {
        m_logger->LogError("MotionControlLayer: Failed to move to node " + currentNode.Id);
        success = false;
        break;
      }

      // Log successful movement completion
      m_logger->LogInfo("MotionControlLayer: Successfully moved to node " + currentNode.Id);

      // Check for cancellation after each node
      if (m_cancelRequested) {
        m_logger->LogWarning("MotionControlLayer: Execution cancelled during node " +
          std::to_string(i + 1) + "/" + std::to_string(m_plannedPath.size()));
        success = false;
        break;
      }
    }

    // Calculate execution time
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Set execution complete
    m_isExecuting = false;

    // Call completion callback if set
    CompletionCallback callback = nullptr;
    {
      std::lock_guard<std::mutex> callbackLock(m_mutex);
      callback = m_pathCompletionCallback;
    }

    if (callback) {
      callback(!m_cancelRequested && success);
    }

    if (m_cancelRequested) {
      m_logger->LogInfo("MotionControlLayer: Path execution cancelled after " +
        std::to_string(duration) + "ms");
    }
    else if (!success) {
      m_logger->LogError("MotionControlLayer: Path execution failed after " +
        std::to_string(duration) + "ms");
    }
    else {
      m_logger->LogInfo("MotionControlLayer: Path execution completed successfully in " +
        std::to_string(duration) + "ms");
    }
  }

  m_logger->LogInfo("MotionControlLayer: Execution thread stopped");
}

bool MotionControlLayer::MoveToNode(const Node& node) {
  // Skip if no device or position specified
  if (node.Device.empty() || node.Position.empty()) {
    m_logger->LogWarning("MotionControlLayer: Node " + node.Id + " missing device or position");
    return false;
  }

  // Get the position from configuration
  auto posOpt = m_configManager.GetNamedPosition(node.Device, node.Position);
  if (!posOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Position " + node.Position + " not found for device " + node.Device);
    return false;
  }

  const auto& targetPosition = posOpt.value().get();

  // Log detailed position information
  std::stringstream positionLog;
  positionLog << "MotionControlLayer: Moving to node " << node.Id
    << " (Label: " << node.Label << ")"
    << " Device: " << node.Device
    << ", Position: " << node.Position
    << " - Coordinates: "
    << "X:" << targetPosition.x << ", "
    << "Y:" << targetPosition.y << ", "
    << "Z:" << targetPosition.z;

  // Include rotation values if any are non-zero
  if (targetPosition.u != 0.0 || targetPosition.v != 0.0 || targetPosition.w != 0.0) {
    positionLog << ", U:" << targetPosition.u << ", "
      << "V:" << targetPosition.v << ", "
      << "W:" << targetPosition.w;
  }

  m_logger->LogInfo(positionLog.str());

  // Determine which controller manager to use and move to the position
  bool moveCommandSent = false;
  if (IsDevicePIController(node.Device)) {
    m_logger->LogInfo("MotionControlLayer: Using PI controller for device " + node.Device);
    moveCommandSent = m_piControllerManager.MoveToNamedPosition(node.Device, node.Position, false); // Non-blocking!
  }
  else {
    m_logger->LogInfo("MotionControlLayer: Using ACS controller for device " + node.Device);
    moveCommandSent = m_acsControllerManager.MoveToNamedPosition(node.Device, node.Position, false); // Non-blocking!
  }

  if (!moveCommandSent) {
    m_logger->LogError("MotionControlLayer: Failed to send move command for node " + node.Id);
    return false;
  }

  // Now actively poll until position is reached
  m_logger->LogInfo("MotionControlLayer: Waiting for device " + node.Device + " to reach position");
  bool positionReached = WaitForPositionReached(node.Device, node.Position, targetPosition);

  if (positionReached) {
    m_logger->LogInfo("MotionControlLayer: Position reached for node " + node.Id);
    UpdateDevicePosition(node.Device);
    return true;
  }
  else {
    m_logger->LogError("MotionControlLayer: Timeout waiting for position to be reached for node " + node.Id);
    return false;
  }
}


bool MotionControlLayer::ValidateNodeTransition(const Node& fromNode, const Node& toNode) {
  // Get the graph
  auto graphOpt = m_configManager.GetGraph(m_currentGraphName);
  if (!graphOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Graph " + m_currentGraphName + " not found");
    return false;
  }

  const auto& graph = graphOpt.value().get();

  // Log the nodes we're trying to transition between
  m_logger->LogInfo("Checking transition from " + fromNode.Id + " to " + toNode.Id);

  // Find an edge connecting these nodes
  bool edgeFound = false;
  Edge foundEdge;

  for (const auto& edge : graph.Edges) {
    bool directConnection = (edge.Source == fromNode.Id && edge.Target == toNode.Id);
    bool bidirectionalConnection = (edge.Conditions.IsBidirectional &&
      edge.Source == toNode.Id && edge.Target == fromNode.Id);

    // Log each edge check
    m_logger->LogInfo("Checking edge: " + edge.Source + " -> " + edge.Target +
      ", Bidirectional: " + (edge.Conditions.IsBidirectional ? "true" : "false"));

    if (directConnection || bidirectionalConnection) {
      edgeFound = true;
      foundEdge = edge;
      m_logger->LogInfo("Found valid edge: " + foundEdge.Source + " -> " + foundEdge.Target);
      break;
    }
  }

  if (!edgeFound) {
    m_logger->LogError("MotionControlLayer: No valid edge found between nodes " +
      fromNode.Id + " and " + toNode.Id);
    return false;
  }

  // Check if operator approval is required
  if (foundEdge.Conditions.RequiresOperatorApproval) {
    m_logger->LogInfo("MotionControlLayer: Edge " + foundEdge.Id + " requires operator approval");
    // In a real implementation, we would show a prompt and wait for approval
    // For now, just simulate approval
    std::this_thread::sleep_for(std::chrono::seconds(1));
    m_logger->LogInfo("MotionControlLayer: Operator approval granted for edge " + foundEdge.Id);
  }

  return true;
}

bool MotionControlLayer::IsDevicePIController(const std::string& deviceName) const {
  // Get the device info from configuration
  auto deviceOpt = m_configManager.GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // PI controllers use port 50000, ACS uses different ports (like 701)
  return (device.Port == 50000);
}


void MotionControlLayer::RenderUI() {
  // Simple UI for monitoring and control
  if (!ImGui::Begin("Motion Control")) {
    ImGui::End();
    return;
  }

  ImGui::Text("Path Execution Status: %s", m_isExecuting ? "Running" : "Idle");

  if (!m_plannedPath.empty()) {
    ImGui::Text("Planned Path: %zu nodes", m_plannedPath.size());

    // Show current node with label
    if (m_currentNodeIndex < m_plannedPath.size()) {
      const Node& currentNode = m_plannedPath[m_currentNodeIndex].get();
      std::string currentNodeLabel = currentNode.Label.empty() ? currentNode.Id : currentNode.Label;
      ImGui::Text("Current Node: %s (%s)", currentNodeLabel.c_str(), currentNode.Id.c_str());
    }
    else {
      ImGui::Text("Current Node: %zu / %zu", m_currentNodeIndex + 1, m_plannedPath.size());
    }

    // Progress bar
    float progress = static_cast<float>(GetPathProgress());
    ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

    if (m_isExecuting) {
      if (ImGui::Button("Cancel Execution")) {
        CancelExecution();
      }
    }
    else {
      if (ImGui::Button("Execute Path")) {
        ExecutePath();
      }
    }
  }
  else {
    ImGui::Text("No path planned");
  }

  // Path Planning section
  ImGui::Separator();
  ImGui::Text("Path Planning");

  // Declare graphName variable
  static char graphName[128] = "Process_Flow"; // Default to Process_Flow
  static char startNode[128] = "";
  static char endNode[128] = "";
  static bool startNodeInitialized = false;
  static bool endNodeInitialized = false;

  // Get all available graphs
  const auto& allGraphs = m_configManager.GetAllGraphs();
  if (ImGui::BeginCombo("Graph", graphName)) {
    for (const auto& [name, graph] : allGraphs) {
      bool isSelected = (strcmp(graphName, name.c_str()) == 0);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        strcpy_s(graphName, sizeof(graphName), name.c_str());
        // Reset node selections when graph changes
        startNodeInitialized = false;
        endNodeInitialized = false;
        memset(startNode, 0, sizeof(startNode));
        memset(endNode, 0, sizeof(endNode));
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Try to determine current node position for default
  if (!startNodeInitialized) {
    // Find a device from the graph
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      if (!graph.Nodes.empty()) {
        const auto& firstNode = graph.Nodes.front();
        std::string deviceName = firstNode.Device;
        if (!deviceName.empty()) {
          std::string currentNodeId;
          if (GetDeviceCurrentNode(graphName, deviceName, currentNodeId) && !currentNodeId.empty()) {
            strcpy_s(startNode, sizeof(startNode), currentNodeId.c_str());
          }
          else {
            // Default to first node if no match
            strcpy_s(startNode, sizeof(startNode), graph.Nodes.front().Id.c_str());
          }
          startNodeInitialized = true;
        }
      }
    }
  }

  // Initialize end node to something valid if empty
  if (!endNodeInitialized) {
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      if (!graph.Nodes.empty()) {
        if (graph.Nodes.size() > 1) {
          // Use second node if available
          strcpy_s(endNode, sizeof(endNode), graph.Nodes[1].Id.c_str());
        }
        else {
          // Use the first node if only one exists
          strcpy_s(endNode, sizeof(endNode), graph.Nodes.front().Id.c_str());
        }
        endNodeInitialized = true;
      }
    }
  }

  // Start Node dropdown with labels - now with current position highlighting
  if (ImGui::BeginCombo("Start Node", GetNodeLabelAndId(graphName, startNode).c_str())) {
    auto nodes = GetAllNodesWithLabels(graphName);
    for (const auto& [id, label] : nodes) {
      std::string displayText = label + " (" + id + ")";
      bool isSelected = (std::string(startNode) == id);

      // Highlight if this is the current position
      bool isCurrentPosition = IsDeviceAtNode(graphName, id);
      if (isCurrentPosition) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green text
        displayText += " [CURRENT]";
      }

      if (ImGui::Selectable(displayText.c_str(), isSelected)) {
        strcpy_s(startNode, sizeof(startNode), id.c_str());
      }

      if (isCurrentPosition) {
        ImGui::PopStyleColor();
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // End Node dropdown with labels - similar highlighting
  if (ImGui::BeginCombo("End Node", GetNodeLabelAndId(graphName, endNode).c_str())) {
    auto nodes = GetAllNodesWithLabels(graphName);
    for (const auto& [id, label] : nodes) {
      std::string displayText = label + " (" + id + ")";
      bool isSelected = (std::string(endNode) == id);

      // Highlight if this is the current position
      bool isCurrentPosition = IsDeviceAtNode(graphName, id);
      if (isCurrentPosition) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green text
        displayText += " [CURRENT]";
      }

      if (ImGui::Selectable(displayText.c_str(), isSelected)) {
        strcpy_s(endNode, sizeof(endNode), id.c_str());
      }

      if (isCurrentPosition) {
        ImGui::PopStyleColor();
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Add a "Use Current Position" button
  if (ImGui::Button("Use Current Position as Start")) {
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      if (!graph.Nodes.empty()) {
        const auto& firstNode = graph.Nodes.front();
        std::string deviceName = firstNode.Device;
        if (!deviceName.empty()) {
          std::string currentNodeId;
          if (GetDeviceCurrentNode(graphName, deviceName, currentNodeId) && !currentNodeId.empty()) {
            strcpy_s(startNode, sizeof(startNode), currentNodeId.c_str());
            m_logger->LogInfo("MotionControlLayer: Set start node to current position: " + currentNodeId);
          }
          else {
            m_logger->LogWarning("MotionControlLayer: Could not determine current position");
          }
        }
      }
    }
  }

  ImGui::SameLine();

  if (ImGui::Button("Plan Path")) {
    PlanPath(graphName, startNode, endNode);
  }

  // Display current positions section
  ImGui::Separator();
  ImGui::Text("Current Positions:");

  for (const auto& [deviceName, position] : m_deviceCurrentPositions) {
    ImGui::Text("%s: X:%.3f Y:%.3f Z:%.3f",
      deviceName.c_str(), position.x, position.y, position.z);

    std::string nodeId;
    if (GetDeviceCurrentNode(graphName, deviceName, nodeId)) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f),
        "[%s]", GetNodeLabelAndId(graphName, nodeId).c_str());
    }
  }

  ImGui::End();
}




std::string MotionControlLayer::GetNodeLabel(const std::string& graphName, const std::string& nodeId) const {
  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    return "Unknown";
  }

  const auto& graph = graphOpt.value().get();

  for (const auto& node : graph.Nodes) {
    if (node.Id == nodeId) {
      return node.Label;
    }
  }

  return "Unknown";
}

std::string MotionControlLayer::GetNodeLabelAndId(const std::string& graphName, const std::string& nodeId) const {
  std::string label = GetNodeLabel(graphName, nodeId);

  if (label == "Unknown") {
    return nodeId;
  }

  return label + " (" + nodeId + ")";
}

std::vector<std::pair<std::string, std::string>> MotionControlLayer::GetAllNodesWithLabels(const std::string& graphName) const {
  std::vector<std::pair<std::string, std::string>> result;

  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    return result;
  }

  const auto& graph = graphOpt.value().get();

  for (const auto& node : graph.Nodes) {
    result.push_back({ node.Id, node.Label });
  }

  return result;
}


bool MotionControlLayer::WaitForPositionReached(const std::string& deviceName, const std::string& positionName,
  const PositionStruct& targetPosition, double timeoutSeconds) {
  // Get settings for tolerance from configuration
  double tolerance = 0.01; // Default tolerance
  const auto& settings = m_configManager.GetSettings();
  if (settings.PositionTolerance > 0) {
    tolerance = settings.PositionTolerance;
  }

  m_logger->LogInfo("MotionControlLayer: Waiting for position with tolerance " + std::to_string(tolerance));

  // Use system clock for timeout
  auto startTime = std::chrono::steady_clock::now();
  int pollCount = 0;

  while (true) {
    pollCount++;

    // Check for cancellation
    if (m_cancelRequested) {
      m_logger->LogWarning("MotionControlLayer: Position waiting cancelled");
      return false;
    }

    // Check if position is reached
    if (IsPositionReached(deviceName, targetPosition, tolerance)) {
      if (pollCount % 10 == 0 || pollCount < 5) { // Log only occasionally to reduce spam
        m_logger->LogInfo("MotionControlLayer: Position reached after " + std::to_string(pollCount) + " polls");
      }
      return true;
    }

    // Log position status every few polls
    if (pollCount % 10 == 0) {
      PositionStruct currentPos;
      if (GetCurrentPosition(deviceName, currentPos)) {
        std::stringstream ss;
        ss << "MotionControlLayer: Current position for " << deviceName
          << " - X:" << currentPos.x << ", Y:" << currentPos.y << ", Z:" << currentPos.z;
        if (currentPos.u != 0 || currentPos.v != 0 || currentPos.w != 0) {
          ss << ", U:" << currentPos.u << ", V:" << currentPos.v << ", W:" << currentPos.w;
        }
        m_logger->LogInfo(ss.str());
      }
    }

    // Check for timeout
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

    if (elapsedSeconds > timeoutSeconds) {
      m_logger->LogError("MotionControlLayer: Timeout waiting for position to be reached after " +
        std::to_string(elapsedSeconds) + " seconds");
      return false;
    }

    // Sleep to avoid CPU spikes (100ms polling interval)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool MotionControlLayer::IsPositionReached(const std::string& deviceName, const PositionStruct& targetPosition,
  double tolerance) {
  PositionStruct currentPosition;
  if (!GetCurrentPosition(deviceName, currentPosition)) {
    return false; // Can't determine position
  }

  // Check if all axes are within tolerance
  bool xOk = std::abs(currentPosition.x - targetPosition.x) <= tolerance;
  bool yOk = std::abs(currentPosition.y - targetPosition.y) <= tolerance;
  bool zOk = std::abs(currentPosition.z - targetPosition.z) <= tolerance;

  // For rotation axes, only check if they are used (non-zero target)
  bool uOk = (targetPosition.u == 0) || (std::abs(currentPosition.u - targetPosition.u) <= tolerance);
  bool vOk = (targetPosition.v == 0) || (std::abs(currentPosition.v - targetPosition.v) <= tolerance);
  bool wOk = (targetPosition.w == 0) || (std::abs(currentPosition.w - targetPosition.w) <= tolerance);

  return xOk && yOk && zOk && uOk && vOk && wOk;
}

bool MotionControlLayer::GetCurrentPosition(const std::string& deviceName, PositionStruct& currentPosition) {
  // Initialize position with zeros
  currentPosition = PositionStruct();

  // Determine which controller manager to use and get the current position
  if (IsDevicePIController(deviceName)) {
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      return false;
    }

    // For PI controllers, get position for each axis
    std::map<std::string, double> positions;
    if (controller->GetPositions(positions)) {
      currentPosition.x = positions["X"];
      currentPosition.y = positions["Y"];
      currentPosition.z = positions["Z"];
      currentPosition.u = positions["U"];
      currentPosition.v = positions["V"];
      currentPosition.w = positions["W"];
      return true;
    }
  }
  else {
    ACSController* controller = m_acsControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      return false;
    }

    // For ACS controllers, get position for each axis
    std::map<std::string, double> positions;
    if (controller->GetPositions(positions)) {
      // ACS might have different axes available
      if (positions.find("X") != positions.end()) currentPosition.x = positions["X"];
      if (positions.find("Y") != positions.end()) currentPosition.y = positions["Y"];
      if (positions.find("Z") != positions.end()) currentPosition.z = positions["Z"];
      return true;
    }
  }

  return false;
}

bool MotionControlLayer::GetDeviceCurrentNode(const std::string& graphName, const std::string& deviceName, std::string& nodeId) const {
  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    return false;
  }

  const auto& graph = graphOpt.value().get();

  // Update position if needed
  const_cast<MotionControlLayer*>(this)->UpdateDevicePosition(deviceName);

  // Get current position
  auto it = m_deviceCurrentPositions.find(deviceName);
  if (it == m_deviceCurrentPositions.end()) {
    return false;
  }

  const auto& currentPos = it->second;

  // Check all nodes for this device
  for (const auto& node : graph.Nodes) {
    if (node.Device == deviceName && !node.Position.empty()) {
      // Get the position for this node
      auto posOpt = m_configManager.GetNamedPosition(deviceName, node.Position);
      if (posOpt.has_value()) {
        const auto& nodePos = posOpt.value().get();
        // Settings from configuration
        double tolerance = 0.1;  // Default tolerance 100um
        const auto& settings = m_configManager.GetSettings();
        if (settings.PositionTolerance > 0) {
          tolerance = settings.PositionTolerance;
        }

        // Compare positions
        if (IsPositionsEqual(currentPos, nodePos, tolerance)) {
          nodeId = node.Id;
          return true;
        }
      }
    }
  }

  return false;
}

bool MotionControlLayer::IsDeviceAtNode(const std::string& graphName, const std::string& nodeId, double tolerance) const {
  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    return false;
  }

  const auto& graph = graphOpt.value().get();

  // Find the node
  const Node* targetNode = nullptr;
  for (const auto& node : graph.Nodes) {
    if (node.Id == nodeId) {
      targetNode = &node;
      break;
    }
  }

  if (!targetNode || targetNode->Device.empty() || targetNode->Position.empty()) {
    return false;
  }

  // Get the target position from configuration
  auto posOpt = m_configManager.GetNamedPosition(targetNode->Device, targetNode->Position);
  if (!posOpt.has_value()) {
    return false;
  }

  const auto& targetPosition = posOpt.value().get();

  // Get the current position
  auto it = m_deviceCurrentPositions.find(targetNode->Device);
  if (it == m_deviceCurrentPositions.end()) {
    // Position not cached, get it directly
    PositionStruct currentPos;
    if (!const_cast<MotionControlLayer*>(this)->GetCurrentPosition(targetNode->Device, currentPos)) {
      return false;
    }
    // Perform comparison
    return IsPositionsEqual(currentPos, targetPosition, tolerance);
  }

  // Use cached position for comparison
  const auto& currentPos = it->second;
  return IsPositionsEqual(currentPos, targetPosition, tolerance);
}

std::string MotionControlLayer::GetNodeIdFromCurrentPosition(const std::string& graphName, const std::string& deviceName) const {
  std::string nodeId;
  if (GetDeviceCurrentNode(graphName, deviceName, nodeId)) {
    return nodeId;
  }
  return "";
}

void MotionControlLayer::UpdateDevicePosition(const std::string& deviceName) {
  PositionStruct currentPos;
  if (GetCurrentPosition(deviceName, currentPos)) {
    m_deviceCurrentPositions[deviceName] = currentPos;
  }
}

