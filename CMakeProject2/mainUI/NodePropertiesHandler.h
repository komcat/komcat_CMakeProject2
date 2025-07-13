// NodePropertiesHandler.h
#pragma once

#include <string>
#include <memory>
#include "include/motions/MotionTypes.h"
#include "include/motions/MotionConfigManager.h"
#include "include/logger.h"

class NodePropertiesHandler {
public:
  NodePropertiesHandler(MotionConfigManager& configManager, Logger* logger);
  ~NodePropertiesHandler();

  // Show properties dialog for a specific node
  void ShowNodeProperties(const std::string& graphName, const std::string& nodeId);

  // Render the properties dialog (call this in main render loop)
  void RenderPropertiesDialog();

  // Check if dialog is currently open
  bool IsDialogOpen() const { return m_showDialog; }

private:
  MotionConfigManager& m_configManager;
  Logger* m_logger;

  // Dialog state
  bool m_showDialog = false;
  std::string m_currentGraphName;
  std::string m_currentNodeId;
  Node m_currentNode;
  PositionStruct m_currentPosition;
  bool m_positionFound = false;

  // Helper methods
  void LoadNodeData();
  void RenderNodeInfo();
  void RenderPositionInfo();
  void RenderDeviceInfo();
  void RenderGraphInfo();
};