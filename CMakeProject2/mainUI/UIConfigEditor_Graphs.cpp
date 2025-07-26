// UIConfigEditor_Graphs.cpp - Enhanced graph management functionality
#include "UIConfigEditor.h"
#include "imgui.h"
#include <set>
#include <cstring>

void UIConfigEditor::RenderGraphsTab() {
    // Set font scale for better readability
    ImGui::SetWindowFontScale(1.50f);

    // Get available content region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    float totalWidth = contentRegion.x;

    // Calculate column widths: 15%, 45%, 40%
    float leftColumnWidth = totalWidth * 0.15f;
    float middleColumnWidth = totalWidth * 0.45f;
    float rightColumnWidth = totalWidth * 0.40f;

    // Left panel - Graph list (15%)
    ImGui::BeginChild("GraphList", ImVec2(leftColumnWidth, 0), true);
    RenderGraphList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle panel - Nodes/Edges selection (45%)
    ImGui::BeginChild("NodesEdgesList", ImVec2(middleColumnWidth, 0), true);

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
        ImGui::TextWrapped("Select a graph first.");
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel - Node/Edge details (40%)
    ImGui::BeginChild("ElementDetails", ImVec2(rightColumnWidth, 0), true);

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
            ImGui::TextWrapped("Select a node or edge to edit its details.");
        }
    }
    else {
        ImGui::TextWrapped("Select a graph first.");
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

    // Action buttons row
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

    ImGui::SameLine();

    // Delete All Nodes button with confirmation
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Delete All Nodes")) {
        ImGui::OpenPopup("Delete All Nodes?");
    }
    ImGui::PopStyleColor(2);

    // Confirmation popup for deleting all nodes
    if (ImGui::BeginPopupModal("Delete All Nodes?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto graphOpt = configManager.GetGraph(m_selectedGraph);
        int nodeCount = 0;
        int filteredNodeCount = 0;

        if (graphOpt.has_value()) {
            const auto& graph = graphOpt.value().get();
            nodeCount = static_cast<int>(graph.Nodes.size());

            // Count filtered nodes
            for (const auto& node : graph.Nodes) {
                if (m_deviceFilter.empty() || node.Device == m_deviceFilter) {
                    filteredNodeCount++;
                }
            }
        }

        if (m_deviceFilter.empty()) {
            ImGui::Text("Are you sure you want to delete ALL %d nodes?", nodeCount);
        }
        else {
            ImGui::Text("Are you sure you want to delete %d nodes", filteredNodeCount);
            ImGui::Text("from device '%s'?", m_deviceFilter.c_str());
        }

        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This operation cannot be undone!");
        ImGui::Separator();

        if (ImGui::Button("Yes, Delete All", ImVec2(140, 0))) {
            DeleteAllNodes();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Node list
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

    // Action buttons row
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

    ImGui::SameLine();

    // Delete All Edges button with confirmation
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Delete All Edges")) {
        ImGui::OpenPopup("Delete All Edges?");
    }
    ImGui::PopStyleColor(2);

    // Confirmation popup for deleting all edges
    if (ImGui::BeginPopupModal("Delete All Edges?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto graphOpt = configManager.GetGraph(m_selectedGraph);
        int edgeCount = 0;
        int filteredEdgeCount = 0;

        if (graphOpt.has_value()) {
            const auto& graph = graphOpt.value().get();
            edgeCount = static_cast<int>(graph.Edges.size());

            // Count filtered edges
            std::map<std::string, const Node*> nodeMap;
            for (const auto& node : graph.Nodes) {
                nodeMap[node.Id] = &node;
            }

            for (const auto& edge : graph.Edges) {
                if (m_deviceFilter.empty()) {
                    filteredEdgeCount++;
                }
                else {
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
                    if (matchesFilter) {
                        filteredEdgeCount++;
                    }
                }
            }
        }

        if (m_deviceFilter.empty()) {
            ImGui::Text("Are you sure you want to delete ALL %d edges?", edgeCount);
        }
        else {
            ImGui::Text("Are you sure you want to delete %d edges", filteredEdgeCount);
            ImGui::Text("related to device '%s'?", m_deviceFilter.c_str());
        }

        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This operation cannot be undone!");
        ImGui::Separator();

        if (ImGui::Button("Yes, Delete All", ImVec2(140, 0))) {
            DeleteAllEdges();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Edge list
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

void UIConfigEditor::DeleteAllNodes() {
    if (m_selectedGraph.empty()) {
        m_logger->LogError("Cannot delete nodes: No graph selected");
        return;
    }

    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (!graphOpt.has_value()) {
        m_logger->LogError("Graph not found: " + m_selectedGraph);
        return;
    }

    Graph updatedGraph = graphOpt.value().get();
    auto originalNodeCount = updatedGraph.Nodes.size();

    // Remove nodes based on filter
    if (m_deviceFilter.empty()) {
        // Delete all nodes
        updatedGraph.Nodes.clear();
        // Also clear all edges since they'll be invalid
        updatedGraph.Edges.clear();
        m_logger->LogInfo("Deleted all nodes and edges from graph: " + m_selectedGraph);
    }
    else {
        // Delete only nodes matching the device filter
        auto nodeCountBefore = updatedGraph.Nodes.size();

        // Collect node IDs that will be deleted
        std::set<std::string> nodesToDelete;
        for (const auto& node : updatedGraph.Nodes) {
            if (node.Device == m_deviceFilter) {
                nodesToDelete.insert(node.Id);
            }
        }

        // Remove filtered nodes
        updatedGraph.Nodes.erase(
            std::remove_if(updatedGraph.Nodes.begin(), updatedGraph.Nodes.end(),
                [this](const Node& node) { return node.Device == m_deviceFilter; }),
            updatedGraph.Nodes.end());

        // Remove edges that reference deleted nodes
        updatedGraph.Edges.erase(
            std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
                [&nodesToDelete](const Edge& edge) {
                    return nodesToDelete.count(edge.Source) > 0 || nodesToDelete.count(edge.Target) > 0;
                }),
            updatedGraph.Edges.end());

        auto deletedNodeCount = nodeCountBefore - updatedGraph.Nodes.size();
        m_logger->LogInfo("Deleted " + std::to_string(deletedNodeCount) +
            " nodes from device '" + m_deviceFilter + "' in graph: " + m_selectedGraph);
    }

    try {
        configManager.UpdateGraph(m_selectedGraph, updatedGraph);
        m_selectedNode.clear();
        RefreshGraphData();
        SaveChanges();
    }
    catch (const std::exception& e) {
        m_logger->LogError("Failed to delete nodes: " + std::string(e.what()));
    }
}

void UIConfigEditor::DeleteAllEdges() {
    if (m_selectedGraph.empty()) {
        m_logger->LogError("Cannot delete edges: No graph selected");
        return;
    }

    auto graphOpt = configManager.GetGraph(m_selectedGraph);
    if (!graphOpt.has_value()) {
        m_logger->LogError("Graph not found: " + m_selectedGraph);
        return;
    }

    Graph updatedGraph = graphOpt.value().get();
    auto originalEdgeCount = updatedGraph.Edges.size();

    if (m_deviceFilter.empty()) {
        // Delete all edges
        updatedGraph.Edges.clear();
        m_logger->LogInfo("Deleted all edges from graph: " + m_selectedGraph);
    }
    else {
        // Delete only edges related to the filtered device
        std::map<std::string, const Node*> nodeMap;
        for (const auto& node : updatedGraph.Nodes) {
            nodeMap[node.Id] = &node;
        }

        auto edgeCountBefore = updatedGraph.Edges.size();

        updatedGraph.Edges.erase(
            std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
                [this, &nodeMap](const Edge& edge) {
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
                    return matchesFilter;
                }),
            updatedGraph.Edges.end());

        auto deletedEdgeCount = edgeCountBefore - updatedGraph.Edges.size();
        m_logger->LogInfo("Deleted " + std::to_string(deletedEdgeCount) +
            " edges related to device '" + m_deviceFilter + "' from graph: " + m_selectedGraph);
    }

    try {
        configManager.UpdateGraph(m_selectedGraph, updatedGraph);
        m_selectedEdge.clear();
        RefreshGraphData();
        SaveChanges();
    }
    catch (const std::exception& e) {
        m_logger->LogError("Failed to delete edges: " + std::string(e.what()));
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