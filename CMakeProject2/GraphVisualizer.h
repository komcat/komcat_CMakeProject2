// GraphVisualizer.h
#pragma once

#include "MotionConfigManager.h"
#include "MotionTypes.h"
#include "logger.h"
#include "imgui.h"
#include <string>
#include <memory>
#include <map>
#include <vector>

class GraphVisualizer {
public:
    GraphVisualizer(MotionConfigManager& configManager);
    ~GraphVisualizer() = default;

    // Render the graph visualization
    void RenderUI();

    // Show/hide the visualizer window
    void ToggleWindow() { m_showWindow = !m_showWindow; }
    bool IsVisible() const { return m_showWindow; }

    // Set the active graph to visualize
    void SetActiveGraph(const std::string& graphName);

private:
    // Reference to the config manager
    MotionConfigManager& m_configManager;
    Logger* m_logger;

    // UI state
    bool m_showWindow = false;
    std::string m_activeGraph;
    float m_zoomLevel = 1.0f;
    ImVec2 m_panOffset = ImVec2(0, 0);
    bool m_isCanvasHovered = false;  // Track if canvas is hovered

    // Node dragging state
    bool m_isDragging = false;
    std::string m_draggedNodeId;
    ImVec2 m_dragStartPos;
    ImVec2 m_lastMousePos;

    // Rendering constants
    const float NODE_WIDTH = 160.0f;
    const float NODE_HEIGHT = 80.0f;
    const float NODE_ROUNDING = 5.0f;
    const ImU32 NODE_COLOR = IM_COL32(70, 70, 200, 255);
    const ImU32 NODE_BORDER_COLOR = IM_COL32(255, 255, 255, 255);
    const ImU32 SELECTED_NODE_COLOR = IM_COL32(120, 120, 255, 255);
    const ImU32 EDGE_COLOR = IM_COL32(200, 200, 200, 255);
    const ImU32 EDGE_HOVER_COLOR = IM_COL32(250, 250, 100, 255);
    const ImU32 BIDIRECTIONAL_EDGE_COLOR = IM_COL32(50, 205, 50, 255); // Green color for bidirectional edges
    const ImU32 ARROW_COLOR = IM_COL32(220, 220, 220, 255);
    const float ARROW_SIZE = 10.0f;
    const float EDGE_THICKNESS = 2.0f;
    const float TEXT_PADDING = 5.0f;

    // Helper functions
    void RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos);
    void RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos);
    void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);

    // Coordinate conversion helpers
    ImVec2 GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const;
    ImVec2 CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const;

    // Node position helpers
    ImVec2 GetNodePosition(const Node& node) const;
    void SaveNodePosition(const std::string& nodeId, const ImVec2& newPos);

    // Draw arrow for an edge
    void DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, bool isBidirectional = false);

    // Hit testing
    std::string GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos);
};