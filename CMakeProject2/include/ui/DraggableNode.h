// DraggableNode.h
#pragma once

#include "imgui.h"
#include <string>

class DraggableNode {
public:
    DraggableNode();
    ~DraggableNode() = default;

    // Render the window with draggable node
    void RenderUI();

    // Show/hide the window
    void ToggleWindow() { m_showWindow = !m_showWindow; }
    bool IsVisible() const { return m_showWindow; }

private:
    // UI state
    bool m_showWindow = true;
    float m_zoomLevel = 1.0f;
    ImVec2 m_panOffset = ImVec2(0, 0);
    bool m_isWindowHovered = false;    // Track if our window is hovered
    bool m_isCanvasHovered = false;    // Track if our canvas is hovered

    // Node state
    ImVec2 m_nodePosition = ImVec2(200, 200); // Initial position
    bool m_isDragging = false;
    ImVec2 m_dragStartPos;
    ImVec2 m_lastMousePos;

    // Rendering constants
    const float NODE_WIDTH = 160.0f;
    const float NODE_HEIGHT = 80.0f;
    const float NODE_ROUNDING = 5.0f;
    const ImU32 NODE_COLOR = IM_COL32(70, 70, 200, 255);
    const ImU32 NODE_BORDER_COLOR = IM_COL32(255, 255, 255, 255);
    const ImU32 SELECTED_NODE_COLOR = IM_COL32(120, 120, 255, 255);
    const float TEXT_PADDING = 5.0f;

    // Helper functions
    void RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void RenderNode(ImDrawList* drawList, const ImVec2& canvasPos);
    void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);

    // Coordinate conversion helpers
    ImVec2 WorldToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const;
    ImVec2 CanvasToWorld(const ImVec2& pos, const ImVec2& canvasPos) const;
};