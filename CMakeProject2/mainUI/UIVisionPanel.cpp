// UIVisionPanel.cpp - Debug version with explicit node list integration
#include "UIVisionPanel.h"
#include "include/halcon/VisionCircleDetection.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include "include/motions/MotionConfigManager.h"
#include "include/machine_operations.h"
#include <iostream>
#include <filesystem>

// OpenGL headers for texture management
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif

UIVisionPanel::UIVisionPanel() {
  std::cout << "[UIVisionPanel] Initializing Vision Panel with Circle Detection and Node Navigation" << std::endl;

  // Initialize node list as visible for testing
  m_showNodeList = false; // Start with it off, user can toggle

  // Initialize auto-execution settings
  m_autoExecute = false;
  m_autoExecuteInterval = 200.0f; // Default 200ms
  m_lastAutoExecuteTime = 0.0f;

  InitializeCircleDetection();
}

UIVisionPanel::~UIVisionPanel() {
  CleanupImageTexture();
  std::cout << "[UIVisionPanel] Vision Panel destroyed" << std::endl;
}

void UIVisionPanel::SetMotionConfigManager(MotionConfigManager* configManager) {
  m_configManager = configManager;
  if (m_configManager) {
    std::cout << "[UIVisionPanel] Motion Config Manager connected - loading nodes..." << std::endl;
    LoadNodesFromConfig();
    std::cout << "[UIVisionPanel] Loaded " << m_nodes.size() << " nodes from config" << std::endl;
  }
  else {
    std::cout << "[UIVisionPanel] Motion Config Manager is NULL!" << std::endl;
  }
}

void UIVisionPanel::SetMachineOperations(MachineOperations* machineOps) {
  m_machineOperations = machineOps;
  if (m_machineOperations) {
    std::cout << "[UIVisionPanel] Machine Operations connected" << std::endl;
  }
  else {
    std::cout << "[UIVisionPanel] Machine Operations is NULL!" << std::endl;
  }
}

void UIVisionPanel::RenderUI() {
  if (!m_showWindow) return;

  // Update auto-execution timer
  UpdateAutoExecution();

  ImGui::SetNextWindowSize(ImVec2(1400, 800), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Vision Processing & Navigation", &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Debug info at top
  if (m_showNodeList) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Node List: ENABLED (%zu nodes)", m_nodes.size());
  }
  else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Node List: DISABLED");
  }
  ImGui::SameLine();
  ImGui::Text("| Config: %s | MachineOps: %s",
    m_configManager ? "OK" : "NULL",
    m_machineOperations ? "OK" : "NULL");

  ImGui::Separator();

  // Calculate layout based on whether node list is shown
  ImVec2 contentSize = ImGui::GetContentRegionAvail();

  if (m_showNodeList) {
    //std::cout << "[UIVisionPanel] Rendering with 4-panel layout" << std::endl;

    // Four-panel layout: Controls | Image | Results | Nodes
    float leftWidth = contentSize.x * 0.25f;   // 25% for controls
    float middleWidth = contentSize.x * 0.35f; // 35% for image
    float rightWidth = contentSize.x * 0.20f;  // 20% for results
    float nodeWidth = contentSize.x * 0.20f;   // 20% for nodes

    // Left Panel - Controls
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle Panel - Image Display
    ImGui::BeginChild("ImagePanel", ImVec2(middleWidth, contentSize.y), true);
    RenderImageDisplay();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel - Results
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Node Panel - Navigation
    ImGui::BeginChild("NodePanel", ImVec2(nodeWidth, contentSize.y), true);
    RenderNodeListPanel();
    ImGui::EndChild();
  }
  else {
    // Original three-panel layout
    float leftWidth = contentSize.x * 0.3f;    // 30% for controls
    float middleWidth = contentSize.x * 0.45f; // 45% for image display
    float rightWidth = contentSize.x * 0.25f;  // 25% for results

    // Left Panel - Controls
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle Panel - Image Display
    ImGui::BeginChild("ImagePanel", ImVec2(middleWidth, contentSize.y), true);
    RenderImageDisplay();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel - Results
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();
  }

  ImGui::End();
}

void UIVisionPanel::RenderLeftPanel() {
  ImGui::Text("Vision & Navigation");
  ImGui::Separator();

  // Toggle node list panel with debug info
  bool oldState = m_showNodeList;
  if (ImGui::Checkbox("Show Node List", &m_showNodeList)) {
    std::cout << "[UIVisionPanel] Node list toggled: " << (m_showNodeList ? "ON" : "OFF") << std::endl;

    if (m_showNodeList && m_configManager && oldState != m_showNodeList) {
      std::cout << "[UIVisionPanel] Refreshing nodes from config..." << std::endl;
      LoadNodesFromConfig(); // Refresh when opening
      std::cout << "[UIVisionPanel] Now have " << m_nodes.size() << " nodes" << std::endl;
    }
  }

  // Show status
  if (m_showNodeList) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "(%zu nodes)", m_nodes.size());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Camera Selection
  RenderCameraSelection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Circle Detection Controls
  RenderCircleDetectionControls();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // NEW: Auto-execution controls
  RenderAutoExecutionControls();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Parameter Controls
  if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCircleParameterControls();
  }
}

void UIVisionPanel::RenderNodeListPanel() {
  ImGui::Text("Node Navigation");
  ImGui::Separator();

  // Debug status
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "DEBUG STATUS:");
  ImGui::Text("Config Manager: %s", m_configManager ? "Connected" : "NULL");
  ImGui::Text("Machine Ops: %s", m_machineOperations ? "Connected" : "NULL");
  ImGui::Text("Nodes Loaded: %zu", m_nodes.size());

  if (!m_configManager) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: Config Manager not available");
    ImGui::Text("Node navigation requires MotionConfigManager");
    ImGui::Text("Check MainUIManager setup");
    return;
  }

  if (!m_machineOperations) {
    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WARNING: Machine Operations not available");
    ImGui::Text("Navigation will be disabled");
    ImGui::Separator();
  }

  RenderNodeListControls();
  ImGui::Separator();
  RenderNodeListTable();
}

void UIVisionPanel::RenderNodeListControls() {
  // Refresh button
  if (ImGui::Button("Refresh", ImVec2(60, 25))) {
    std::cout << "[UIVisionPanel] Manual refresh requested" << std::endl;
    LoadNodesFromConfig();
    std::cout << "[UIVisionPanel] After refresh: " << m_nodes.size() << " nodes" << std::endl;
  }

  // Debug button
  ImGui::SameLine();
  if (ImGui::Button("Debug", ImVec2(50, 25))) {
    std::cout << "[UIVisionPanel] === DEBUG NODE INFO ===" << std::endl;
    std::cout << "Config Manager: " << (m_configManager ? "OK" : "NULL") << std::endl;
    std::cout << "Machine Operations: " << (m_machineOperations ? "OK" : "NULL") << std::endl;
    std::cout << "Total nodes: " << m_nodes.size() << std::endl;

    if (m_configManager) {
      try {
        auto graphNames = m_configManager->GetAllGraphNames();
        std::cout << "Available graphs: " << graphNames.size() << std::endl;
        for (const auto& graphName : graphNames) {
          std::cout << "  - Graph: " << graphName << std::endl;
        }
      }
      catch (const std::exception& e) {
        std::cout << "Error getting graphs: " << e.what() << std::endl;
      }
    }

    for (size_t i = 0; i < m_nodes.size() && i < 5; i++) {
      const auto& node = m_nodes[i];
      std::cout << "Node " << i << ": " << node.name << " (Graph: " << node.graphName
        << ", Device: " << node.deviceName << ")" << std::endl;
    }
    std::cout << "=========================" << std::endl;
  }

  // Filter input
  ImGui::PushItemWidth(-1);
  ImGui::InputTextWithHint("##filter", "Filter nodes...", &m_filterText[0], 256);
  ImGui::PopItemWidth();

  ImGui::Text("Total: %zu nodes", m_nodes.size());
}

void UIVisionPanel::RenderNodeListTable() {
  if (m_nodes.empty()) {
    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No nodes found");

    if (!m_configManager) {
      ImGui::Text("Config Manager not connected");
    }
    else {
      ImGui::Text("Check motion configuration");
      ImGui::Text("Ensure graphs have nodes defined");

      if (ImGui::Button("Try Load Again", ImVec2(-1, 30))) {
        LoadNodesFromConfig();
      }
    }
    return;
  }

  // Filter nodes
  std::vector<NodeInfo*> filteredNodes;
  for (auto& node : m_nodes) {
    if (m_filterText.empty() ||
      node.name.find(m_filterText) != std::string::npos ||
      node.graphName.find(m_filterText) != std::string::npos) {
      filteredNodes.push_back(&node);
    }
  }

  ImGui::Text("Showing: %zu of %zu", filteredNodes.size(), m_nodes.size());

  // Node table
  if (ImGui::BeginTable("Nodes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_ScrollY, ImVec2(-1, -50))) {

    ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableHeadersRow();

    for (auto* node : filteredNodes) {
      ImGui::TableNextRow();

      // Node name column
      ImGui::TableNextColumn();

      // Color code based on status
      ImVec4 textColor = ImVec4(1, 1, 1, 1); // Default white
      if (node->isCurrentPosition) {
        textColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // Light green
      }
      else if (!node->isReachable) {
        textColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f); // Light red
      }

      ImGui::TextColored(textColor, "%s", node->name.c_str());

      // Show graph name as subtitle
      if (!node->graphName.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%s)", node->graphName.c_str());
      }

      // Action column
      ImGui::TableNextColumn();

      bool canNavigate = m_machineOperations && node->isReachable && !node->isCurrentPosition;

      if (!canNavigate) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
      }

      if (ImGui::Button(("Go##" + node->id).c_str(), ImVec2(40, 20))) {
        if (canNavigate) {
          std::cout << "[UIVisionPanel] Navigation button clicked for: " << node->id << std::endl;
          if (NavigateToNode(node->id)) {
            std::cout << "[UIVisionPanel] Navigation command sent successfully" << std::endl;
          }
          else {
            std::cout << "[UIVisionPanel] Navigation command failed" << std::endl;
          }
        }
      }

      if (!canNavigate) {
        ImGui::PopStyleVar();
      }

      // Tooltip
      if (ImGui::IsItemHovered()) {
        std::string tooltip = "Node: " + node->name;
        tooltip += "\nID: " + node->id;
        tooltip += "\nGraph: " + node->graphName;
        if (!node->deviceName.empty()) {
          tooltip += "\nDevice: " + node->deviceName;
        }
        if (!node->positionName.empty()) {
          tooltip += "\nPosition: " + node->positionName;
        }
        if (node->isCurrentPosition) {
          tooltip += "\nStatus: Current Position";
        }
        else if (!node->isReachable) {
          tooltip += "\nStatus: Not Reachable";
        }
        else if (!m_machineOperations) {
          tooltip += "\nStatus: Navigation Disabled";
        }
        else {
          tooltip += "\nStatus: Ready to Navigate";
        }
        ImGui::SetTooltip("%s", tooltip.c_str());
      }
    }

    ImGui::EndTable();
  }
}

void UIVisionPanel::LoadNodesFromConfig() {
  m_nodes.clear();

  if (!m_configManager) {
    std::cout << "[UIVisionPanel] LoadNodesFromConfig: No config manager!" << std::endl;
    return;
  }

  std::cout << "[UIVisionPanel] Loading nodes from configuration..." << std::endl;

  try {
    // Get all graph names
    auto graphNames = m_configManager->GetAllGraphNames();
    std::cout << "[UIVisionPanel] Found " << graphNames.size() << " graphs" << std::endl;

    for (const auto& graphName : graphNames) {
      std::cout << "[UIVisionPanel] Processing graph: " << graphName << std::endl;

      auto graphOpt = m_configManager->GetGraph(graphName);
      if (!graphOpt.has_value()) {
        std::cout << "[UIVisionPanel] Could not get graph: " << graphName << std::endl;
        continue;
      }

      const auto& graph = graphOpt.value().get();
      std::cout << "[UIVisionPanel] Graph " << graphName << " has " << graph.Nodes.size() << " nodes" << std::endl;

      // Process each node in the graph
      for (const auto& node : graph.Nodes) {
        NodeInfo nodeInfo;
        nodeInfo.id = node.Id;
        nodeInfo.name = !node.Label.empty() ? node.Label : node.Id;
        nodeInfo.graphName = graphName;
        nodeInfo.deviceName = node.Device;
        nodeInfo.positionName = node.Position;

        // Node is reachable if it has a device assigned
        nodeInfo.isReachable = !node.Device.empty();

        m_nodes.push_back(nodeInfo);

        std::cout << "[UIVisionPanel] Added node: " << nodeInfo.name
          << " (Device: " << nodeInfo.deviceName << ")" << std::endl;
      }
    }

    std::cout << "[UIVisionPanel] Successfully loaded " << m_nodes.size() << " nodes from configuration" << std::endl;

  }
  catch (const std::exception& e) {
    std::cout << "[UIVisionPanel] Exception loading nodes: " << e.what() << std::endl;
  }
}

bool UIVisionPanel::NavigateToNode(const std::string& nodeId) {
  std::cout << "[UIVisionPanel] NavigateToNode called with ID: " << nodeId << std::endl;

  if (!m_configManager || !m_machineOperations) {
    std::cout << "[UIVisionPanel] Cannot navigate: missing components (Config: "
      << (m_configManager ? "OK" : "NULL") << ", MachineOps: "
      << (m_machineOperations ? "OK" : "NULL") << ")" << std::endl;
    return false;
  }

  // Find the node
  NodeInfo* targetNode = nullptr;
  for (auto& node : m_nodes) {
    if (node.id == nodeId) {
      targetNode = &node;
      break;
    }
  }

  if (!targetNode) {
    std::cout << "[UIVisionPanel] Node not found: " << nodeId << std::endl;
    return false;
  }

  if (!targetNode->isReachable) {
    std::cout << "[UIVisionPanel] Node not reachable: " << nodeId << " (Device: " << targetNode->deviceName << ")" << std::endl;
    return false;
  }

  std::cout << "[UIVisionPanel] Attempting navigation to node: " << nodeId
    << " (Device: " << targetNode->deviceName
    << ", Graph: " << targetNode->graphName
    << ", Position: " << targetNode->positionName << ")" << std::endl;

  try {
    bool success = false;

    // Try to navigate using MoveDeviceToNode first
    if (!targetNode->graphName.empty() && !targetNode->deviceName.empty()) {
      std::cout << "[UIVisionPanel] Trying MoveDeviceToNode..." << std::endl;
      success = m_machineOperations->MoveDeviceToNode(
        targetNode->deviceName,
        targetNode->graphName,
        targetNode->id,
        false, // Don't wait for completion
        "UIVisionPanel"
      );

      if (success) {
        std::cout << "[UIVisionPanel] MoveDeviceToNode succeeded" << std::endl;
      }
      else {
        std::cout << "[UIVisionPanel] MoveDeviceToNode failed" << std::endl;
      }
    }

    // Fallback to direct position movement
    if (!success && !targetNode->positionName.empty() && !targetNode->deviceName.empty()) {
      std::cout << "[UIVisionPanel] Trying MoveToPointName fallback..." << std::endl;
      success = m_machineOperations->MoveToPointName(
        targetNode->deviceName,
        targetNode->positionName,
        false,
        "UIVisionPanel"
      );

      if (success) {
        std::cout << "[UIVisionPanel] MoveToPointName succeeded" << std::endl;
      }
      else {
        std::cout << "[UIVisionPanel] MoveToPointName failed" << std::endl;
      }
    }

    if (success) {
      std::cout << "[UIVisionPanel] Navigation command sent successfully for: " << nodeId << std::endl;

      // Update UI to show movement in progress
      for (auto& node : m_nodes) {
        node.isCurrentPosition = false; // Clear all current position flags
      }

      return true;
    }
    else {
      std::cout << "[UIVisionPanel] All navigation methods failed for: " << nodeId << std::endl;
      return false;
    }

  }
  catch (const std::exception& e) {
    std::cout << "[UIVisionPanel] Exception during navigation: " << e.what() << std::endl;
    return false;
  }
}

// Keep all existing circle detection methods unchanged...
void UIVisionPanel::InitializeCircleDetection() {
  m_circleDetector = std::make_unique<VisionCircleDetection>();

  if (!m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Creating default parameter file" << std::endl;
    if (VisionCircleDetection::CreateDefaultParameterFile(m_parameterFilePath)) {
      m_circleDetector->LoadParameters(m_parameterFilePath);
    }
  }

  std::cout << "[UIVisionPanel] Circle detection initialized successfully" << std::endl;
}

void UIVisionPanel::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  if (m_cameraManager) {
    std::cout << "[UIVisionPanel] Camera Manager connected" << std::endl;

    auto cameras = GetAvailableCameras();
    if (!cameras.empty()) {
      m_selectedCameraId = cameras[0];
      std::cout << "[UIVisionPanel] Auto-selected camera: " << m_selectedCameraId << std::endl;
    }
  }
}

void UIVisionPanel::RenderRightPanel() {
  ImGui::Text("Detection Results");
  ImGui::Separator();

  RenderCircleDetectionResults();
}

void UIVisionPanel::RenderCameraSelection() {
  ImGui::Text("Camera Source");

  if (!m_cameraManager) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Camera Manager not available");
    return;
  }

  auto cameras = GetAvailableCameras();

  if (cameras.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No cameras available");
    return;
  }

  if (ImGui::BeginCombo("Select Camera", m_selectedCameraId.c_str())) {
    for (const auto& cameraId : cameras) {
      bool isSelected = (m_selectedCameraId == cameraId);
      if (ImGui::Selectable(cameraId.c_str(), isSelected)) {
        m_selectedCameraId = cameraId;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (!m_selectedCameraId.empty()) {
    auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

    ImGui::Text("Status:");
    ImGui::SameLine();
    if (status.connected) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected");
    }

    if (!status.connected) {
      ImGui::SameLine();
      if (ImGui::Button("Connect")) {
        m_cameraManager->ConnectCamera(m_selectedCameraId);
      }
    }
  }
}

void UIVisionPanel::RenderCircleDetectionControls() {
  ImGui::Text("Circle Detection");

  if (!m_circleDetector) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Circle detector not initialized");
    return;
  }

  // Main execute button
  bool canExecute = m_cameraManager && !m_selectedCameraId.empty();

  if (!canExecute) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  // Change button color if auto-execution is active
  if (m_autoExecute) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.2f, 1.0f));  // Orange for auto mode
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.3f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));  // Green for manual
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  }

  std::string buttonText = m_autoExecute ? "Auto Detection Running" : "Manual Detection";
  if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 40))) {
    if (canExecute && !m_autoExecute) {  // Only allow manual execution when auto is off
      ExecuteCircleDetection();
    }
  }

  ImGui::PopStyleColor(2);

  if (!canExecute) {
    ImGui::PopStyleVar();
  }

  if (!canExecute) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Select and connect a camera first");
  }
  else if (m_autoExecute) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Auto-execution active (%.0f ms intervals)", m_autoExecuteInterval);
  }

  // Processing time display
  if (m_hasResult) {
    ImGui::Text("Last processing time: %.1f ms", m_circleDetector->GetLastProcessingTime());
  }
}

void UIVisionPanel::RenderCircleDetectionResults() {
  if (!m_hasResult) {
    ImGui::Text("No detection results yet.");
    ImGui::Text("Execute circle detection to see results.");
    return;
  }

  const auto& result = m_lastResult;

  ImGui::SetWindowFontScale(1.2f);
  if (result.found) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Circle Detected");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ No Circle Found");
  }
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();

  if (result.found) {
    if (ImGui::BeginTable("Results", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center X");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerX);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerY);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Radius");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.radius);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Confidence");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f%%", result.confidence * 100.0);

      ImGui::EndTable();
    }

    ImGui::Spacing();

    if (ImGui::Button("Send to Robot", ImVec2(-1, 30))) {
      std::cout << "[UIVisionPanel] Sending coordinates to robot: ("
        << result.centerX << ", " << result.centerY << ")" << std::endl;
    }
  }
  else {
    ImGui::Text("Detection failed:");
    ImGui::BulletText("Candidates found: %d", result.numCandidates);

    if (!result.errorMessage.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error: %s", result.errorMessage.c_str());
    }
  }
}

void UIVisionPanel::RenderCircleParameterControls() {
  if (!m_circleDetector) return;

  auto params = m_circleDetector->GetParameters();
  bool paramsChanged = false;

  ImGui::Text("Quick Presets:");
  if (ImGui::Button("Small", ImVec2(50, 25))) {
    params.minRadius = 10.0f;
    params.maxRadius = 30.0f;
    params.targetRadius = 20.0f;
    paramsChanged = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Medium", ImVec2(50, 25))) {
    params.minRadius = 40.0f;
    params.maxRadius = 80.0f;
    params.targetRadius = 60.0f;
    paramsChanged = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Large", ImVec2(50, 25))) {
    params.minRadius = 80.0f;
    params.maxRadius = 150.0f;
    params.targetRadius = 115.0f;
    paramsChanged = true;
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Circle Size:");
  if (ImGui::SliderFloat("Target", &params.targetRadius, 10.0f, 200.0f, "%.0f")) {
    paramsChanged = true;
  }
  if (ImGui::SliderFloat("Min", &params.minRadius, 1.0f, 200.0f, "%.0f")) {
    paramsChanged = true;
  }
  if (ImGui::SliderFloat("Max", &params.maxRadius, 1.0f, 200.0f, "%.0f")) {
    paramsChanged = true;
  }

  ImGui::Spacing();
  ImGui::Text("Threshold:");
  if (ImGui::SliderInt("Low", &params.thresholdLow, 0, 255)) {
    paramsChanged = true;
  }
  if (ImGui::SliderInt("High", &params.thresholdHigh, 0, 255)) {
    paramsChanged = true;
  }

  if (paramsChanged) {
    if (params.minRadius > params.maxRadius) {
      params.maxRadius = params.minRadius;
    }
    if (params.targetRadius < params.minRadius) {
      params.targetRadius = params.minRadius;
    }
    if (params.targetRadius > params.maxRadius) {
      params.targetRadius = params.maxRadius;
    }
    if (params.thresholdLow > params.thresholdHigh) {
      params.thresholdHigh = params.thresholdLow;
    }

    m_circleDetector->SetParameters(params);
  }
}

void UIVisionPanel::ExecuteCircleDetection() {
  if (!m_circleDetector || !m_cameraManager || m_selectedCameraId.empty()) {
    std::cout << "[UIVisionPanel] Cannot execute: missing components" << std::endl;
    return;
  }

  //std::cout << "[UIVisionPanel] Executing circle detection on camera: " << m_selectedCameraId << std::endl;

  std::vector<uint8_t> imageBuffer;
  int width, height, channels;

  if (!CaptureImageFromCamera(imageBuffer, width, height, channels)) {
    //std::cout << "[UIVisionPanel] Failed to capture image from camera" << std::endl;
    return;
  }

  m_lastResult = m_circleDetector->DetectFromBuffer(imageBuffer.data(), width, height, channels);
  m_hasResult = true;

  if (m_lastResult.found) {
    //std::cout << "[UIVisionPanel] Circle detected at (" << m_lastResult.centerX
    //  << ", " << m_lastResult.centerY << ") with radius " << m_lastResult.radius << std::endl;
  }
  else {
    //std::cout << "[UIVisionPanel] No circle detected. Candidates: " << m_lastResult.numCandidates << std::endl;
  }
}

std::vector<std::string> UIVisionPanel::GetAvailableCameras() {
  if (!m_cameraManager) {
    return {};
  }

  return m_cameraManager->GetCameraIds();
}

bool UIVisionPanel::CaptureImageFromCamera(std::vector<uint8_t>& imageBuffer, int& width, int& height, int& channels) {
  if (!m_cameraManager || m_selectedCameraId.empty()) {
    return false;
  }

  ICameraHardware* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (!camera || !camera->IsConnected()) {
    std::cout << "[UIVisionPanel] Camera not connected: " << m_selectedCameraId << std::endl;
    return false;
  }

  CameraFrameData frameData;
  if (!camera->CaptureFrame(frameData)) {
    std::cout << "[UIVisionPanel] Failed to capture frame" << std::endl;
    return false;
  }

  if (!frameData.IsValid() || frameData.imageData.empty()) {
    std::cout << "[UIVisionPanel] Invalid frame data" << std::endl;
    return false;
  }

  width = frameData.width;
  height = frameData.height;
  channels = frameData.channels;
  imageBuffer = frameData.imageData;

  UpdateImageTexture(imageBuffer, width, height, channels);

  return true;
}

void UIVisionPanel::LoadParameters() {
  if (m_circleDetector && m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters loaded from: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::SaveParameters() {
  if (m_circleDetector && m_circleDetector->SaveParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters saved to: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::ResetToDefaults() {
  if (m_circleDetector) {
    auto defaults = VisionCircleDetection::GetDefaultParameters();
    m_circleDetector->SetParameters(defaults);
    std::cout << "[UIVisionPanel] Parameters reset to defaults" << std::endl;
  }
}

void UIVisionPanel::RenderImageDisplay() {
  ImGui::Text("Image Display");
  ImGui::Separator();

  if (!m_hasImageData || m_imageTextureId == 0) {
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    ImVec2 placeholderSize = ImVec2(availableSize.x, availableSize.y - 30);

    ImGui::BeginChild("ImagePlaceholder", placeholderSize, true);

    ImVec2 centerPos = ImVec2(placeholderSize.x * 0.5f - 60, placeholderSize.y * 0.5f - 10);
    ImGui::SetCursorPos(centerPos);

    if (m_hasResult) {
      ImGui::Text("Image processed");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
      ImGui::Text("Click 'Execute' to");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 15, centerPos.y + 35));
      ImGui::Text("capture new image");
    }
    else {
      ImGui::Text("No image captured");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 30, centerPos.y + 20));
      ImGui::Text("Click 'Execute Detection'");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 35));
      ImGui::Text("to capture and process");
    }

    ImGui::EndChild();

    ImGui::Text("Image size: No image loaded");
    return;
  }

  RenderImageWithOverlay();

  ImGui::Text("Image size: %dx%d (%d channels)", m_imageWidth, m_imageHeight,
    m_lastImageData.size() / (m_imageWidth * m_imageHeight));
}

void UIVisionPanel::RenderImageWithOverlay() {
  if (m_imageTextureId == 0 || m_imageWidth == 0 || m_imageHeight == 0) {
    return;
  }

  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  availableSize.y -= 30;

  float imageAspect = static_cast<float>(m_imageWidth) / static_cast<float>(m_imageHeight);
  ImVec2 displaySize;

  if (availableSize.x / imageAspect <= availableSize.y) {
    displaySize.x = availableSize.x;
    displaySize.y = availableSize.x / imageAspect;
  }
  else {
    displaySize.x = availableSize.y * imageAspect;
    displaySize.y = availableSize.y;
  }

  ImVec2 imagePos = ImVec2(
    (availableSize.x - displaySize.x) * 0.5f,
    (availableSize.y - displaySize.y) * 0.5f
  );

  ImGui::SetCursorPos(imagePos);
  ImVec2 screenImagePos = ImGui::GetCursorScreenPos();

  ImGui::Image((ImTextureID)(intptr_t)m_imageTextureId, displaySize);

  if (m_hasResult && m_lastResult.found) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float scaleX = displaySize.x / m_imageWidth;
    float scaleY = displaySize.y / m_imageHeight;

    float centerX = screenImagePos.x + (m_lastResult.centerX * scaleX);
    float centerY = screenImagePos.y + (m_lastResult.centerY * scaleY);
    float radius = m_lastResult.radius * scaleX;

    ImU32 circleColor = IM_COL32(255, 0, 0, 255);
    drawList->AddCircle(ImVec2(centerX, centerY), radius, circleColor, 64, 2.0f);

    ImU32 crosshairColor = IM_COL32(0, 255, 0, 255);
    float crossSize = 10.0f;
    drawList->AddLine(
      ImVec2(centerX - crossSize, centerY),
      ImVec2(centerX + crossSize, centerY),
      crosshairColor, 2.0f
    );
    drawList->AddLine(
      ImVec2(centerX, centerY - crossSize),
      ImVec2(centerX, centerY + crossSize),
      crosshairColor, 2.0f
    );

    drawList->AddCircleFilled(ImVec2(centerX, centerY), 3.0f, crosshairColor);

    std::string coordText = "(" + std::to_string((int)m_lastResult.centerX) +
      ", " + std::to_string((int)m_lastResult.centerY) + ")";
    ImVec2 textPos(centerX + 15, centerY - 25);
    ImVec2 textSize = ImGui::CalcTextSize(coordText.c_str());

    drawList->AddRectFilled(
      ImVec2(textPos.x - 2, textPos.y - 2),
      ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
      IM_COL32(0, 0, 0, 180)
    );

    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), coordText.c_str());

    if (m_circleDetector) {
      auto params = m_circleDetector->GetParameters();

      float roiCenterX = screenImagePos.x + ((m_imageWidth / 2.0f + params.roiOffsetX) * scaleX);
      float roiCenterY = screenImagePos.y + ((m_imageHeight / 2.0f + params.roiOffsetY) * scaleY);
      float roiSize = params.roiSize * scaleX;

      ImVec2 roiTopLeft(roiCenterX - roiSize, roiCenterY - roiSize);
      ImVec2 roiBottomRight(roiCenterX + roiSize, roiCenterY + roiSize);

      ImU32 roiColor = IM_COL32(255, 255, 0, 128);
      drawList->AddRect(roiTopLeft, roiBottomRight, roiColor, 0.0f, 0, 1.0f);
    }
  }

  if (m_hasResult && m_lastResult.found) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● Detected Circle");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+ Center Point");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "□ ROI");
  }
}

void UIVisionPanel::UpdateImageTexture(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
  m_lastImageData = imageData;
  m_imageWidth = width;
  m_imageHeight = height;
  m_hasImageData = true;

  if (m_imageTextureId == 0) {
    glGenTextures(1, &m_imageTextureId);
  }

  glBindTexture(GL_TEXTURE_2D, m_imageTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (channels == 1) {
    std::vector<uint8_t> rgbData(width * height * 3);
    for (int i = 0; i < width * height; i++) {
      rgbData[i * 3 + 0] = imageData[i];
      rgbData[i * 3 + 1] = imageData[i];
      rgbData[i * 3 + 2] = imageData[i];
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
  }
  else if (channels == 3) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData.data());
  }
  else if (channels == 4) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
  }

  glBindTexture(GL_TEXTURE_2D, 0);

 // std::cout << "[UIVisionPanel] Updated image texture: " << width << "x" << height << " (" << channels << " channels)" << std::endl;
}

void UIVisionPanel::CleanupImageTexture() {
  if (m_imageTextureId != 0) {
    glDeleteTextures(1, &m_imageTextureId);
    m_imageTextureId = 0;
    m_hasImageData = false;
    std::cout << "[UIVisionPanel] Cleaned up image texture" << std::endl;
  }
}

// NEW: Auto-execution methods
void UIVisionPanel::UpdateAutoExecution() {
  if (!m_autoExecute) {
    return;
  }

  // Check if we can execute
  bool canExecute = m_cameraManager && !m_selectedCameraId.empty() && m_circleDetector;
  if (!canExecute) {
    return;
  }

  // Get current time in milliseconds
  float currentTime = ImGui::GetTime() * 1000.0f;

  // Check if enough time has passed since last execution
  if (currentTime - m_lastAutoExecuteTime >= m_autoExecuteInterval) {
    ExecuteCircleDetection();
    m_lastAutoExecuteTime = currentTime;
  }
}

void UIVisionPanel::RenderAutoExecutionControls() {
  ImGui::Text("Auto Execution");

  // Auto-execute toggle
  bool oldAutoExecute = m_autoExecute;
  if (ImGui::Checkbox("Enable Auto Execution", &m_autoExecute)) {
    if (m_autoExecute && !oldAutoExecute) {
      // Starting auto-execution
      m_lastAutoExecuteTime = ImGui::GetTime() * 1000.0f;
      std::cout << "[UIVisionPanel] Auto-execution started (interval: " << m_autoExecuteInterval << "ms)" << std::endl;
    }
    else if (!m_autoExecute && oldAutoExecute) {
      // Stopping auto-execution
      std::cout << "[UIVisionPanel] Auto-execution stopped" << std::endl;
    }
  }

  if (m_autoExecute) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
  }

  // Interval control
  float intervalMs = m_autoExecuteInterval;
  if (ImGui::SliderFloat("Interval (ms)", &intervalMs, 50.0f, 2000.0f, "%.0f ms")) {
    m_autoExecuteInterval = intervalMs;
  }

  // Quick preset buttons
  ImGui::Text("Quick Presets:");
  if (ImGui::Button("50ms", ImVec2(45, 25))) {
    m_autoExecuteInterval = 50.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("100ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 100.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("200ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 200.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("500ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 500.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("1s", ImVec2(35, 25))) {
    m_autoExecuteInterval = 1000.0f;
  }

  // Show execution rate
  if (m_autoExecute) {
    float fps = 1000.0f / m_autoExecuteInterval;
    ImGui::Text("Execution rate: %.1f Hz (%.0f ms)", fps, m_autoExecuteInterval);

    // Show time until next execution
    float currentTime = ImGui::GetTime() * 1000.0f;
    float timeUntilNext = m_autoExecuteInterval - (currentTime - m_lastAutoExecuteTime);
    if (timeUntilNext > 0) {
      ImGui::Text("Next execution in: %.0f ms", timeUntilNext);
    }
    else {
      ImGui::Text("Executing...");
    }
  }
  else {
    ImGui::Text("Manual execution only");
  }

  // Warning for high frequency
  if (m_autoExecuteInterval < 100.0f) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: High frequency may impact performance");
  }
}