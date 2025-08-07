// UINodeListWindow_Proper.cpp - Following UIConfigVisualizer movement pattern exactly
#include "UINodeListWindow.h"
#include "include/motions/MotionConfigManager.h"
#include "include/machine_operations.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <set>      // for std::set in UpdateCategories
#include <iomanip>  // for std::setprecision in FormatPosition



UINodeListWindow::UINodeListWindow() {
  std::cout << "[UINodeListWindow] Node List Window initialized" << std::endl;
}

UINodeListWindow::~UINodeListWindow() {
  std::cout << "[UINodeListWindow] Node List Window destroyed" << std::endl;
}

// ============================================================================
// COMPONENT SETUP METHODS (Same as before)
// ============================================================================

void UINodeListWindow::SetMotionConfigManager(MotionConfigManager* configManager) {
  m_configManager = configManager;
  if (m_configManager) {
    std::cout << "[UINodeListWindow] Motion Config Manager connected" << std::endl;
    RefreshNodeList();
  }
}

void UINodeListWindow::SetMachineOperations(MachineOperations* machineOps) {
  m_machineOperations = machineOps;
  if (m_machineOperations) {
    std::cout << "[UINodeListWindow] Machine Operations connected" << std::endl;
    RefreshNodeList();
  }
}

void UINodeListWindow::SetPIControllerManager(PIControllerManager* piManager) {
  m_piManager = piManager;
  if (m_piManager) {
    std::cout << "[UINodeListWindow] PI Controller Manager connected" << std::endl;
    UpdateNodeReachability();
  }
}

void UINodeListWindow::SetACSControllerManager(ACSControllerManager* acsManager) {
  m_acsManager = acsManager;
  if (m_acsManager) {
    std::cout << "[UINodeListWindow] ACS Controller Manager connected" << std::endl;
    UpdateNodeReachability();
  }
}

// ============================================================================
// CORE MOVEMENT IMPLEMENTATION - Following UIConfigVisualizer Pattern EXACTLY
// ============================================================================

void UINodeListWindow::NavigateToNode(const std::string& nodeId) {
  if (!m_configManager || !m_machineOperations) {
    std::cout << "[UINodeListWindow] ConfigManager or MachineOperations not available" << std::endl;
    return;
  }

  auto it = m_nodeMap.find(nodeId);
  if (it == m_nodeMap.end()) {
    std::cout << "[UINodeListWindow] Node not found: " << nodeId << std::endl;
    return;
  }

  const NodeInfo& nodeInfo = it->second;

  if (!CanNavigateToNode(nodeId)) {
    std::cout << "[UINodeListWindow] Cannot navigate to node: " << nodeId << std::endl;
    return;
  }

  // Get the actual Node from the config to access Device and Position
  auto graphOpt = m_configManager->GetGraph(nodeInfo.graphName);
  if (!graphOpt.has_value()) {
    std::cout << "[UINodeListWindow] Graph not found: " << nodeInfo.graphName << std::endl;
    return;
  }

  const auto& graph = graphOpt.value().get();

  // Find the target node in the graph
  const Node* targetNode = nullptr;
  for (const auto& node : graph.Nodes) {
    if (node.Id == nodeId) {
      targetNode = &node;
      break;
    }
  }

  if (!targetNode) {
    std::cout << "[UINodeListWindow] Target node not found in graph: " << nodeId << std::endl;
    return;
  }

  // Execute movement using the EXACT same pattern as UIConfigVisualizer
  ExecuteMovementToNode(*targetNode, nodeInfo.graphName);
}

void UINodeListWindow::ExecuteMovementToNode(const Node& targetNode, const std::string& graphName) {
  if (!m_machineOperations) {
    std::cout << "[UINodeListWindow] MachineOperations not available" << std::endl;
    return;
  }

  // Check if node has device assigned
  if (targetNode.Device.empty()) {
    std::cout << "[UINodeListWindow] Cannot move: Node " << targetNode.Id << " has no device assigned" << std::endl;
    return;
  }

  std::cout << "[UINodeListWindow] Starting navigation to node: " << targetNode.Id << std::endl;

  // Set movement state
  m_isMoving = true;
  m_movingToNode = targetNode.Id;
  m_moveProgress = 0.0f;
  m_moveStartTime = std::chrono::steady_clock::now();

  bool moveResult = false;

  try {
    // METHOD 1: Try MoveDeviceToNode first (preferred - follows graph navigation)
    if (!graphName.empty()) {
      std::cout << "[UINodeListWindow] >>> Executing MoveDeviceToNode for node: " << targetNode.Id
        << " (Device: " << targetNode.Device << ", Graph: " << graphName << ")" << std::endl;

      // Call the exact same method as UIConfigVisualizer
      moveResult = m_machineOperations->MoveDeviceToNode(
        targetNode.Device,    // Device name
        graphName,           // Graph name
        targetNode.Id,       // Node ID
        false,               // waitForCompletion
        "UINodeListWindow"   // caller
      );

      if (moveResult) {
        std::cout << "[UINodeListWindow] MoveDeviceToNode command sent successfully" << std::endl;
      }
      else {
        std::cout << "[UINodeListWindow] MoveDeviceToNode command failed" << std::endl;
      }
    }

    // METHOD 2: Try MoveToPointName as fallback (direct position movement)
    if (!moveResult && !targetNode.Position.empty()) {
      std::cout << "[UINodeListWindow] >>> Executing MoveToPointName for node: " << targetNode.Id
        << " (Device: " << targetNode.Device << ", Position: " << targetNode.Position << ")" << std::endl;

      // Call the exact same method as UIConfigVisualizer  
      moveResult = m_machineOperations->MoveToPointName(
        targetNode.Device,    // Device name
        targetNode.Position,  // Position name
        false,               // waitForCompletion
        "UINodeListWindow"   // caller
      );

      if (moveResult) {
        std::cout << "[UINodeListWindow] MoveToPointName command sent successfully" << std::endl;
      }
      else {
        std::cout << "[UINodeListWindow] MoveToPointName command failed" << std::endl;
      }
    }

    // If both methods failed
    if (!moveResult) {
      if (targetNode.Position.empty()) {
        std::cout << "[UINodeListWindow] Cannot move: Node " << targetNode.Id
          << " has no position assigned and graph movement failed" << std::endl;
      }
      else {
        std::cout << "[UINodeListWindow] Both movement methods failed for node: " << targetNode.Id << std::endl;
      }
      ResetMovementState();
    }

  }
  catch (const std::exception& e) {
    std::cout << "[UINodeListWindow] Exception during movement: " << e.what() << std::endl;
    ResetMovementState();
  }
}

void UINodeListWindow::StopMovement() {
  if (!m_isMoving) {
    return;
  }

  std::cout << "[UINodeListWindow] Stopping movement to: " << m_movingToNode << std::endl;

  // Use MachineOperations to stop movement
  if (m_machineOperations) {
    try {
      // Try to stop movement - this may depend on your MachineOperations API
      // You might need to adjust this method name based on actual API
      bool stopResult = m_machineOperations->StopAllMovement();
      if (stopResult) {
        std::cout << "[UINodeListWindow] Stop command sent successfully" << std::endl;
      }
      else {
        std::cout << "[UINodeListWindow] Stop command failed" << std::endl;
      }
    }
    catch (const std::exception& e) {
      std::cout << "[UINodeListWindow] Exception during stop: " << e.what() << std::endl;
    }
  }

  ResetMovementState();
}

void UINodeListWindow::ResetMovementState() {
  m_isMoving = false;
  m_movingToNode = "";
  m_moveProgress = 0.0f;
  m_moveStartTime = std::chrono::steady_clock::time_point{};
}

bool UINodeListWindow::CanNavigateToNode(const std::string& nodeId) {
  auto it = m_nodeMap.find(nodeId);
  if (it == m_nodeMap.end()) {
    return false;
  }

  const NodeInfo& node = it->second;

  // Cannot navigate if already moving
  if (m_isMoving) {
    return false;
  }

  // Cannot navigate if already at position
  if (node.isCurrentPosition) {
    return false;
  }

  // Cannot navigate if not reachable
  if (!node.isReachable) {
    return false;
  }

  // Check if machine operations is available for movement
  if (!m_machineOperations) {
    return false;
  }

  // Check if we have config manager
  if (!m_configManager) {
    return false;
  }

  // Additional validation: Node must have a device assigned
  if (node.deviceName.empty()) {
    std::cout << "[UINodeListWindow] Node has no device assigned: " << nodeId << std::endl;
    return false;
  }

  return true;
}

// ============================================================================
// NODE DATA MANAGEMENT - Updated to work with actual Node structure
// ============================================================================

void UINodeListWindow::RefreshNodeList() {
  m_nodes.clear();
  m_nodeMap.clear();

  // Load from config manager using actual Node structure
  LoadNodesFromConfig();

  // Update derived data
  UpdateCategories();
  UpdateNodeReachability();
  UpdateCurrentPosition();

  std::cout << "[UINodeListWindow] Refreshed node list: " << m_nodes.size() << " nodes" << std::endl;
}

void UINodeListWindow::LoadNodesFromConfig() {
  if (!m_configManager) {
    return;
  }

  try {
    // Get all graph names
    auto graphNames = m_configManager->GetAllGraphNames();

    for (const auto& graphName : graphNames) {
      auto graphOpt = m_configManager->GetGraph(graphName);
      if (!graphOpt.has_value()) {
        continue;
      }

      const auto& graph = graphOpt.value().get();

      // Process each node in the graph
      for (const auto& node : graph.Nodes) {
        NodeInfo nodeInfo;
        nodeInfo.id = node.Id;
        nodeInfo.name = !node.Label.empty() ? node.Label : node.Id;
        nodeInfo.description = "Node from graph: " + graphName;
        nodeInfo.category = graphName;
        nodeInfo.graphName = graphName;
        nodeInfo.deviceName = node.Device;
        nodeInfo.positionName = node.Position;

        // Get position coordinates if available
        if (!node.Device.empty() && !node.Position.empty()) {
          auto positionOpt = m_configManager->GetNamedPosition(node.Device, node.Position);
          if (positionOpt.has_value()) {
            const auto& position = positionOpt.value().get();

            // Store position data
            nodeInfo.positions["x"] = position.x;
            nodeInfo.positions["y"] = position.y;
            nodeInfo.positions["z"] = position.z;

            // Add U, V, W for hex devices
            if (node.Device.find("hex") != std::string::npos) {
              nodeInfo.positions["u"] = position.u;
              nodeInfo.positions["v"] = position.v;
              nodeInfo.positions["w"] = position.w;
            }

            // Estimate time to reach this node
            nodeInfo.estimatedTime = EstimateTimeToNode(nodeInfo);
          }
        }

        m_nodes.push_back(nodeInfo);
        m_nodeMap[nodeInfo.id] = nodeInfo;
      }
    }

    std::cout << "[UINodeListWindow] Loaded " << m_nodes.size() << " nodes from "
      << graphNames.size() << " graphs" << std::endl;

  }
  catch (const std::exception& e) {
    std::cout << "[UINodeListWindow] Error loading nodes from config: " << e.what() << std::endl;
  }
}

double UINodeListWindow::EstimateTimeToNode(const NodeInfo& nodeInfo) {
  // Simple time estimation based on device type and distance
  if (nodeInfo.deviceName.find("hex") != std::string::npos) {
    return 15.0; // Hex stages typically take longer
  }
  else if (nodeInfo.deviceName.find("gantry") != std::string::npos) {
    return 8.0;  // Gantry moves are typically faster
  }
  else {
    return 10.0; // Default estimate
  }
}

void UINodeListWindow::UpdateNodeReachability() {
  for (auto& node : m_nodes) {
    // Node is reachable if it has a device assigned and controllers are available
    node.isReachable = !node.deviceName.empty();

    // Additional check for controller availability
    if (node.isReachable) {
      bool hasControllers = (m_piManager && m_piManager->GetControllerCount() > 0) ||
        (m_acsManager && m_acsManager->GetControllerCount() > 0);
      if (!hasControllers) {
        node.isReachable = false;
      }
    }

    // Update in map as well
    m_nodeMap[node.id] = node;
  }
}

void UINodeListWindow::UpdateCurrentPosition() {
  // Reset all current position flags
  for (auto& node : m_nodes) {
    node.isCurrentPosition = false;
    m_nodeMap[node.id] = node;
  }

  // Update movement progress if we're currently moving
  UpdateMovementProgress();
}

void UINodeListWindow::UpdateMovementProgress() {
  if (!m_isMoving || !m_machineOperations) {
    return;
  }

  try {
    // Simple time-based progress estimation
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_moveStartTime);

    // Get estimated time for this node
    auto it = m_nodeMap.find(m_movingToNode);
    double estimatedTime = (it != m_nodeMap.end()) ? it->second.estimatedTime : 10.0;

    float calculatedProgress = static_cast<float>(elapsed.count()) / (estimatedTime * 1000.0f);
    m_moveProgress = (std::min)(1.0f, calculatedProgress);

    // Check if movement should be complete (this is simplified - in reality you'd check device status)
    if (m_moveProgress >= 1.0f) {
      std::cout << "[UINodeListWindow] Movement to " << m_movingToNode << " completed (estimated)" << std::endl;
      ResetMovementState();
      UpdateCurrentPosition();
    }

  }
  catch (const std::exception& e) {
    std::cout << "[UINodeListWindow] Error updating movement progress: " << e.what() << std::endl;
    ResetMovementState();
  }
}

// ============================================================================
// UI RENDERING METHODS (Keep existing implementation)
// ============================================================================

void UINodeListWindow::RenderUI() {
  if (!m_showWindow) return;

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Node Navigation", &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Update current position periodically
  static float lastUpdate = 0.0f;
  float currentTime = ImGui::GetTime();
  if (currentTime - lastUpdate > 1.0f) { // Update every second
    UpdateCurrentPosition();
    lastUpdate = currentTime;
  }

  RenderMainPanel();

  ImGui::End();
}

// Add these methods to UINodeListWindow.cpp

void UINodeListWindow::RenderMainPanel() {
  // Check if we have any data sources
  if (!m_configManager && !m_machineOperations) {
    ImGui::Text("No data sources connected");
    ImGui::Text("Connect MotionConfigManager or MachineOperations to see nodes");
    return;
  }

  // Split into main list and details panel
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float listWidth = m_showDetailsPanel ? contentSize.x * 0.6f : contentSize.x;
  float detailsWidth = contentSize.x * 0.4f;

  // Left panel - Node list
  ImGui::BeginChild("NodeListPanel", ImVec2(listWidth, contentSize.y), true);

  RenderFilterControls();
  ImGui::Separator();
  RenderNodeList();

  ImGui::EndChild();

  if (m_showDetailsPanel) {
    ImGui::SameLine();

    // Right panel - Details
    ImGui::BeginChild("DetailsPanel", ImVec2(detailsWidth, contentSize.y), true);
    RenderDetailsPanel();
    ImGui::EndChild();
  }

  // Status bar at bottom
  RenderStatusBar();
}

void UINodeListWindow::RenderFilterControls() {
  ImGui::Text("Node Navigation");

  // Filter text box
  ImGui::PushItemWidth(200);
  ImGui::InputTextWithHint("##filter", "Search nodes...", &m_filterText[0], 256);
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Category filter
  ImGui::PushItemWidth(120);
  if (ImGui::BeginCombo("Category", m_selectedCategory.c_str())) {
    if (ImGui::Selectable("All", m_selectedCategory == "All")) {
      m_selectedCategory = "All";
    }
    for (const auto& category : m_categories) {
      bool isSelected = (m_selectedCategory == category);
      if (ImGui::Selectable(category.c_str(), isSelected)) {
        m_selectedCategory = category;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Options
  if (ImGui::Button("Refresh")) {
    RefreshNodeList();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Details", &m_showDetailsPanel);

  ImGui::SameLine();
  ImGui::Checkbox("Reachable Only", &m_showOnlyReachable);

  // Quick stats
  auto filteredNodes = GetFilteredNodes();
  ImGui::Text("Showing %zu of %zu nodes", filteredNodes.size(), m_nodes.size());
}

void UINodeListWindow::RenderNodeList() {
  auto filteredNodes = GetFilteredNodes();

  if (filteredNodes.empty()) {
    ImGui::Text("No nodes found");
    if (!m_filterText.empty() || m_selectedCategory != "All") {
      ImGui::Text("Try adjusting your filters");
    }
    return;
  }

  // Table for better organization
  if (ImGui::BeginTable("NodesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY)) {

    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableHeadersRow();

    for (const auto& node : filteredNodes) {
      RenderNodeItem(node);
    }

    ImGui::EndTable();
  }
}

void UINodeListWindow::RenderNodeItem(const NodeInfo& node) {
  ImGui::TableNextRow();

  // Status column
  ImGui::TableNextColumn();
  ImVec4 statusColor = GetNodeStatusColor(node);
  const char* statusIcon = GetNodeStatusIcon(node);
  ImGui::TextColored(statusColor, "%s", statusIcon);

  if (ImGui::IsItemHovered()) {
    std::string tooltip = "Node: " + node.name;
    if (node.isCurrentPosition) {
      tooltip += "\nStatus: Current Position";
    }
    else if (!node.isReachable) {
      tooltip += "\nStatus: Not Reachable";
    }
    else {
      tooltip += "\nStatus: Available";
      tooltip += "\nEst. Time: " + FormatTime(node.estimatedTime);
    }
    ImGui::SetTooltip("%s", tooltip.c_str());
  }

  // Node column
  ImGui::TableNextColumn();

  // Make node selectable
  bool isSelected = (m_selectedNodeId == node.id);
  if (ImGui::Selectable(("##" + node.id).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
    m_selectedNodeId = node.id;
  }

  // Node name (overlay on selectable)
  ImGui::SameLine();
  ImGui::Text("%s", node.name.c_str());

  if (!node.description.empty() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", node.description.c_str());
  }

  // Category column
  ImGui::TableNextColumn();
  ImGui::Text("%s", node.category.c_str());

  // Actions column
  ImGui::TableNextColumn();

  bool canNavigate = CanNavigateToNode(node.id);
  if (!canNavigate) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button(("Go##" + node.id).c_str(), ImVec2(35, 20))) {
    if (canNavigate) {
      NavigateToNode(node.id);
    }
  }

  if (!canNavigate) {
    ImGui::PopStyleVar();
  }

  ImGui::SameLine();
  if (ImGui::Button(("?##" + node.id).c_str(), ImVec2(20, 20))) {
    m_selectedNodeId = node.id;
    m_showDetailsPanel = true;
  }
}

void UINodeListWindow::RenderDetailsPanel() {
  ImGui::Text("Node Details");
  ImGui::Separator();

  if (m_selectedNodeId.empty()) {
    ImGui::Text("Select a node to view details");
    return;
  }

  auto it = m_nodeMap.find(m_selectedNodeId);
  if (it == m_nodeMap.end()) {
    ImGui::Text("Selected node not found");
    return;
  }

  const NodeInfo& node = it->second;

  // Node information
  ImGui::Text("Name: %s", node.name.c_str());
  ImGui::Text("ID: %s", node.id.c_str());
  ImGui::Text("Category: %s", node.category.c_str());
  ImGui::Text("Device: %s", node.deviceName.c_str());
  ImGui::Text("Position: %s", node.positionName.c_str());

  if (!node.description.empty()) {
    ImGui::Spacing();
    ImGui::Text("Description:");
    ImGui::TextWrapped("%s", node.description.c_str());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Position information
  ImGui::Text("Positions:");
  if (node.positions.empty()) {
    ImGui::Text("  No position data available");
  }
  else {
    for (const auto& [axis, position] : node.positions) {
      ImGui::Text("  %s: %.3f", axis.c_str(), position);
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Navigation controls
  RenderNavigationControls();
}

void UINodeListWindow::RenderNavigationControls() {
  if (m_selectedNodeId.empty()) {
    return;
  }

  auto it = m_nodeMap.find(m_selectedNodeId);
  if (it == m_nodeMap.end()) {
    return;
  }

  const NodeInfo& node = it->second;

  ImGui::Text("Navigation:");

  if (node.isCurrentPosition) {
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Already at this position");
    return;
  }

  bool canNavigate = CanNavigateToNode(node.id);

  if (!canNavigate) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Cannot navigate to this node");
    if (!node.isReachable) {
      ImGui::Text("Reason: Node not reachable");
    }
    else if (m_isMoving) {
      ImGui::Text("Reason: Movement in progress");
    }
  }

  // Navigation button
  if (ImGui::Button("Navigate to Node", ImVec2(-1, 40))) {
    if (canNavigate) {
      NavigateToNode(node.id);
    }
  }

  if (!canNavigate) {
    ImGui::PopStyleVar();
  }

  // Stop button if moving
  if (m_isMoving) {
    ImGui::Spacing();
    if (ImGui::Button("Stop Movement", ImVec2(-1, 30))) {
      StopMovement();
    }

    // Progress bar
    ImGui::Spacing();
    ImGui::Text("Moving to: %s", m_movingToNode.c_str());
    ImGui::ProgressBar(m_moveProgress, ImVec2(-1, 0));
  }

  // Settings
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::Text("Settings:");
  ImGui::Checkbox("Confirm before move", &m_confirmBeforeMove);
}

void UINodeListWindow::RenderStatusBar() {
  ImGui::Separator();

  // Status information
  if (m_isMoving) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Moving to: %s (%.0f%%)",
      m_movingToNode.c_str(), m_moveProgress * 100.0f);
  }
  else {
    ImGui::Text("Ready");
  }

  ImGui::SameLine();
  ImGui::Text(" | ");
  ImGui::SameLine();

  // Connection status
  int connectedSystems = 0;
  if (m_configManager) connectedSystems++;
  if (m_machineOperations) connectedSystems++;
  if (m_piManager) connectedSystems++;
  if (m_acsManager) connectedSystems++;

  ImGui::Text("Connected: %d/4 systems", connectedSystems);
}

void UINodeListWindow::UpdateCategories() {
  m_categories.clear();
  std::set<std::string> uniqueCategories;

  for (const auto& node : m_nodes) {
    uniqueCategories.insert(node.category);
  }

  for (const auto& category : uniqueCategories) {
    m_categories.push_back(category);
  }
}

std::vector<UINodeListWindow::NodeInfo> UINodeListWindow::GetFilteredNodes() {
  std::vector<NodeInfo> filtered;

  for (const auto& node : m_nodes) {
    if (NodeMatchesFilter(node)) {
      filtered.push_back(node);
    }
  }

  return filtered;
}

bool UINodeListWindow::NodeMatchesFilter(const NodeInfo& node) {
  // Category filter
  if (m_selectedCategory != "All" && node.category != m_selectedCategory) {
    return false;
  }

  // Reachability filter
  if (m_showOnlyReachable && !node.isReachable) {
    return false;
  }

  // Text filter
  if (!m_filterText.empty()) {
    std::string filterLower = m_filterText;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    std::string nameLower = node.name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    if (nameLower.find(filterLower) == std::string::npos) {
      return false;
    }
  }

  return true;
}

std::string UINodeListWindow::FormatPosition(const std::map<std::string, double>& positions) {
  if (positions.empty()) {
    return "No position data";
  }

  std::stringstream ss;
  bool first = true;
  for (const auto& [axis, position] : positions) {
    if (!first) ss << ", ";
    ss << axis << ":" << std::fixed << std::setprecision(2) << position;
    first = false;
  }

  return ss.str();
}

std::string UINodeListWindow::FormatTime(double seconds) {
  if (seconds < 60) {
    return std::to_string((int)seconds) + "s";
  }
  else {
    int minutes = (int)(seconds / 60);
    int remainingSeconds = (int)(seconds) % 60;
    return std::to_string(minutes) + "m " + std::to_string(remainingSeconds) + "s";
  }
}

ImVec4 UINodeListWindow::GetNodeStatusColor(const NodeInfo& node) {
  if (node.isCurrentPosition) {
    return ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // Light green
  }
  else if (!node.isReachable) {
    return ImVec4(1.0f, 0.5f, 0.5f, 1.0f); // Light red
  }
  else {
    return ImVec4(0.5f, 0.5f, 1.0f, 1.0f); // Light blue
  }
}

const char* UINodeListWindow::GetNodeStatusIcon(const NodeInfo& node) {
  if (node.isCurrentPosition) {
    return "●"; // Current position
  }
  else if (!node.isReachable) {
    return "✗"; // Not reachable
  }
  else {
    return "○"; // Available
  }
}