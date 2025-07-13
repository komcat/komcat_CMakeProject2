// NodePropertiesHandler.cpp
#include "NodePropertiesHandler.h"
#include "imgui.h"
#include <iomanip>
#include <sstream>

NodePropertiesHandler::NodePropertiesHandler(MotionConfigManager& configManager, Logger* logger)
  : m_configManager(configManager), m_logger(logger) {
  m_logger->LogInfo("NodePropertiesHandler initialized");
}

NodePropertiesHandler::~NodePropertiesHandler() {
  m_logger->LogInfo("NodePropertiesHandler destroyed");
}

void NodePropertiesHandler::ShowNodeProperties(const std::string& graphName, const std::string& nodeId) {
  m_currentGraphName = graphName;
  m_currentNodeId = nodeId;
  m_showDialog = true;

  // Load the node data
  LoadNodeData();

  m_logger->LogInfo("Showing properties for node: " + nodeId + " in graph: " + graphName);
}

void NodePropertiesHandler::LoadNodeData() {
  m_positionFound = false;

  // Get the node from the graph
  auto graphOpt = m_configManager.GetGraph(m_currentGraphName);
  if (!graphOpt.has_value()) {
    m_logger->LogError("Graph not found: " + m_currentGraphName);
    return;
  }

  const auto& graph = graphOpt.value().get();

  // Find the node
  bool nodeFound = false;
  for (const auto& node : graph.Nodes) {
    if (node.Id == m_currentNodeId) {
      m_currentNode = node;
      nodeFound = true;
      break;
    }
  }

  if (!nodeFound) {
    m_logger->LogError("Node not found: " + m_currentNodeId);
    return;
  }

  // Get the position data if available
  if (!m_currentNode.Device.empty() && !m_currentNode.Position.empty()) {
    auto positionOpt = m_configManager.GetNamedPosition(m_currentNode.Device, m_currentNode.Position);
    if (positionOpt.has_value()) {
      m_currentPosition = positionOpt.value();
      m_positionFound = true;
    }
  }
}

void NodePropertiesHandler::RenderPropertiesDialog() {
  if (!m_showDialog) return;

  // Set dialog size and position
  ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  // Create the modal dialog
  if (ImGui::BeginPopupModal(("Node Properties##" + m_currentNodeId).c_str(), &m_showDialog,
    ImGuiWindowFlags_AlwaysAutoResize)) {

    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Node Properties");
    ImGui::Separator();

    // Render different sections
    RenderNodeInfo();
    ImGui::Spacing();

    RenderDeviceInfo();
    ImGui::Spacing();

    RenderPositionInfo();
    ImGui::Spacing();

    RenderGraphInfo();

    // Close button
    ImGui::Separator();
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) / 2);
    if (ImGui::Button("Close", ImVec2(100, 30))) {
      m_showDialog = false;
    }

    ImGui::EndPopup();
  }

  // Open the modal if we just set showDialog to true
  if (m_showDialog && !ImGui::IsPopupOpen(("Node Properties##" + m_currentNodeId).c_str())) {
    ImGui::OpenPopup(("Node Properties##" + m_currentNodeId).c_str());
  }
}

void NodePropertiesHandler::RenderNodeInfo() {
  if (ImGui::CollapsingHeader("Node Information", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginTable("NodeInfoTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Node ID
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Node ID");
    ImGui::TableNextColumn(); ImGui::Selectable(m_currentNode.Id.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

    // Node Label
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Label");
    ImGui::TableNextColumn();
    if (m_currentNode.Label.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(No label)");
    }
    else {
      ImGui::Selectable(m_currentNode.Label.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);
    }

    // Graph Name
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Graph");
    ImGui::TableNextColumn(); ImGui::Selectable(m_currentGraphName.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

    ImGui::EndTable();
  }
}

void NodePropertiesHandler::RenderDeviceInfo() {
  if (ImGui::CollapsingHeader("Device Information", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginTable("DeviceInfoTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Device Name
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Device");
    ImGui::TableNextColumn();
    if (m_currentNode.Device.empty()) {
      ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "(No device assigned)");
    }
    else {
      ImGui::Selectable(m_currentNode.Device.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);
    }

    // Position Name
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Position");
    ImGui::TableNextColumn();
    if (m_currentNode.Position.empty()) {
      ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "(No position assigned)");
    }
    else {
      ImGui::Selectable(m_currentNode.Position.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);
    }

    // Device Status (if device is assigned)
    if (!m_currentNode.Device.empty()) {
      auto deviceOpt = m_configManager.GetDevice(m_currentNode.Device);
      if (deviceOpt.has_value()) {
        const auto& device = deviceOpt.value().get();

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IP Address");
        ImGui::TableNextColumn(); ImGui::Selectable(device.IpAddress.c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Port");
        ImGui::TableNextColumn(); ImGui::Text("%d", device.Port);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Device ID");
        ImGui::TableNextColumn(); ImGui::Text("%d", device.Id);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Enabled");
        ImGui::TableNextColumn();
        if (device.IsEnabled) {
          ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Yes");
        }
        else {
          ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "No");
        }
      }
    }

    ImGui::EndTable();
  }
}

void NodePropertiesHandler::RenderPositionInfo() {
  if (ImGui::CollapsingHeader("Position Information", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (m_positionFound) {
      ImGui::BeginTable("PositionInfoTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
      ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      // Format numbers with precision
      auto formatPosition = [](double value) -> std::string {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6) << value;
        return ss.str();
      };

      // X, Y, Z coordinates
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("X");
      ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.x).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Y");
      ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.y).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Z");
      ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.z).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

      // U, V, W coordinates (if this is a hex device)
      if (m_currentNode.Device.find("hex") != std::string::npos) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("U");
        ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.u).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("V");
        ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.v).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("W");
        ImGui::TableNextColumn(); ImGui::Selectable(formatPosition(m_currentPosition.w).c_str(), false, ImGuiSelectableFlags_AllowItemOverlap);
      }

      ImGui::EndTable();
    }
    else {
      ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Position data not found");
      if (m_currentNode.Device.empty() || m_currentNode.Position.empty()) {
        ImGui::Text("Device or position not assigned to this node.");
      }
      else {
        ImGui::Text("Position '%s' not found for device '%s'.",
          m_currentNode.Position.c_str(), m_currentNode.Device.c_str());
      }
    }
  }
}

void NodePropertiesHandler::RenderGraphInfo() {
  if (ImGui::CollapsingHeader("Graph Layout Information")) {
    ImGui::BeginTable("GraphInfoTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Graph position
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Graph X");
    ImGui::TableNextColumn(); ImGui::Text("%d", m_currentNode.X);

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("Graph Y");
    ImGui::TableNextColumn(); ImGui::Text("%d", m_currentNode.Y);

    // Additional graph statistics
    auto graphOpt = m_configManager.GetGraph(m_currentGraphName);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();

      // Count edges connected to this node
      int connectedEdges = 0;
      for (const auto& edge : graph.Edges) {
        if (edge.Source == m_currentNodeId || edge.Target == m_currentNodeId) {
          connectedEdges++;
        }
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Connected Edges");
      ImGui::TableNextColumn(); ImGui::Text("%d", connectedEdges);
    }

    ImGui::EndTable();
  }
}