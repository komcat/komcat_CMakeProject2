// MotionConfigEditor.h
#pragma once

#include "MotionConfigManager.h"
#include "MotionTypes.h"
#include "include/logger.h"
#include <string>
#include <memory>
#include <map>
#include <vector>

class MotionConfigEditor {
public:
    MotionConfigEditor(MotionConfigManager& configManager);
    ~MotionConfigEditor() = default;

    // Render the ImGui editor interface
    void RenderUI();

    // Show/hide the editor window
    void ToggleWindow() { m_showWindow = !m_showWindow; }
    bool IsVisible() const { return m_showWindow; }

private:

    // Add to private section in MotionConfigEditor.h
    float m_middleColumnWidth = 200.0f;  // Default width

    // Reference to the config manager
    MotionConfigManager& m_configManager;
    Logger* m_logger;

    // UI state
    bool m_showWindow = true;
    bool m_showDevicesTab = true;
    bool m_showPositionsTab = false;
    bool m_showGraphsTab = false;
    bool m_showSettingsTab = false;

    // Device editing
    std::string m_selectedDevice;
    std::string m_newDeviceName;
    MotionDevice m_editingDevice;
    bool m_isAddingNewDevice = false;

    // Position editing
    std::string m_selectedPosition;
    std::string m_newPositionName;
    PositionStruct m_editingPosition;
    bool m_isAddingNewPosition = false;

    // Graph editing
    std::string m_selectedGraph;
    std::string m_newGraphName;
    bool m_isAddingNewGraph = false;

    // Node editing
    std::string m_selectedNode;
    Node m_editingNode;
    bool m_isAddingNewNode = false;

    // Edge editing
    std::string m_selectedEdge;
    Edge m_editingEdge;
    bool m_isAddingNewEdge = false;

    // Temporary strings for editing
    std::string m_newNodeId;
    std::string m_newNodeLabel;
    std::string m_newNodeDevice;
    std::string m_newNodePosition;

    std::string m_newEdgeId;
    std::string m_newEdgeSource;
    std::string m_newEdgeTarget;
    std::string m_newEdgeLabel;

    // Buffers for text input
    char m_ipAddressBuffer[64] = { 0 };
    char m_nodeIdBuffer[64] = { 0 };
    char m_nodeLabelBuffer[128] = { 0 };
    char m_nodeDeviceBuffer[64] = { 0 };
    char m_nodePositionBuffer[64] = { 0 };
    char m_edgeIdBuffer[64] = { 0 };
    char m_edgeSourceBuffer[64] = { 0 };
    char m_edgeTargetBuffer[64] = { 0 };
    char m_edgeLabelBuffer[128] = { 0 };


    // UI rendering functions
    void RenderDevicesTab();
    void RenderPositionsTab();
    void RenderGraphsTab();
    void RenderSettingsTab();

    // Graph-specific rendering functions
    void RenderGraphList();
    void RenderNodeList();
    void RenderEdgeList();
    void RenderNodeDetails();
    void RenderEdgeDetails();

    // Helper functions
    void RefreshDeviceData();
    void SaveChanges();
    void DeleteSelectedDevice();
    void DeleteSelectedPosition();
    void AddNewDevice();
    void AddNewPosition();

    // Graph helper functions
    void AddNewNode();
    void DeleteSelectedNode();
    void AddNewEdge();
    void DeleteSelectedEdge();
    void RefreshGraphData();

    // Updated graph in the config manager
    void UpdateGraph();
};