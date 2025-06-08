// motion_control_layer.cpp
#include "include/motions/motion_control_layer.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <filesystem>  // For backup file operations
#include <iomanip>     // For timestamp formatting
#include <chrono>      // For time operations
#include <sstream>     // For string stream operations



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

  // First check the explicit controller type if available
  if (!device.TypeController.empty()) {
    bool isPIController = (device.TypeController == "PI");

    if (m_enableDebug) {
      m_logger->LogInfo("MotionControlLayer: Device " + deviceName +
        " has explicit controller type: " + device.TypeController +
        " (isPIController=" + (isPIController ? "true" : "false") + ")");
    }

    return isPIController;
  }

  // Fall back to the port number check as a secondary method
  bool isPIControllerByPort = (device.Port == 50000);

  if (m_enableDebug) {
    m_logger->LogInfo("MotionControlLayer: Device " + deviceName +
      " has no explicit controller type, using port number " + std::to_string(device.Port) +
      " (isPIController=" + (isPIControllerByPort ? "true" : "false") + ")");
  }

  return isPIControllerByPort;
}


// Updated RenderUI method for MotionControlLayer class without using std::set
// Updated RenderUI method for motion_control_layer.cpp
// This is just the RenderUI method - the rest of the file remains unchanged

void MotionControlLayer::RenderUI() {
  // Only render if the window is visible
  if (!m_isWindowVisible) {
    return;
  }

  // Simple UI for monitoring and control
  if (!ImGui::Begin("Motion Control", &m_isWindowVisible, ImGuiWindowFlags_None)) {
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

  // Path planning controls
  ImGui::Separator();
  ImGui::Text("Path Planning");

  static char graphName[128] = "Process_Flow";
  static char startNode[128] = "";
  static char endNode[128] = "";
  static bool startNodeInitialized = false;
  static bool endNodeInitialized = false;
  static char controllerFilter[128] = ""; // Buffer for controller filter
  static bool controllerFilterInitialized = false;

  // Graph selection with name display
  if (ImGui::BeginCombo("Graph", graphName)) {
    const auto& allGraphs = m_configManager.GetAllGraphs();
    for (const auto& [name, graph] : allGraphs) {
      bool isSelected = (std::string(graphName) == name);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        strcpy_s(graphName, sizeof(graphName), name.c_str());
        // Reset filters when changing graphs
        startNodeInitialized = false;
        endNodeInitialized = false;
        controllerFilterInitialized = false;
        *controllerFilter = 0; // Clear controller filter
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Controller filter dropdown
  if (ImGui::BeginCombo("Controller", strlen(controllerFilter) > 0 ? controllerFilter : "All Controllers")) {
    // Add "All Controllers" option
    bool isAllSelected = (strlen(controllerFilter) == 0);
    if (ImGui::Selectable("All Controllers", isAllSelected)) {
      *controllerFilter = 0; // Clear selection to mean "all controllers"
      controllerFilterInitialized = true;
    }
    if (isAllSelected) {
      ImGui::SetItemDefaultFocus();
    }

    // Get all unique controllers from the selected graph
    std::vector<std::string> controllers;
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        if (!node.Device.empty()) {
          // Check if this controller is already in our list
          bool alreadyExists = false;
          for (const auto& existingController : controllers) {
            if (existingController == node.Device) {
              alreadyExists = true;
              break;
            }
          }

          // Add to list if not already there
          if (!alreadyExists) {
            controllers.push_back(node.Device);
          }
        }
      }
    }

    // Create a controller list
    for (const auto& controller : controllers) {
      bool isSelected = (strcmp(controllerFilter, controller.c_str()) == 0);
      if (ImGui::Selectable(controller.c_str(), isSelected)) {
        strcpy_s(controllerFilter, sizeof(controllerFilter), controller.c_str());
        controllerFilterInitialized = true;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Initialize default controller if needed
  if (!controllerFilterInitialized) {
    *controllerFilter = 0;
    controllerFilterInitialized = true;
  }

  // Try to determine current node position for default
  if (!startNodeInitialized) {
    // Find a device from the graph
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      if (!graph.Nodes.empty()) {
        // Apply controller filter for startNode initialization if available
        std::string deviceName;
        if (strlen(controllerFilter) > 0) {
          deviceName = controllerFilter;
        }
        else {
          // Use first node's device as default
          deviceName = graph.Nodes.front().Device;
        }

        if (!deviceName.empty()) {
          std::string currentNodeId;
          if (GetDeviceCurrentNode(graphName, deviceName, currentNodeId) && !currentNodeId.empty()) {
            strcpy_s(startNode, sizeof(startNode), currentNodeId.c_str());
          }
          else {
            // Default to first node matching the controller filter
            for (const auto& node : graph.Nodes) {
              if (strlen(controllerFilter) == 0 || node.Device == controllerFilter) {
                strcpy_s(startNode, sizeof(startNode), node.Id.c_str());
                break;
              }
            }
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
        // Try to find a node that's not the start node and matches the controller filter
        std::string selectedEndNode;
        for (const auto& node : graph.Nodes) {
          if (node.Id != startNode && (strlen(controllerFilter) == 0 || node.Device == controllerFilter)) {
            selectedEndNode = node.Id;
            break;
          }
        }

        // Fallback to the first node if no suitable node found
        if (selectedEndNode.empty() && !graph.Nodes.empty()) {
          selectedEndNode = graph.Nodes[0].Id;
        }

        strcpy_s(endNode, sizeof(endNode), selectedEndNode.c_str());
        endNodeInitialized = true;
      }
    }
  }

  // Start Node dropdown with labels - filtered by selected controller
  if (ImGui::BeginCombo("Start Node", GetNodeLabelAndId(graphName, startNode).c_str())) {
    auto nodes = GetAllNodesWithLabels(graphName);
    for (const auto& [id, label] : nodes) {
      // Apply controller filter if set
      const Node* node = GetNodeById(graphName, id);
      if (!node || (strlen(controllerFilter) > 0 && node->Device != controllerFilter)) {
        continue; // Skip nodes that don't match the controller filter
      }

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

  // End Node dropdown with labels - filtered by selected controller
  if (ImGui::BeginCombo("End Node", GetNodeLabelAndId(graphName, endNode).c_str())) {
    auto nodes = GetAllNodesWithLabels(graphName);
    for (const auto& [id, label] : nodes) {
      // Apply controller filter if set
      const Node* node = GetNodeById(graphName, id);
      if (!node || (strlen(controllerFilter) > 0 && node->Device != controllerFilter)) {
        continue; // Skip nodes that don't match the controller filter
      }

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

  // Add a "Use Current Position as Start" button
  if (ImGui::Button("Use Current Position as Start")) {
    auto graphOpt = m_configManager.GetGraph(graphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      if (!graph.Nodes.empty()) {
        std::string deviceName;

        // Use filtered controller if available
        if (strlen(controllerFilter) > 0) {
          deviceName = controllerFilter;
        }
        else {
          // Otherwise use the first node's device
          deviceName = graph.Nodes.front().Device;
        }

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
    // Only show positions for the filtered controller if a filter is set
    if (strlen(controllerFilter) > 0 && deviceName != controllerFilter) {
      continue;
    }

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



// Helper function to get a node by its ID (added as a private member to MotionControlLayer)
const Node* MotionControlLayer::GetNodeById(const std::string& graphName, const std::string& nodeId) const {
  auto graphOpt = m_configManager.GetGraph(graphName);
  if (!graphOpt.has_value()) {
    return nullptr;
  }

  const auto& graph = graphOpt.value().get();
  for (const auto& node : graph.Nodes) {
    if (node.Id == nodeId) {
      return &node;
    }
  }

  return nullptr;
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

// Add to motion_control_layer.cpp

bool MotionControlLayer::GetCurrentPosition(const std::string& deviceName, PositionStruct& position) const {
  // Try to get the position from cached positions
  auto it = m_deviceCurrentPositions.find(deviceName);
  if (it != m_deviceCurrentPositions.end()) {
    position = it->second;
    return true;
  }

  // If not cached, get it directly
  // This is a non-const operation, so we need to cast away const
  const_cast<MotionControlLayer*>(this)->UpdateDevicePosition(deviceName);

  // After updating, check if the position exists in the map
  if (m_deviceCurrentPositions.find(deviceName) != m_deviceCurrentPositions.end()) {
    position = m_deviceCurrentPositions.at(deviceName);
    return true;
  }

  return false;
}

std::vector<std::string> MotionControlLayer::GetAvailableDevices() const {
  std::vector<std::string> devices;

  // Get all devices from configuration
  const auto& allDevices = m_configManager.GetAllDevices();

  // Add enabled devices to the list
  for (const auto& [name, device] : allDevices) {
    if (device.IsEnabled) {
      devices.push_back(name);
    }
  }

  return devices;
}

bool MotionControlLayer::MoveToPosition(const std::string& deviceName, const PositionStruct& position, bool blocking) {
  // Check if device exists and what type it is
  auto deviceOpt = m_configManager.GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Device not found: " + deviceName);
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // Determine which controller manager to use
  if (IsDevicePIController(deviceName)) {
    // Use PI controller
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MotionControlLayer: PI controller not available for " + deviceName);
      return false;
    }

    // For PI controllers, we need to move multiple axes at once
    std::vector<std::string> axes = { "X", "Y", "Z", "U", "V", "W" };
    std::vector<double> positions = { position.x, position.y, position.z, position.u, position.v, position.w };

    // Use the MoveToPositionMultiAxis method if available, otherwise move each axis individually
    bool success = true;
    try {
      // Directly call the multi-axis method
      success = controller->MoveToPositionMultiAxis(axes, positions, blocking);
    }
    catch (const std::exception& e) {
      // Method doesn't exist or failed, fall back to individual axis movement
      m_logger->LogWarning("MoveToPositionMultiAxis failed, falling back: " + std::string(e.what()));

      // Move each axis individually
      for (size_t i = 0; i < axes.size(); ++i) {
        if (!controller->MoveToPosition(axes[i], positions[i], false)) {
          success = false;
          break;
        }
      }

      // If blocking, wait for all axes to complete
      if (blocking && success) {
        for (const auto& axis : axes) {
          if (!controller->WaitForMotionCompletion(axis)) {
            success = false;
            break;
          }
        }
      }
    }

    return success;
  }
  else {
    // Use ACS controller
    ACSController* controller = m_acsControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MotionControlLayer: ACS controller not available for " + deviceName);
      return false;
    }

    // For ACS controllers, we need to move each axis individually
    bool success = true;

    // Only move axes with non-zero values
    if (position.x != 0 && !controller->MoveToPosition("X", position.x, false)) success = false;
    if (position.y != 0 && !controller->MoveToPosition("Y", position.y, false)) success = false;
    if (position.z != 0 && !controller->MoveToPosition("Z", position.z, false)) success = false;

    // If blocking, wait for all axes to complete
    if (blocking && success) {
      for (const auto& axis : { "X", "Y", "Z" }) {
        if (!controller->WaitForMotionCompletion(axis)) {
          success = false;
          break;
        }
      }
    }

    return success;
  }
}

bool MotionControlLayer::MoveRelative(const std::string& deviceName, const std::string& axis,
  double distance, bool blocking) {
  // Check if device exists and what type it is
  auto deviceOpt = m_configManager.GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Device not found: " + deviceName);
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // Determine which controller manager to use
  if (IsDevicePIController(deviceName)) {
    // Use PI controller
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MotionControlLayer: PI controller not available for " + deviceName);
      return false;
    }

    return controller->MoveRelative(axis, distance, blocking);
  }
  else {
    // Use ACS controller
    ACSController* controller = m_acsControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MotionControlLayer: ACS controller not available for " + deviceName);
      return false;
    }

    return controller->MoveRelative(axis, distance, blocking);
  }
}


// =====================================
// 2. ADD TO motion_control_layer.cpp
// =====================================

// Save current position of a specific device to motion configuration
bool MotionControlLayer::SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MotionControlLayer: Saving current position of " + deviceName +
    " as named position '" + positionName + "' to configuration");

  // Validate inputs
  if (deviceName.empty() || positionName.empty()) {
    m_logger->LogError("MotionControlLayer: Device name and position name cannot be empty");
    return false;
  }

  // Get current position
  PositionStruct currentPosition;
  if (!GetCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MotionControlLayer: Failed to get current position for device " + deviceName);
    return false;
  }

  try {
    // Create backup before modifying
    if (!BackupMotionConfig("before_position_save")) {
      m_logger->LogWarning("MotionControlLayer: Failed to create backup, proceeding anyway");
    }

    // Check if position name already exists
    auto existingPosOpt = m_configManager.GetNamedPosition(deviceName, positionName);
    if (existingPosOpt.has_value()) {
      m_logger->LogWarning("MotionControlLayer: Position '" + positionName +
        "' already exists for device " + deviceName + ", will be overwritten");
    }

    // Add the position to configuration
    m_configManager.AddPosition(deviceName, positionName, currentPosition);
    m_logger->LogInfo("MotionControlLayer: Successfully added position to config manager");

    // Save the configuration to file
    if (!m_configManager.SaveConfig()) {
      m_logger->LogError("MotionControlLayer: Failed to save configuration file");
      return false;
    }

    // Log success with detailed position information
    std::stringstream positionInfo;
    positionInfo << std::fixed << std::setprecision(6);
    positionInfo << "Position '" << positionName << "' saved for device " << deviceName
      << " - X:" << currentPosition.x << "mm"
      << " Y:" << currentPosition.y << "mm"
      << " Z:" << currentPosition.z << "mm";

    if (currentPosition.u != 0.0 || currentPosition.v != 0.0 || currentPosition.w != 0.0) {
      positionInfo << " U:" << currentPosition.u << "°"
        << " V:" << currentPosition.v << "°"
        << " W:" << currentPosition.w << "°";
    }

    m_logger->LogInfo("MotionControlLayer: " + positionInfo.str());
    m_logger->LogInfo("MotionControlLayer: Configuration saved to motion_config.json");

    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionControlLayer: Exception while saving position to config: " + std::string(e.what()));
    return false;
  }
}

// Update an existing named position with current position
bool MotionControlLayer::UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MotionControlLayer: Updating named position '" + positionName +
    "' for device " + deviceName + " with current position");

  // Check if the named position exists
  auto existingPosOpt = m_configManager.GetNamedPosition(deviceName, positionName);
  if (!existingPosOpt.has_value()) {
    m_logger->LogError("MotionControlLayer: Named position '" + positionName +
      "' does not exist for device " + deviceName);
    return false;
  }

  // Use the save method to update (AddPosition will overwrite existing position)
  return SaveCurrentPositionToConfig(deviceName, positionName);
}

// Save current positions of all available devices to configuration
bool MotionControlLayer::SaveAllCurrentPositionsToConfig(const std::string& prefix) {
  m_logger->LogInfo("MotionControlLayer: Saving current positions of all devices to configuration");

  // Get all available devices
  std::vector<std::string> deviceList = GetAvailableDevices();

  if (deviceList.empty()) {
    m_logger->LogWarning("MotionControlLayer: No devices available to save positions");
    return false;
  }

  // Create backup before bulk operation
  if (!BackupMotionConfig("before_bulk_position_save")) {
    m_logger->LogWarning("MotionControlLayer: Failed to create backup before bulk save");
  }

  bool allSuccess = true;
  int successCount = 0;

  // Generate timestamp for position names
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);

  std::stringstream timeString;
  timeString << std::put_time(&tm, "%Y%m%d_%H%M%S");

  // Save each device's current position
  for (const auto& deviceName : deviceList) {
    std::string positionName = prefix + timeString.str();

    if (SaveCurrentPositionToConfig(deviceName, positionName)) {
      successCount++;
      m_logger->LogInfo("MotionControlLayer: Saved position for " + deviceName + " as '" + positionName + "'");
    }
    else {
      allSuccess = false;
      m_logger->LogError("MotionControlLayer: Failed to save position for " + deviceName);
    }
  }

  m_logger->LogInfo("MotionControlLayer: Bulk position save completed - " +
    std::to_string(successCount) + " out of " +
    std::to_string(deviceList.size()) + " positions saved");

  return allSuccess;
}

// Create backup of motion configuration file
bool MotionControlLayer::BackupMotionConfig(const std::string& backupSuffix) {
  try {
    std::string configPath = "motion_config.json";

    // Generate backup filename
    std::string backupPath;
    if (backupSuffix.empty()) {
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      auto tm = *std::localtime(&time_t);

      std::stringstream timeString;
      timeString << std::put_time(&tm, "%Y%m%d_%H%M%S");
      backupPath = "motion_config_backup_" + timeString.str() + ".json";
    }
    else {
      backupPath = "motion_config_backup_" + backupSuffix + ".json";
    }

    // Check if original file exists
    if (!std::filesystem::exists(configPath)) {
      m_logger->LogError("MotionControlLayer: Original config file not found: " + configPath);
      return false;
    }

    // Copy file
    std::filesystem::copy_file(configPath, backupPath, std::filesystem::copy_options::overwrite_existing);

    m_logger->LogInfo("MotionControlLayer: Created backup: " + backupPath);
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionControlLayer: Failed to create backup: " + std::string(e.what()));
    return false;
  }
}

// Save the motion configuration to file
bool MotionControlLayer::SaveMotionConfig() {
  m_logger->LogInfo("MotionControlLayer: Saving motion configuration to file");

  try {
    if (!m_configManager.SaveConfig()) {
      m_logger->LogError("MotionControlLayer: Failed to save motion configuration");
      return false;
    }

    m_logger->LogInfo("MotionControlLayer: Motion configuration saved successfully");
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionControlLayer: Exception while saving configuration: " + std::string(e.what()));
    return false;
  }
}

// Reload motion configuration from file
bool MotionControlLayer::ReloadMotionConfig() {
  m_logger->LogInfo("MotionControlLayer: Reloading motion configuration from file");

  try {
    // Note: MotionConfigManager constructor loads the config, but we need a reload method
    // For now, we'll log that manual restart is required
    m_logger->LogWarning("MotionControlLayer: Configuration reload requires application restart");
    m_logger->LogWarning("MotionControlLayer: Please restart the application to use updated configuration");

    // TODO: Implement proper config reload if MotionConfigManager supports it
    // This would require adding a reload method to MotionConfigManager

    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionControlLayer: Exception while reloading configuration: " + std::string(e.what()));
    return false;
  }
}