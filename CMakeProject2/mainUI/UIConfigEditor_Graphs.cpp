// UIConfigEditor_Graphs.cpp - Graph management functionality
#include "UIConfigEditor.h"
#include "imgui.h"
#include <set>
#include <cstring>

void UIConfigEditor::RenderGraphsTab() {
  // Left panel - Graph list
  ImGui::BeginChild("GraphList", ImVec2(200, 0), true);
  RenderGraphList();
  ImGui::EndChild();

  ImGui::SameLine();

  // Add resize handle/splitter
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 0.7f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 0.9f));
  ImGui::Button("##splitter", ImVec2(8.0f, -1));
  if (ImGui::IsItemActive()) {
    m_middleColumnWidth += ImGui::GetIO().MouseDelta.x;
    m_middleColumnWidth = std::max(m_middleColumnWidth, 100.0f);
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Middle panel - Nodes/Edges selection with dynamic width
  ImGui::BeginChild("NodesEdgesList", ImVec2(m_middleColumnWidth, 0), true);

  if (!m_selectedGraph.empty()) {
    // Tabs for nodes and edges
    if (ImGui::BeginTabBar("GraphElementsTab")) {
      if (ImGui::BeginTabItem("Nodes")) {
        RenderNodeList();
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Edges")) {
        RenderEdgeList();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
  }
  else {
    ImGui::Text("Select a graph first.");
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel - Node/Edge details
  ImGui::BeginChild("ElementDetails", ImVec2(0, 0), true);

  if (!m_selectedGraph.empty()) {
    if (!m_selectedNode.empty()) {
      RenderNodeDetails();
    }
    else if (!m_selectedEdge.empty()) {
      RenderEdgeDetails();
    }
    else if (m_isAddingNewNode) {
      RenderNodeDetails();
    }
    else if (m_isAddingNewEdge) {
      RenderEdgeDetails();
    }
    else {
      ImGui::Text("Select a node or edge to edit its details.");
    }
  }
  else {
    ImGui::Text("Select a graph first.");
  }

  ImGui::EndChild();
}

void UIConfigEditor::RenderGraphList() {
  ImGui::Text("Available Graphs");
  ImGui::Separator();

  const auto& graphs = configManager.GetAllGraphs();

  for (const auto& [name, graph] : graphs) {
    bool isSelected = (m_selectedGraph == name);
    if (ImGui::Selectable(name.c_str(), isSelected)) {
      m_selectedGraph = name;
      m_selectedNode.clear();
      m_selectedEdge.clear();
      m_isAddingNewNode = false;
      m_isAddingNewEdge = false;
      RefreshGraphData();
    }
  }
}

void UIConfigEditor::RenderNodeList() {
  ImGui::Text("Nodes for %s", m_selectedGraph.c_str());

  // Device filter dropdown
  if (ImGui::BeginCombo("Filter by Device", m_deviceFilter.empty() ? "All Devices" : m_deviceFilter.c_str())) {
    bool isSelected = m_deviceFilter.empty();
    if (ImGui::Selectable("All Devices", isSelected)) {
      m_deviceFilter = "";
    }
    if (isSelected) {
      ImGui::SetItemDefaultFocus();
    }

    std::set<std::string> devices;
    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        if (!node.Device.empty()) {
          devices.insert(node.Device);
        }
      }
    }

    for (const auto& device : devices) {
      isSelected = (m_deviceFilter == device);
      if (ImGui::Selectable(device.c_str(), isSelected)) {
        m_deviceFilter = device;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  if (ImGui::Button("Add New Node")) {
    m_isAddingNewNode = true;
    m_isAddingNewEdge = false;
    m_selectedNode.clear();
    m_selectedEdge.clear();

    m_editingNode = Node();
    m_newNodeId = "node_" + std::to_string(time(nullptr) % 10000);
    m_newNodeLabel = "New Node";
    m_newNodeDevice = m_deviceFilter;
    m_newNodePosition = "";

    strncpy_s(m_nodeIdBuffer, sizeof(m_nodeIdBuffer), m_newNodeId.c_str(), _TRUNCATE);
    strncpy_s(m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer), m_newNodeLabel.c_str(), _TRUNCATE);
    strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), m_newNodeDevice.c_str(), _TRUNCATE);
    strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), m_newNodePosition.c_str(), _TRUNCATE);
  }

  ImGui::Separator();

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (graphOpt.has_value()) {
    const auto& graph = graphOpt.value().get();

    for (const auto& node : graph.Nodes) {
      if (!m_deviceFilter.empty() && node.Device != m_deviceFilter) {
        continue;
      }

      bool isSelected = (m_selectedNode == node.Id);

      std::string displayText;
      if (!node.Label.empty()) {
        displayText = node.Label + " (" + node.Id + ")";
      }
      else {
        displayText = node.Id;
      }

      if (!node.Device.empty() && !node.Position.empty()) {
        displayText += " - " + node.Device + "." + node.Position;
      }

      if (ImGui::Selectable(displayText.c_str(), isSelected)) {
        m_selectedNode = node.Id;
        m_selectedEdge.clear();
        m_isAddingNewNode = false;
        m_isAddingNewEdge = false;

        m_editingNode = node;

        strncpy_s(m_nodeIdBuffer, sizeof(m_nodeIdBuffer), node.Id.c_str(), _TRUNCATE);
        strncpy_s(m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer), node.Label.c_str(), _TRUNCATE);
        strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), node.Device.c_str(), _TRUNCATE);
        strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), node.Position.c_str(), _TRUNCATE);
      }
    }
  }
}

void UIConfigEditor::RenderEdgeList() {
  ImGui::Text("Edges for %s", m_selectedGraph.c_str());

  // Device filter dropdown
  if (ImGui::BeginCombo("Filter by Device", m_deviceFilter.empty() ? "All Devices" : m_deviceFilter.c_str())) {
    bool isSelected = m_deviceFilter.empty();
    if (ImGui::Selectable("All Devices", isSelected)) {
      m_deviceFilter = "";
    }
    if (isSelected) {
      ImGui::SetItemDefaultFocus();
    }

    std::set<std::string> devices;
    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        if (!node.Device.empty()) {
          devices.insert(node.Device);
        }
      }
    }

    for (const auto& device : devices) {
      isSelected = (m_deviceFilter == device);
      if (ImGui::Selectable(device.c_str(), isSelected)) {
        m_deviceFilter = device;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  if (ImGui::Button("Add New Edge")) {
    m_isAddingNewEdge = true;
    m_isAddingNewNode = false;
    m_selectedEdge.clear();
    m_selectedNode.clear();

    m_editingEdge = Edge();
    m_newEdgeId = "edge_" + std::to_string(time(nullptr) % 10000);
    m_newEdgeLabel = "New Edge";
    m_newEdgeSource = "";
    m_newEdgeTarget = "";

    strncpy_s(m_edgeIdBuffer, sizeof(m_edgeIdBuffer), m_newEdgeId.c_str(), _TRUNCATE);
    strncpy_s(m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer), m_newEdgeLabel.c_str(), _TRUNCATE);
    strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), m_newEdgeSource.c_str(), _TRUNCATE);
    strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), m_newEdgeTarget.c_str(), _TRUNCATE);
  }

  ImGui::Separator();

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (graphOpt.has_value()) {
    const auto& graph = graphOpt.value().get();

    std::map<std::string, const Node*> nodeMap;
    for (const auto& node : graph.Nodes) {
      nodeMap[node.Id] = &node;
    }

    for (const auto& edge : graph.Edges) {
      if (!m_deviceFilter.empty()) {
        bool matchesFilter = false;

        auto sourceIt = nodeMap.find(edge.Source);
        if (sourceIt != nodeMap.end() && sourceIt->second->Device == m_deviceFilter) {
          matchesFilter = true;
        }

        if (!matchesFilter) {
          auto targetIt = nodeMap.find(edge.Target);
          if (targetIt != nodeMap.end() && targetIt->second->Device == m_deviceFilter) {
            matchesFilter = true;
          }
        }

        if (!matchesFilter) {
          continue;
        }
      }

      bool isSelected = (m_selectedEdge == edge.Id);

      std::string sourceLabel = "unknown";
      std::string targetLabel = "unknown";

      auto sourceIt = nodeMap.find(edge.Source);
      if (sourceIt != nodeMap.end()) {
        sourceLabel = sourceIt->second->Label.empty() ? edge.Source : sourceIt->second->Label;
      }

      auto targetIt = nodeMap.find(edge.Target);
      if (targetIt != nodeMap.end()) {
        targetLabel = targetIt->second->Label.empty() ? edge.Target : targetIt->second->Label;
      }

      std::string directionSymbol = edge.Conditions.IsBidirectional ? " <-> " : " -> ";
      std::string edgeLabel = edge.Label.empty() ? edge.Id : edge.Label;
      std::string displayText = edgeLabel + " (" + sourceLabel + directionSymbol + targetLabel + ")";

      if (ImGui::Selectable(displayText.c_str(), isSelected)) {
        m_selectedEdge = edge.Id;
        m_selectedNode.clear();
        m_isAddingNewNode = false;
        m_isAddingNewEdge = false;

        m_editingEdge = edge;

        strncpy_s(m_edgeIdBuffer, sizeof(m_edgeIdBuffer), edge.Id.c_str(), _TRUNCATE);
        strncpy_s(m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer), edge.Label.c_str(), _TRUNCATE);
        strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), edge.Source.c_str(), _TRUNCATE);
        strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), edge.Target.c_str(), _TRUNCATE);
      }
    }
  }
}

void UIConfigEditor::RefreshGraphData() {
  m_selectedNode.clear();
  m_selectedEdge.clear();
  m_isAddingNewNode = false;
  m_isAddingNewEdge = false;

  m_nodeIdBuffer[0] = '\0';
  m_nodeLabelBuffer[0] = '\0';
  m_nodeDeviceBuffer[0] = '\0';
  m_nodePositionBuffer[0] = '\0';
  m_edgeIdBuffer[0] = '\0';
  m_edgeLabelBuffer[0] = '\0';
  m_edgeSourceBuffer[0] = '\0';
  m_edgeTargetBuffer[0] = '\0';

  m_logger->LogInfo("Refreshing graph data for " + m_selectedGraph);
}