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

  std::vector<NodeInfo*> filteredNodes;
  for (auto& node : m_nodes) {
    if (m_filterText.empty() ||
      node.name.find(m_filterText) != std::string::npos ||
      node.graphName.find(m_filterText) != std::string::npos) {
      filteredNodes.push_back(&node);
    }
  }

  ImGui::Text("Showing: %zu of %zu", filteredNodes.size(), m_nodes.size());

  if (ImGui::BeginTable("Nodes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_ScrollY, ImVec2(-1, -50))) {

    ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableHeadersRow();

    for (auto* node : filteredNodes) {
      ImGui::TableNextRow();

      ImGui::TableNextColumn();

      ImVec4 textColor = ImVec4(1, 1, 1, 1);
      if (node->isCurrentPosition) {
        textColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
      }
      else if (!node->isReachable) {
        textColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
      }

      ImGui::TextColored(textColor, "%s", node->name.c_str());

      if (!node->graphName.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%s)", node->graphName.c_str());
      }

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

bool UIVisionPanel::NavigateToNode(const std::string& nodeId) {
  std::cout << "[UIVisionPanel] NavigateToNode called with ID: " << nodeId << std::endl;

  if (!m_configManager || !m_machineOperations) {
    std::cout << "[UIVisionPanel] Cannot navigate: missing components (Config: "
      << (m_configManager ? "OK" : "NULL") << ", MachineOps: "
      << (m_machineOperations ? "OK" : "NULL") << ")" << std::endl;
    return false;
  }

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

    if (!targetNode->graphName.empty() && !targetNode->deviceName.empty()) {
      std::cout << "[UIVisionPanel] Trying MoveDeviceToNode..." << std::endl;
      success = m_machineOperations->MoveDeviceToNode(
        targetNode->deviceName,
        targetNode->graphName,
        targetNode->id,
        false,
        "UIVisionPanel"
      );

      if (success) {
        std::cout << "[UIVisionPanel] MoveDeviceToNode succeeded" << std::endl;
      }
      else {
        std::cout << "[UIVisionPanel] MoveDeviceToNode failed" << std::endl;
      }
    }

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

      for (auto& node : m_nodes) {
        node.isCurrentPosition = false;
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