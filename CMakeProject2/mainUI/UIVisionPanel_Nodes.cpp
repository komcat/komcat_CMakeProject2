// UIVisionPanel_Nodes.cpp - Node navigation functionality
#include "UIVisionPanel.h"
#include "include/motions/MotionConfigManager.h"
#include "include/machine_operations.h"
#include <iostream>

void UIVisionPanel::RenderNodeListPanel() {
  ImGui::Text("Node Navigation");
  ImGui::Separator();

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
  if (ImGui::Button("Refresh", ImVec2(60, 25))) {
    std::cout << "[UIVisionPanel] Manual refresh requested" << std::endl;
    LoadNodesFromConfig();
    std::cout << "[UIVisionPanel] After refresh: " << m_nodes.size() << " nodes" << std::endl;
  }

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

  char filterBuffer[256];
  strncpy(filterBuffer, m_filterText.c_str(), sizeof(filterBuffer) - 1);
  filterBuffer[sizeof(filterBuffer) - 1] = '\0';

  if (ImGui::InputText("Filter", filterBuffer, sizeof(filterBuffer))) {
    m_filterText = filterBuffer;
  }

  ImGui::Text("Total: %zu nodes", m_nodes.size());
}


// UPDATE: Modify RenderNodeListTable() to show preset associations
void UIVisionPanel::RenderNodeListTable() {
  if (m_nodes.empty()) {
    ImGui::Text("No nodes available");
    return;
  }

  if (ImGui::BeginTable("NodeTable", 4,
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

    ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthFixed, 80);  // NEW COLUMN
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < m_nodes.size(); i++) {
      const auto& node = m_nodes[i];

      // Skip filtered nodes
      if (!m_filterText.empty() &&
        node.id.find(m_filterText) == std::string::npos &&
        node.name.find(m_filterText) == std::string::npos) {
        continue;
      }

      ImGui::TableNextRow();

      // Node ID/Name
      ImGui::TableNextColumn();
      if (node.isCurrentPosition) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", node.id.c_str());
      }
      else if (!node.isReachable) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", node.id.c_str());
      }
      else {
        ImGui::Text("%s", node.id.c_str());
      }

      // Position info
      ImGui::TableNextColumn();
      ImGui::Text("%s", node.positionName.c_str());

      // NEW: Preset association
      ImGui::TableNextColumn();
      auto it = m_nodeToPresetMap.find(node.id);
      if (it != m_nodeToPresetMap.end()) {
        // Find preset name
        std::string presetName = "?";
        for (const auto& preset : m_availablePresets) {
          if (preset.id == it->second) {
            presetName = preset.name.substr(0, 8); // Truncate for space
            break;
          }
        }
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", presetName.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
      }

      // Go button
      ImGui::TableNextColumn();
      if (ImGui::Button(("Go##" + std::to_string(i)).c_str(), ImVec2(50, 20))) {
        NavigateToNode(node.id);
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

      for (const auto& node : graph.Nodes) {
        NodeInfo nodeInfo;
        nodeInfo.id = node.Id;
        nodeInfo.name = !node.Label.empty() ? node.Label : node.Id;
        nodeInfo.graphName = graphName;
        nodeInfo.deviceName = node.Device;
        nodeInfo.positionName = node.Position;

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


// UPDATE: Modify NavigateToNode() to auto-load preset (using your actual implementation pattern)
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
    std::cout << "[UIVisionPanel] Node not reachable: " << nodeId
      << " (Device: " << targetNode->deviceName << ")" << std::endl;
    return false;
  }

  std::cout << "[UIVisionPanel] Attempting navigation to node: " << nodeId
    << " (Device: " << targetNode->deviceName
    << ", Graph: " << targetNode->graphName
    << ", Position: " << targetNode->positionName << ")" << std::endl;

  try {
    bool success = false;

    // METHOD 1: Try MoveDeviceToNode first (preferred - follows graph navigation)
    if (!targetNode->graphName.empty() && !targetNode->deviceName.empty()) {
      std::cout << "[UIVisionPanel] Trying MoveDeviceToNode..." << std::endl;
      success = m_machineOperations->MoveDeviceToNode(
        targetNode->deviceName,  // Device name
        targetNode->graphName,   // Graph name
        targetNode->id,          // Node ID
        false,                   // waitForCompletion
        "UIVisionPanel"          // caller
      );

      if (success) {
        std::cout << "[UIVisionPanel] MoveDeviceToNode succeeded" << std::endl;
      }
      else {
        std::cout << "[UIVisionPanel] MoveDeviceToNode failed" << std::endl;
      }
    }

    // METHOD 2: Try MoveToPointName as fallback (direct position movement)
    if (!success && !targetNode->positionName.empty() && !targetNode->deviceName.empty()) {
      std::cout << "[UIVisionPanel] Trying MoveToPointName fallback..." << std::endl;
      success = m_machineOperations->MoveToPointName(
        targetNode->deviceName,   // Device name
        targetNode->positionName, // Position name
        false,                    // waitForCompletion
        "UIVisionPanel"           // caller
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

      // Update current position tracking
      for (auto& node : m_nodes) {
        node.isCurrentPosition = false;
      }
      targetNode->isCurrentPosition = true;
      m_selectedNodeId = nodeId;

      // NEW: Auto-load associated preset
      auto it = m_nodeToPresetMap.find(nodeId);
      if (it != m_nodeToPresetMap.end()) {
        // Find if auto-load is enabled
        bool shouldAutoLoad = true;
        for (const auto& mapping : m_nodePresetMappings) {
          if (mapping.nodeId == nodeId) {
            shouldAutoLoad = mapping.autoLoad;
            break;
          }
        }

        if (shouldAutoLoad) {
          if (LoadPreset(it->second)) {
            m_selectedPresetId = it->second;
            std::cout << "[UIVisionPanel] Auto-loaded preset " << it->second
              << " for node " << nodeId << std::endl;
          }
          else {
            std::cerr << "[UIVisionPanel] Failed to auto-load preset for node " << nodeId << std::endl;
          }
        }
        else {
          std::cout << "[UIVisionPanel] Auto-load disabled for node " << nodeId << std::endl;
        }
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