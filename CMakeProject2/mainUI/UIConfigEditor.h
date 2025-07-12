#pragma once

#include <memory>
#include <string>
#include "include/motions/MotionConfigManager.h"
#include "include/motions/MotionTypes.h"
#include "include/logger.h"

class UIConfigEditor {
public:
  UIConfigEditor(MotionConfigManager& configManager);
  ~UIConfigEditor();

  // Disable copy/move to avoid issues with incomplete types
  UIConfigEditor(const UIConfigEditor&) = delete;
  UIConfigEditor& operator=(const UIConfigEditor&) = delete;
  UIConfigEditor(UIConfigEditor&&) = delete;
  UIConfigEditor& operator=(UIConfigEditor&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return showWindow; }

private:
  // Reference to the config manager and logger
  MotionConfigManager& configManager;
  Logger* m_logger;

  // UI state
  bool showWindow = true;
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

  // Clipboard position paste functionality
  bool m_showClipboardConfirmation = false;
  PositionStruct m_oldPosition;
  PositionStruct m_newPosition;

  // Graph editing
  std::string m_selectedGraph;
  std::string m_newGraphName;
  bool m_isAddingNewGraph = false;
  std::string m_deviceFilter = "";
  float m_middleColumnWidth = 200.0f;

  // Node editing
  std::string m_selectedNode;
  Node m_editingNode;
  bool m_isAddingNewNode = false;
  std::string m_newNodeId;
  std::string m_newNodeLabel;
  std::string m_newNodeDevice;
  std::string m_newNodePosition;

  // Edge editing
  std::string m_selectedEdge;
  Edge m_editingEdge;
  bool m_isAddingNewEdge = false;
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

  // UI rendering methods - replicating the original structure
  void RenderDevicesTab();
  void RenderPositionsTab();
  void RenderGraphsTab();
  void RenderSettingsTab();

  // Position-specific UI methods
  void RenderAddNewPositionUI();
  void RenderEditPositionUI();

  // Graph-specific rendering functions
  void RenderGraphList();
  void RenderNodeList();
  void RenderEdgeList();
  void RenderNodeDetails();
  void RenderEdgeDetails();

  // Clipboard functionality
  void ProcessClipboardData();
  void RenderClipboardConfirmationPopup();

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
  void UpdateGraph();
};