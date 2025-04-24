// DraggableNode.cpp
#include "DraggableNode.h"
#include <cmath>
#include <algorithm>

DraggableNode::DraggableNode() {
    // Initialize any needed resources here
}

void DraggableNode::RenderUI() {
    if (!m_showWindow) return;

    // Create our window with NoMove flag to prevent it from being moved
    ImGui::Begin("Draggable Node Demo", &m_showWindow, ImGuiWindowFlags_NoMove);

    // Display instructions and controls at the top
    ImGui::Text("Drag the node to move it around.");
    ImGui::Text("Use middle mouse button to pan, mouse wheel to zoom.");

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

    // Display current node position
    ImGui::Text("Node Position: X=%.1f, Y=%.1f", m_nodePosition.x, m_nodePosition.y);

    // Calculate canvas size based on remaining window space
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Ensure we have at least some space to draw
    if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
    if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

    // Create a bordered canvas area that will capture mouse input
    ImGui::BeginChildFrame(ImGui::GetID("CanvasFrame"), canvasSize,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

    // Get the start position of our canvas (for coordinate conversion)
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Track if canvas is hovered
    m_isCanvasHovered = ImGui::IsWindowHovered();

    // Get the draw list for custom rendering
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Handle mouse input
    HandleInput(canvasPos, canvasSize);

    // Render the background grid
    RenderBackground(drawList, canvasPos, canvasSize);

    // Render the node
    RenderNode(drawList, canvasPos);

    ImGui::EndChildFrame();  // Close the canvas frame
    ImGui::End();            // Close the main window
}

void DraggableNode::RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
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

void DraggableNode::RenderNode(ImDrawList* drawList, const ImVec2& canvasPos) {
    // Convert node position to canvas coordinates
    ImVec2 canvasNodePos = WorldToCanvas(m_nodePosition, canvasPos);

    // Calculate node rectangle positions
    ImVec2 nodeMin = ImVec2(canvasNodePos.x - NODE_WIDTH / 2, canvasNodePos.y - NODE_HEIGHT / 2);
    ImVec2 nodeMax = ImVec2(canvasNodePos.x + NODE_WIDTH / 2, canvasNodePos.y + NODE_HEIGHT / 2);

    // Determine node color based on whether it's being dragged
    ImU32 fillColor = m_isDragging ? SELECTED_NODE_COLOR : NODE_COLOR;

    // Draw node background rectangle
    drawList->AddRectFilled(nodeMin, nodeMax, fillColor, NODE_ROUNDING);
    drawList->AddRect(nodeMin, nodeMax, NODE_BORDER_COLOR, NODE_ROUNDING, 0, 1.5f);

    // Add node label
    std::string nodeLabel = "Draggable Node";
    ImVec2 textSize = ImGui::CalcTextSize(nodeLabel.c_str());
    drawList->AddText(
        ImVec2(canvasNodePos.x - textSize.x / 2, nodeMin.y + TEXT_PADDING),
        IM_COL32(255, 255, 255, 255),
        nodeLabel.c_str()
    );

    // Add position display inside the node
    std::string posInfo = "X: " + std::to_string(static_cast<int>(m_nodePosition.x)) +
        ", Y: " + std::to_string(static_cast<int>(m_nodePosition.y));
    ImVec2 posTextSize = ImGui::CalcTextSize(posInfo.c_str());
    drawList->AddText(
        ImVec2(canvasNodePos.x - posTextSize.x / 2, canvasNodePos.y - posTextSize.y / 2),
        IM_COL32(255, 255, 255, 255),
        posInfo.c_str()
    );
}

void DraggableNode::HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Only process input when our canvas is being hovered
    if (!m_isCanvasHovered && !m_isDragging) return;

    ImVec2 mousePos = ImGui::GetIO().MousePos;

    // Handle mouse wheel for zooming
    if (m_isCanvasHovered && ImGui::GetIO().MouseWheel != 0) {
        float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
        float prevZoom = m_zoomLevel;
        m_zoomLevel = std::max(0.3f, std::min(m_zoomLevel + zoomDelta, 3.0f));

        // Adjust pan to zoom toward the mouse position
        if (m_zoomLevel != prevZoom) {
            ImVec2 mouseWorldPos = CanvasToWorld(mousePos, canvasPos);
            float zoomRatio = m_zoomLevel / prevZoom;
            ImVec2 newMouseWorldPos = ImVec2(mouseWorldPos.x * zoomRatio, mouseWorldPos.y * zoomRatio);
            m_panOffset.x += (mouseWorldPos.x - newMouseWorldPos.x);
            m_panOffset.y += (mouseWorldPos.y - newMouseWorldPos.y);
        }
    }

    // Handle middle mouse button for panning
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
            m_panOffset.x += dragDelta.x / m_zoomLevel;
            m_panOffset.y += dragDelta.y / m_zoomLevel;
        }
    }

    // Check if mouse is over the node
    ImVec2 canvasNodePos = WorldToCanvas(m_nodePosition, canvasPos);
    bool isMouseOverNode = (
        mousePos.x >= canvasNodePos.x - NODE_WIDTH / 2 &&
        mousePos.x <= canvasNodePos.x + NODE_WIDTH / 2 &&
        mousePos.y >= canvasNodePos.y - NODE_HEIGHT / 2 &&
        mousePos.y <= canvasNodePos.y + NODE_HEIGHT / 2
        );

    // Handle left mouse button for node dragging
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isMouseOverNode) {
        // Start dragging
        m_isDragging = true;
        m_dragStartPos = mousePos;
        m_lastMousePos = mousePos;
    }
    else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
        // Continue dragging
        ImVec2 dragDelta = ImVec2(mousePos.x - m_lastMousePos.x, mousePos.y - m_lastMousePos.y);

        // Apply the delta to the node position
        m_nodePosition.x += dragDelta.x / m_zoomLevel;
        m_nodePosition.y += dragDelta.y / m_zoomLevel;

        m_lastMousePos = mousePos;
    }
    else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_isDragging) {
        // End dragging when mouse is released anywhere
        m_isDragging = false;
    }
}

ImVec2 DraggableNode::WorldToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const {
    // Convert from world coordinates to canvas coordinates
    return ImVec2(
        canvasPos.x + (pos.x + m_panOffset.x) * m_zoomLevel,
        canvasPos.y + (pos.y + m_panOffset.y) * m_zoomLevel
    );
}

ImVec2 DraggableNode::CanvasToWorld(const ImVec2& pos, const ImVec2& canvasPos) const {
    // Convert from canvas coordinates to world coordinates
    return ImVec2(
        (pos.x - canvasPos.x) / m_zoomLevel - m_panOffset.x,
        (pos.y - canvasPos.y) / m_zoomLevel - m_panOffset.y
    );
}