// GraphVisualizer.cpp
#include "include/ui/GraphVisualizer.h"
#include <cmath>
#include <algorithm>

GraphVisualizer::GraphVisualizer(MotionConfigManager& configManager)
    : m_configManager(configManager)
    , m_logger(Logger::GetInstance())
{
    m_logger->LogInfo("GraphVisualizer initialized");

    // Set a default active graph if available
    const auto& graphs = m_configManager.GetAllGraphs();
    if (!graphs.empty()) {
        m_activeGraph = graphs.begin()->first;
        m_logger->LogInfo("Default active graph set to: " + m_activeGraph);
    }
}

void GraphVisualizer::SetActiveGraph(const std::string& graphName) {
    if (m_activeGraph != graphName) {
        m_activeGraph = graphName;
        m_zoomLevel = 1.0f;
        m_panOffset = ImVec2(0, 0);
        m_logger->LogInfo("Active graph set to: " + graphName);
    }
}

// GraphVisualizer.cpp - simplified RenderUI method
void GraphVisualizer::RenderUI() {
    if (!m_showWindow) return;

    // Begin the main window with no special flags
    ImGui::Begin("Graph Visualizer", &m_showWindow);

    // Graph selection dropdown
    const auto& allGraphs = m_configManager.GetAllGraphs();
    if (ImGui::BeginCombo("Select Graph", m_activeGraph.c_str())) {
        for (const auto& [graphName, graph] : allGraphs) {
            bool isSelected = (m_activeGraph == graphName);
            if (ImGui::Selectable(graphName.c_str(), isSelected)) {
                SetActiveGraph(graphName);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Add zoom controls
    if (ImGui::Button("Zoom In")) {
        m_zoomLevel = std::min(m_zoomLevel * 1.2f, 3.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) {
        m_zoomLevel = std::max(m_zoomLevel / 1.2f, 0.3f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_zoomLevel = 1.0f;
        m_panOffset = ImVec2(0, 0);
    }

    // Display instructions
    ImGui::Text("Drag nodes to reposition them. Positions will be saved automatically.");
    ImGui::Text("Use middle mouse button to pan, mouse wheel to zoom.");

    // Calculate canvas size and position
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Ensure we have at least some space to draw
    if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
    if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

    // Create a child frame for the canvas with NoMove flag
    ImGui::BeginChildFrame(ImGui::GetID("CanvasFrame"), canvasSize,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

    // Get the canvas position for coordinate calculations
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Track if the canvas is hovered
    m_isCanvasHovered = ImGui::IsWindowHovered();

    // Get the draw list for custom rendering
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Handle input first
    HandleInput(canvasPos, canvasSize);

    // Render the graph
    RenderBackground(drawList, canvasPos, canvasSize);

    // Only draw graph content if we have a selected graph
    if (!m_activeGraph.empty()) {
        RenderEdges(drawList, canvasPos);
        RenderNodes(drawList, canvasPos);
    }

    ImGui::EndChildFrame();
    ImGui::End();
}

void GraphVisualizer::HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Get current mouse position
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    // Handle zooming with mouse wheel
    if (m_isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
        float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
        float prevZoom = m_zoomLevel;
        m_zoomLevel = std::max(0.3f, std::min(m_zoomLevel + zoomDelta, 3.0f));

        // Adjust pan to zoom toward mouse position
        if (m_zoomLevel != prevZoom) {
            ImVec2 mouseGraphPos = CanvasToGraph(mousePos, canvasPos);
            float zoomRatio = m_zoomLevel / prevZoom;
            ImVec2 newMouseGraphPos = ImVec2(mouseGraphPos.x * zoomRatio, mouseGraphPos.y * zoomRatio);
            m_panOffset.x += (mouseGraphPos.x - newMouseGraphPos.x);
            m_panOffset.y += (mouseGraphPos.y - newMouseGraphPos.y);
        }
    }

    // Handle panning with middle mouse button
    if (m_isCanvasHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
            m_panOffset.x += dragDelta.x / m_zoomLevel;
            m_panOffset.y += dragDelta.y / m_zoomLevel;
        }
    }

    // Node dragging with left mouse button
    if (m_isCanvasHovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_isDragging) {
            // Find the node under the cursor
            std::string nodeId = GetNodeAtPosition(mousePos, canvasPos);

            if (!nodeId.empty()) {
                // Start dragging
                m_isDragging = true;
                m_draggedNodeId = nodeId;
                m_dragStartPos = mousePos;
                m_lastMousePos = mousePos;

                m_logger->LogInfo("Started dragging node: " + nodeId);
            }
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
            // Continue dragging
            ImVec2 dragDelta = ImVec2(mousePos.x - m_lastMousePos.x, mousePos.y - m_lastMousePos.y);

            // Apply the delta to the node position
            auto graphOpt = m_configManager.GetGraph(m_activeGraph);
            if (graphOpt.has_value()) {
                const auto& graph = graphOpt.value().get();

                // Find the dragged node
                const Node* draggedNode = nullptr;
                for (const auto& node : graph.Nodes) {
                    if (node.Id == m_draggedNodeId) {
                        draggedNode = &node;
                        break;
                    }
                }

                if (draggedNode) {
                    // Calculate new position
                    ImVec2 nodePos = GetNodePosition(*draggedNode);
                    ImVec2 newNodePos = ImVec2(
                        nodePos.x + dragDelta.x / m_zoomLevel,
                        nodePos.y + dragDelta.y / m_zoomLevel
                    );

                    // Update position in the graph
                    Graph updatedGraph = graph;
                    for (Node& node : updatedGraph.Nodes) {
                        if (node.Id == m_draggedNodeId) {
                            node.X = static_cast<int>(newNodePos.x);
                            node.Y = static_cast<int>(newNodePos.y);
                            m_configManager.UpdateGraph(m_activeGraph, updatedGraph);
                            break;
                        }
                    }
                }
            }

            // Update last mouse position for next frame
            m_lastMousePos = mousePos;
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
            // End dragging
            auto graphOpt = m_configManager.GetGraph(m_activeGraph);
            if (graphOpt.has_value()) {
                const auto& graph = graphOpt.value().get();

                const Node* draggedNode = nullptr;
                for (const auto& node : graph.Nodes) {
                    if (node.Id == m_draggedNodeId) {
                        draggedNode = &node;
                        break;
                    }
                }

                if (draggedNode) {
                    ImVec2 nodePos = GetNodePosition(*draggedNode);

                    // Save changes permanently
                    SaveNodePosition(m_draggedNodeId, nodePos);
                    m_logger->LogInfo("Saved position for node: " + m_draggedNodeId +
                        " at X:" + std::to_string(static_cast<int>(nodePos.x)) +
                        ", Y:" + std::to_string(static_cast<int>(nodePos.y)));
                }
            }

            // Clear drag state
            m_isDragging = false;
            m_draggedNodeId.clear();
        }
    }
    else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
        // Handle the case where mouse is released outside the canvas
        m_isDragging = false;
        m_draggedNodeId.clear();
    }
}



void GraphVisualizer::RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Draw the canvas background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(40, 40, 40, 255));

    // Draw a grid
    float gridSize = 50.0f * m_zoomLevel;
    ImU32 gridColor = IM_COL32(60, 60, 60, 200);

    // Adjust grid offset based on panning
    float offsetX = fmodf(m_panOffset.x * m_zoomLevel, gridSize);
    float offsetY = fmodf(m_panOffset.y * m_zoomLevel, gridSize);

    // Draw vertical grid lines
    for (float x = offsetX; x < canvasSize.x; x += gridSize) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
            gridColor
        );
    }

    // Draw horizontal grid lines
    for (float y = offsetY; y < canvasSize.y; y += gridSize) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
            gridColor
        );
    }
}

void GraphVisualizer::RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos) {
    auto graphOpt = m_configManager.GetGraph(m_activeGraph);
    if (!graphOpt.has_value()) return;

    const auto& graph = graphOpt.value().get();

    // Render each node as a rectangle
    for (const auto& node : graph.Nodes) {
        // Get node center position
        ImVec2 nodePos = GetNodePosition(node);

        // Convert to canvas coordinates
        ImVec2 canvasNodePos = GraphToCanvas(nodePos, canvasPos);

        // Calculate node rectangle positions
        ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);
        ImVec2 nodeMax = ImVec2(canvasNodePos.x + NODE_WIDTH / 2, canvasNodePos.y + NODE_HEIGHT / 2);

        // Determine node color based on whether it's being dragged
        ImU32 fillColor = (m_draggedNodeId == node.Id) ? SELECTED_NODE_COLOR : NODE_COLOR;

        // Draw node background rectangle
        drawList->AddRectFilled(nodeMin, nodeMax, fillColor, NODE_ROUNDING);
        drawList->AddRect(nodeMin, nodeMax, NODE_BORDER_COLOR, NODE_ROUNDING, 0, 1.5f);

        // Add node label
        std::string nodeLabel = node.Label;
        ImVec2 textSize = ImGui::CalcTextSize(nodeLabel.c_str());
        drawList->AddText(
            ImVec2(canvasNodePos.x - textSize.x / 2, nodeMin.y + TEXT_PADDING),
            IM_COL32(255, 255, 255, 255),
            nodeLabel.c_str()
        );

        // Add device and position info
        std::string deviceInfo = "Device: " + node.Device;
        ImVec2 deviceTextSize = ImGui::CalcTextSize(deviceInfo.c_str());
        drawList->AddText(
            ImVec2(canvasNodePos.x - deviceTextSize.x / 2, nodeMin.y + textSize.y + 2 * TEXT_PADDING),
            IM_COL32(200, 200, 200, 255),
            deviceInfo.c_str()
        );

        std::string posInfo = "Position: " + node.Position;
        ImVec2 posTextSize = ImGui::CalcTextSize(posInfo.c_str());
        drawList->AddText(
            ImVec2(canvasNodePos.x - posTextSize.x / 2, nodeMin.y + textSize.y + deviceTextSize.y + 3 * TEXT_PADDING),
            IM_COL32(200, 200, 200, 255),
            posInfo.c_str()
        );

        // Show coordinates for debugging
        std::string coordInfo = "X: " + std::to_string(node.X) + ", Y: " + std::to_string(node.Y);
        ImVec2 coordTextSize = ImGui::CalcTextSize(coordInfo.c_str());
        drawList->AddText(
            ImVec2(canvasNodePos.x - coordTextSize.x / 2, nodeMin.y + textSize.y + deviceTextSize.y + posTextSize.y + 4 * TEXT_PADDING),
            IM_COL32(150, 150, 150, 255),
            coordInfo.c_str()
        );
    }
}

// In GraphVisualizer.cpp, update the RenderEdges function

void GraphVisualizer::RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos) {
    auto graphOpt = m_configManager.GetGraph(m_activeGraph);
    if (!graphOpt.has_value()) return;

    const auto& graph = graphOpt.value().get();

    // First create a map of nodes by ID for quick lookup
    std::map<std::string, const Node*> nodeMap;
    for (const auto& node : graph.Nodes) {
        nodeMap[node.Id] = &node;
    }

    // Draw edges as lines with arrows
    for (const auto& edge : graph.Edges) {
        // Get source and target nodes
        auto sourceIt = nodeMap.find(edge.Source);
        auto targetIt = nodeMap.find(edge.Target);

        if (sourceIt == nodeMap.end() || targetIt == nodeMap.end()) {
            continue;  // Skip if we can't find either node
        }

        // Get node positions
        ImVec2 sourcePos = GetNodePosition(*sourceIt->second);
        ImVec2 targetPos = GetNodePosition(*targetIt->second);

        // Convert to canvas coordinates
        ImVec2 canvasSourcePos = GraphToCanvas(sourcePos, canvasPos);
        ImVec2 canvasTargetPos = GraphToCanvas(targetPos, canvasPos);

        // Calculate direction vector
        ImVec2 dir = ImVec2(canvasTargetPos.x - canvasSourcePos.x, canvasTargetPos.y - canvasSourcePos.y);
        float length = sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length < 1e-6f) continue; // Skip if nodes are at the same position

        // Normalize direction vector
        dir.x /= length;
        dir.y /= length;

        // Calculate source and target positions at the edge of each node rectangle
        float halfSrcWidth = NODE_WIDTH / 2.0f;
        float halfSrcHeight = NODE_HEIGHT / 2.0f;
        float halfTgtWidth = NODE_WIDTH / 2.0f;
        float halfTgtHeight = NODE_HEIGHT / 2.0f;

        // For source: move from center toward edge in the direction of the target
        ImVec2 sourceEdge;
        if (std::abs(dir.x * halfSrcHeight) > std::abs(dir.y * halfSrcWidth)) {
            // Intersect with left or right edge
            sourceEdge.x = canvasSourcePos.x + (dir.x > 0 ? halfSrcWidth : -halfSrcWidth);
            sourceEdge.y = canvasSourcePos.y + dir.y * (halfSrcWidth / std::abs(dir.x));
        }
        else {
            // Intersect with top or bottom edge
            sourceEdge.x = canvasSourcePos.x + dir.x * (halfSrcHeight / std::abs(dir.y));
            sourceEdge.y = canvasSourcePos.y + (dir.y > 0 ? halfSrcHeight : -halfSrcHeight);
        }

        // For target: move from center toward edge in the direction of the source
        ImVec2 targetEdge;
        if (std::abs(-dir.x * halfTgtHeight) > std::abs(-dir.y * halfTgtWidth)) {
            // Intersect with left or right edge
            targetEdge.x = canvasTargetPos.x + (-dir.x > 0 ? halfTgtWidth : -halfTgtWidth);
            targetEdge.y = canvasTargetPos.y + -dir.y * (halfTgtWidth / std::abs(dir.x));
        }
        else {
            // Intersect with top or bottom edge
            targetEdge.x = canvasTargetPos.x + -dir.x * (halfTgtHeight / std::abs(dir.y));
            targetEdge.y = canvasTargetPos.y + (-dir.y > 0 ? halfTgtHeight : -halfTgtHeight);
        }

        // Check if edge is bidirectional
        bool isBidirectional = false;

        // Direct access to IsBidirectional property in Conditions
        isBidirectional = edge.Conditions.IsBidirectional;

        // Use green color for bidirectional edges, otherwise use regular edge color
        ImU32 edgeColor = isBidirectional ? BIDIRECTIONAL_EDGE_COLOR : EDGE_COLOR;

        // Draw the edge as a directed arrow, passing isBidirectional flag
        DrawArrow(drawList, sourceEdge, targetEdge, edgeColor, EDGE_THICKNESS, isBidirectional);

        // Add edge label at midpoint
        ImVec2 midpoint = ImVec2((sourceEdge.x + targetEdge.x) * 0.5f, (sourceEdge.y + targetEdge.y) * 0.5f);
        ImVec2 labelSize = ImGui::CalcTextSize(edge.Label.c_str());

        // Draw text background for better visibility
        drawList->AddRectFilled(
            ImVec2(midpoint.x - labelSize.x / 2 - TEXT_PADDING, midpoint.y - labelSize.y / 2 - TEXT_PADDING),
            ImVec2(midpoint.x + labelSize.x / 2 + TEXT_PADDING, midpoint.y + labelSize.y / 2 + TEXT_PADDING),
            IM_COL32(40, 40, 40, 200),
            3.0f
        );

        drawList->AddText(
            ImVec2(midpoint.x - labelSize.x / 2, midpoint.y - labelSize.y / 2),
            IM_COL32(220, 220, 220, 255),
            edge.Label.c_str()
        );
    }
}


ImVec2 GraphVisualizer::GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const {
    // Convert from graph coordinates to canvas coordinates
    return ImVec2(
        canvasPos.x + (pos.x + m_panOffset.x) * m_zoomLevel,
        canvasPos.y + (pos.y + m_panOffset.y) * m_zoomLevel
    );
}

ImVec2 GraphVisualizer::CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const {
    // Convert from canvas coordinates to graph coordinates
    return ImVec2(
        (pos.x - canvasPos.x) / m_zoomLevel - m_panOffset.x,
        (pos.y - canvasPos.y) / m_zoomLevel - m_panOffset.y
    );
}

ImVec2 GraphVisualizer::GetNodePosition(const Node& node) const {
    // Get node position from its X and Y properties
    return ImVec2(static_cast<float>(node.X), static_cast<float>(node.Y));
}

void GraphVisualizer::SaveNodePosition(const std::string& nodeId, const ImVec2& newPos) {
    // Update graph with new node position and save to config
    auto graphOpt = m_configManager.GetGraph(m_activeGraph);
    if (!graphOpt.has_value()) return;

    Graph updatedGraph = graphOpt.value().get();

    for (Node& node : updatedGraph.Nodes) {
        if (node.Id == nodeId) {
            node.X = static_cast<int>(newPos.x);
            node.Y = static_cast<int>(newPos.y);
            break;
        }
    }

    // Update the graph in the config manager
    try {
        m_configManager.UpdateGraph(m_activeGraph, updatedGraph);
        m_configManager.SaveConfig();
    }
    catch (const std::exception& e) {
        m_logger->LogError("Failed to save node position: " + std::string(e.what()));
    }
}

void GraphVisualizer::DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, bool isBidirectional) {
    // Draw the main line
    drawList->AddLine(start, end, color, thickness);

    // Calculate arrowhead direction
    ImVec2 dir = ImVec2(end.x - start.x, end.y - start.y);
    float length = sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length < 1e-6f) return; // Skip if points are too close

    // Normalize direction
    dir.x /= length;
    dir.y /= length;

    // Calculate perpendicular vector
    ImVec2 perp = ImVec2(-dir.y, dir.x);

    // Calculate arrowhead points for end arrow
    ImVec2 endArrowP1 = ImVec2(
        end.x - dir.x * ARROW_SIZE + perp.x * ARROW_SIZE * 0.5f,
        end.y - dir.y * ARROW_SIZE + perp.y * ARROW_SIZE * 0.5f
    );

    ImVec2 endArrowP2 = ImVec2(
        end.x - dir.x * ARROW_SIZE - perp.x * ARROW_SIZE * 0.5f,
        end.y - dir.y * ARROW_SIZE - perp.y * ARROW_SIZE * 0.5f
    );

    // Draw arrowhead at end
    drawList->AddTriangleFilled(end, endArrowP1, endArrowP2, color);

    // If bidirectional, draw arrowhead at start as well
    if (isBidirectional) {
        ImVec2 startArrowP1 = ImVec2(
            start.x + dir.x * ARROW_SIZE + perp.x * ARROW_SIZE * 0.5f,
            start.y + dir.y * ARROW_SIZE + perp.y * ARROW_SIZE * 0.5f
        );

        ImVec2 startArrowP2 = ImVec2(
            start.x + dir.x * ARROW_SIZE - perp.x * ARROW_SIZE * 0.5f,
            start.y + dir.y * ARROW_SIZE - perp.y * ARROW_SIZE * 0.5f
        );

        // Draw arrowhead at start
        drawList->AddTriangleFilled(start, startArrowP1, startArrowP2, color);
    }
}



std::string GraphVisualizer::GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos) {
    auto graphOpt = m_configManager.GetGraph(m_activeGraph);
    if (!graphOpt.has_value()) return "";

    const auto& graph = graphOpt.value().get();

    // Convert mouse position to graph coordinates
    ImVec2 graphPos = CanvasToGraph(pos, canvasPos);

    // Check each node
    for (const auto& node : graph.Nodes) {
        ImVec2 nodePos = GetNodePosition(node);

        // Check if mouse position is inside the node rectangle
        if (graphPos.x >= nodePos.x - NODE_WIDTH / 2 / m_zoomLevel &&
            graphPos.x <= nodePos.x + NODE_WIDTH / 2 / m_zoomLevel &&
            graphPos.y >= nodePos.y - NODE_HEIGHT / 2 / m_zoomLevel &&
            graphPos.y <= nodePos.y + NODE_HEIGHT / 2 / m_zoomLevel) {
            return node.Id;
        }
    }

    return ""; // No node at this position
}