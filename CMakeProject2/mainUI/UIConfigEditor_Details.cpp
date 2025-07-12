// UIConfigEditor_Details.cpp - Node and Edge details rendering and management
#include "UIConfigEditor.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

void UIConfigEditor::RenderNodeDetails() {
  if (m_isAddingNewNode) {
    ImGui::Text("Adding New Node to %s", m_selectedGraph.c_str());
  }
  else {
    ImGui::Text("Editing Node: %s", m_selectedNode.c_str());
  }

  ImGui::Separator();

  // Node ID
  if (m_isAddingNewNode) {
    if (ImGui::InputText("Node ID", m_nodeIdBuffer, sizeof(m_nodeIdBuffer))) {
      m_newNodeId = m_nodeIdBuffer;
    }
  }
  else {
    ImGui::Text("Node ID: %s", m_nodeIdBuffer);
  }

  // Node Label
  if (ImGui::InputText("Label", m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer))) {
    if (m_isAddingNewNode) {
      m_newNodeLabel = m_nodeLabelBuffer;
    }
    else {
      m_editingNode.Label = m_nodeLabelBuffer;
    }
  }

  // Device selection dropdown
  if (ImGui::BeginCombo("Device", m_nodeDeviceBuffer)) {
    const auto& devices = configManager.GetAllDevices();
    for (const auto& [deviceName, device] : devices) {
      bool isSelected = (deviceName == std::string(m_nodeDeviceBuffer));
      if (ImGui::Selectable(deviceName.c_str(), isSelected)) {
        strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), deviceName.c_str(), _TRUNCATE);
        if (m_isAddingNewNode) {
          m_newNodeDevice = deviceName;
        }
        else {
          m_editingNode.Device = deviceName;
        }
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Position selection dropdown
  if (ImGui::BeginCombo("Position", m_nodePositionBuffer)) {
    std::string selectedDevice = m_isAddingNewNode ? m_newNodeDevice : m_editingNode.Device;
    if (!selectedDevice.empty()) {
      auto positionsOpt = configManager.GetDevicePositions(selectedDevice);
      if (positionsOpt.has_value()) {
        const auto& positions = positionsOpt.value().get();
        for (const auto& [posName, pos] : positions) {
          bool isSelected = (posName == std::string(m_nodePositionBuffer));
          if (ImGui::Selectable(posName.c_str(), isSelected)) {
            strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), posName.c_str(), _TRUNCATE);

            if (m_isAddingNewNode) {
              m_newNodePosition = posName;
            }
            else {
              m_editingNode.Position = posName;
            }
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
    }
    ImGui::EndCombo();
  }

  // Node position in graph
  int x = m_isAddingNewNode ? 100 : m_editingNode.X;
  int y = m_isAddingNewNode ? 100 : m_editingNode.Y;

  if (ImGui::InputInt("X Position", &x, 10, 50)) {
    if (m_isAddingNewNode) {
      m_editingNode.X = x;
    }
    else {
      m_editingNode.X = x;
    }
  }

  if (ImGui::InputInt("Y Position", &y, 10, 50)) {
    if (m_isAddingNewNode) {
      m_editingNode.Y = y;
    }
    else {
      m_editingNode.Y = y;
    }
  }

  ImGui::Separator();

  // Add/Update or Delete buttons
  if (m_isAddingNewNode) {
    if (ImGui::Button("Add Node")) {
      AddNewNode();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_isAddingNewNode = false;
    }
  }
  else if (!m_selectedNode.empty()) {
    if (ImGui::Button("Update Node")) {
      UpdateGraph();
      m_logger->LogInfo("Updated node: " + m_selectedNode + " in graph: " + m_selectedGraph);
      RefreshGraphData();
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete Node")) {
      ImGui::OpenPopup("Delete Node?");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupModal("Delete Node?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to delete node '%s'?", m_selectedNode.c_str());
      ImGui::Text("This operation cannot be undone!");
      ImGui::Separator();

      if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
        DeleteSelectedNode();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}

void UIConfigEditor::RenderEdgeDetails() {
  if (m_isAddingNewEdge) {
    ImGui::Text("Adding New Edge to %s", m_selectedGraph.c_str());
  }
  else {
    ImGui::Text("Editing Edge: %s", m_selectedEdge.c_str());
  }

  ImGui::Separator();

  // Edge ID
  if (m_isAddingNewEdge) {
    if (ImGui::InputText("Edge ID", m_edgeIdBuffer, sizeof(m_edgeIdBuffer))) {
      m_newEdgeId = m_edgeIdBuffer;
    }
  }
  else {
    ImGui::Text("Edge ID: %s", m_edgeIdBuffer);
  }

  // Edge Label
  if (ImGui::InputText("Label", m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer))) {
    if (m_isAddingNewEdge) {
      m_newEdgeLabel = m_edgeLabelBuffer;
    }
    else {
      m_editingEdge.Label = m_edgeLabelBuffer;
    }
  }

  // Source Node dropdown
  std::string sourceNodeLabel = "Source Node";
  std::string sourceNodeId = m_isAddingNewEdge ? m_newEdgeSource : m_editingEdge.Source;
  if (!sourceNodeId.empty() && !m_selectedGraph.empty()) {
    auto nodePtr = configManager.GetNodeById(m_selectedGraph, sourceNodeId);
    if (nodePtr && !nodePtr->Device.empty() && !nodePtr->Position.empty()) {
      sourceNodeLabel += " (" + nodePtr->Device + "." + nodePtr->Position + ")";
    }
  }
  if (ImGui::BeginCombo(sourceNodeLabel.c_str(), m_edgeSourceBuffer)) {
    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        std::string displayText = node.Id;
        if (!node.Device.empty() && !node.Position.empty()) {
          displayText += " (" + node.Device + "." + node.Position + ")";
        }

        bool isSelected = (node.Id == std::string(m_edgeSourceBuffer));
        if (ImGui::Selectable(displayText.c_str(), isSelected)) {
          strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), node.Id.c_str(), _TRUNCATE);
          if (m_isAddingNewEdge) {
            m_newEdgeSource = node.Id;
          }
          else {
            m_editingEdge.Source = node.Id;
          }
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
    }
    ImGui::EndCombo();
  }

  // Target Node dropdown
  std::string targetNodeLabel = "Target Node";
  std::string targetNodeId = m_isAddingNewEdge ? m_newEdgeTarget : m_editingEdge.Target;
  if (!targetNodeId.empty() && !m_selectedGraph.empty()) {
    auto nodePtr = configManager.GetNodeById(m_selectedGraph, targetNodeId);
    if (nodePtr && !nodePtr->Device.empty() && !nodePtr->Position.empty()) {
      targetNodeLabel += " (" + nodePtr->Device + "." + nodePtr->Position + ")";
    }
  }

  if (ImGui::BeginCombo(targetNodeLabel.c_str(), m_edgeTargetBuffer)) {
    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        std::string displayText = node.Id;
        if (!node.Device.empty() && !node.Position.empty()) {
          displayText += " (" + node.Device + "." + node.Position + ")";
        }

        bool isSelected = (node.Id == std::string(m_edgeTargetBuffer));
        if (ImGui::Selectable(displayText.c_str(), isSelected)) {
          strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), node.Id.c_str(), _TRUNCATE);
          if (m_isAddingNewEdge) {
            m_newEdgeTarget = node.Id;
          }
          else {
            m_editingEdge.Target = node.Id;
          }
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
    }
    ImGui::EndCombo();
  }

  // Edge Conditions
  ImGui::Text("Edge Conditions:");

  bool requiresApproval = m_isAddingNewEdge ? false : m_editingEdge.Conditions.RequiresOperatorApproval;
  if (ImGui::Checkbox("Requires Operator Approval", &requiresApproval)) {
    if (m_isAddingNewEdge) {
      m_editingEdge.Conditions.RequiresOperatorApproval = requiresApproval;
    }
    else {
      m_editingEdge.Conditions.RequiresOperatorApproval = requiresApproval;
    }
  }

  int timeout = m_isAddingNewEdge ? 30 : m_editingEdge.Conditions.TimeoutSeconds;
  if (ImGui::InputInt("Timeout (seconds)", &timeout, 5, 30)) {
    if (timeout < 0) timeout = 0;
    if (m_isAddingNewEdge) {
      m_editingEdge.Conditions.TimeoutSeconds = timeout;
    }
    else {
      m_editingEdge.Conditions.TimeoutSeconds = timeout;
    }
  }

  bool isBidirectional = m_isAddingNewEdge ? false : m_editingEdge.Conditions.IsBidirectional;
  if (ImGui::Checkbox("Bidirectional", &isBidirectional)) {
    if (m_isAddingNewEdge) {
      m_editingEdge.Conditions.IsBidirectional = isBidirectional;
    }
    else {
      m_editingEdge.Conditions.IsBidirectional = isBidirectional;
    }
  }

  ImGui::Separator();

  // Add/Update or Delete buttons
  if (m_isAddingNewEdge) {
    if (ImGui::Button("Add Edge")) {
      AddNewEdge();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_isAddingNewEdge = false;
    }
  }
  else if (!m_selectedEdge.empty()) {
    if (ImGui::Button("Update Edge")) {
      UpdateGraph();
      m_logger->LogInfo("Updated edge: " + m_selectedEdge + " in graph: " + m_selectedGraph);
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete Edge")) {
      ImGui::OpenPopup("Delete Edge?");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupModal("Delete Edge?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Are you sure you want to delete edge '%s'?", m_selectedEdge.c_str());
      ImGui::Text("This operation cannot be undone!");
      ImGui::Separator();

      if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
        DeleteSelectedEdge();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}

// Graph management helper methods
void UIConfigEditor::AddNewNode() {
  if (m_selectedGraph.empty() || m_newNodeId.empty()) {
    m_logger->LogError("Cannot add node: Invalid graph or node ID");
    return;
  }

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (!graphOpt.has_value()) {
    m_logger->LogError("Graph not found: " + m_selectedGraph);
    return;
  }

  const auto& graph = graphOpt.value().get();
  for (const auto& node : graph.Nodes) {
    if (node.Id == m_newNodeId) {
      m_logger->LogError("Node ID already exists: " + m_newNodeId);
      return;
    }
  }

  m_editingNode.Id = m_newNodeId;
  m_editingNode.Label = m_newNodeLabel;
  m_editingNode.Device = m_newNodeDevice;
  m_editingNode.Position = m_newNodePosition;

  UpdateGraph();

  m_logger->LogInfo("Added new node: " + m_newNodeId + " to graph: " + m_selectedGraph);

  m_selectedNode = m_newNodeId;
  m_isAddingNewNode = false;

  SaveChanges();
  RefreshGraphData();
}

void UIConfigEditor::DeleteSelectedNode() {
  if (m_selectedGraph.empty() || m_selectedNode.empty()) {
    return;
  }

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (!graphOpt.has_value()) {
    return;
  }

  const auto& graph = graphOpt.value().get();
  for (const auto& edge : graph.Edges) {
    if (edge.Source == m_selectedNode || edge.Target == m_selectedNode) {
      m_logger->LogError("Cannot delete node: " + m_selectedNode + " because it is used in edge: " + edge.Id);
      return;
    }
  }

  Graph updatedGraph = graph;
  auto beforeSize = updatedGraph.Nodes.size();

  updatedGraph.Nodes.erase(
    std::remove_if(updatedGraph.Nodes.begin(), updatedGraph.Nodes.end(),
      [this](const Node& node) { return node.Id == m_selectedNode; }),
    updatedGraph.Nodes.end());

  auto afterSize = updatedGraph.Nodes.size();

  if (beforeSize == afterSize) {
    m_logger->LogWarning("Node not found for deletion: " + m_selectedNode);
    return;
  }

  try {
    configManager.UpdateGraph(m_selectedGraph, updatedGraph);
    m_logger->LogInfo("Deleted node: " + m_selectedNode + " from graph: " + m_selectedGraph);
  }
  catch (const std::exception& e) {
    m_logger->LogError("Failed to delete node: " + std::string(e.what()));
    return;
  }

  m_selectedNode.clear();
  RefreshGraphData();
  SaveChanges();
}

void UIConfigEditor::AddNewEdge() {
  if (m_selectedGraph.empty() || m_newEdgeId.empty() || m_newEdgeSource.empty() || m_newEdgeTarget.empty()) {
    m_logger->LogError("Cannot add edge: Missing required fields");
    return;
  }

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (!graphOpt.has_value()) {
    m_logger->LogError("Graph not found: " + m_selectedGraph);
    return;
  }

  const auto& graph = graphOpt.value().get();
  for (const auto& edge : graph.Edges) {
    if (edge.Id == m_newEdgeId) {
      m_logger->LogError("Edge ID already exists: " + m_newEdgeId);
      return;
    }
  }

  m_editingEdge.Id = m_newEdgeId;
  m_editingEdge.Label = m_newEdgeLabel;
  m_editingEdge.Source = m_newEdgeSource;
  m_editingEdge.Target = m_newEdgeTarget;

  UpdateGraph();

  m_logger->LogInfo("Added new edge: " + m_newEdgeId + " to graph: " + m_selectedGraph);

  m_selectedEdge = m_newEdgeId;
  m_isAddingNewEdge = false;

  SaveChanges();
  RefreshGraphData();
}

void UIConfigEditor::DeleteSelectedEdge() {
  if (m_selectedGraph.empty() || m_selectedEdge.empty()) {
    return;
  }

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (!graphOpt.has_value()) {
    return;
  }

  Graph updatedGraph = graphOpt.value().get();
  auto beforeSize = updatedGraph.Edges.size();

  updatedGraph.Edges.erase(
    std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
      [this](const Edge& edge) { return edge.Id == m_selectedEdge; }),
    updatedGraph.Edges.end());

  auto afterSize = updatedGraph.Edges.size();

  if (beforeSize == afterSize) {
    m_logger->LogWarning("Edge not found for deletion: " + m_selectedEdge);
    return;
  }

  try {
    configManager.UpdateGraph(m_selectedGraph, updatedGraph);
    m_logger->LogInfo("Deleted edge: " + m_selectedEdge + " from graph: " + m_selectedGraph);
  }
  catch (const std::exception& e) {
    m_logger->LogError("Failed to delete edge: " + std::string(e.what()));
    return;
  }

  m_selectedEdge.clear();
  RefreshGraphData();
  SaveChanges();
}

void UIConfigEditor::UpdateGraph() {
  if (m_selectedGraph.empty()) {
    m_logger->LogError("Cannot update graph: No graph selected");
    return;
  }

  auto graphOpt = configManager.GetGraph(m_selectedGraph);
  if (!graphOpt.has_value()) {
    m_logger->LogError("Graph not found: " + m_selectedGraph);
    return;
  }

  Graph updatedGraph = graphOpt.value().get();

  // If we are editing a node, update or add it
  if (!m_selectedNode.empty() || m_isAddingNewNode) {
    bool isAdding = true;

    if (!m_selectedNode.empty()) {
      isAdding = false;
      updatedGraph.Nodes.erase(
        std::remove_if(updatedGraph.Nodes.begin(), updatedGraph.Nodes.end(),
          [this](const Node& node) { return node.Id == m_selectedNode; }),
        updatedGraph.Nodes.end());
    }

    Node nodeToAdd = m_editingNode;
    if (m_isAddingNewNode) {
      nodeToAdd.Id = m_newNodeId;
      nodeToAdd.Label = m_newNodeLabel;
      nodeToAdd.Device = m_newNodeDevice;
      nodeToAdd.Position = m_newNodePosition;
    }

    updatedGraph.Nodes.push_back(nodeToAdd);

    m_logger->LogInfo(isAdding ?
      "Added new node: " + nodeToAdd.Id + " to graph: " + m_selectedGraph :
      "Updated node: " + nodeToAdd.Id + " in graph: " + m_selectedGraph);
  }

  // If we are editing an edge, update or add it
  if (!m_selectedEdge.empty() || m_isAddingNewEdge) {
    bool isAdding = true;

    if (!m_selectedEdge.empty()) {
      isAdding = false;
      updatedGraph.Edges.erase(
        std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
          [this](const Edge& edge) { return edge.Id == m_selectedEdge; }),
        updatedGraph.Edges.end());
    }

    Edge edgeToAdd = m_editingEdge;
    if (m_isAddingNewEdge) {
      edgeToAdd.Id = m_newEdgeId;
      edgeToAdd.Label = m_newEdgeLabel;
      edgeToAdd.Source = m_newEdgeSource;
      edgeToAdd.Target = m_newEdgeTarget;
    }

    updatedGraph.Edges.push_back(edgeToAdd);

    m_logger->LogInfo(isAdding ?
      "Added new edge: " + edgeToAdd.Id + " to graph: " + m_selectedGraph :
      "Updated edge: " + edgeToAdd.Id + " in graph: " + m_selectedGraph);
  }

  try {
    configManager.UpdateGraph(m_selectedGraph, updatedGraph);
  }
  catch (const std::exception& e) {
    m_logger->LogError("Failed to update graph: " + std::string(e.what()));
    return;
  }
}