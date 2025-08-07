// UINodeListWindow_Proper.h - Following UIConfigVisualizer pattern exactly
#pragma once

#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include "include/motions/MotionTypes.h" // For Node structure

// Forward declarations
class MotionConfigManager;
class MachineOperations;
class PIControllerManager;
class ACSControllerManager;

/**
 * @brief Node List Window for navigation between different positions
 *
 * Updated to follow the EXACT same movement pattern as UIConfigVisualizer.
 * Uses MoveDeviceToNode and MoveToPointName methods with proper Node structure.
 */
class UINodeListWindow {
public:
  UINodeListWindow();
  ~UINodeListWindow();

  void RenderUI();
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

  // Component connections
  void SetMotionConfigManager(MotionConfigManager* configManager);
  void SetMachineOperations(MachineOperations* machineOps);
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);

private:
  // Node information structure - Updated to match UIConfigVisualizer pattern
  struct NodeInfo {
    std::string id;                           // Node.Id
    std::string name;                         // Node.Label or Node.Id
    std::string description;                  // Description for UI
    std::string category;                     // Graph name
    std::string graphName;                    // Graph this node belongs to
    std::string deviceName;                   // Node.Device
    std::string positionName;                 // Node.Position
    std::map<std::string, double> positions;  // Actual coordinates (x,y,z,u,v,w)
    bool isReachable = true;
    bool isCurrentPosition = false;
    double estimatedTime = 0.0;               // seconds to reach
  };

  // UI state
  bool m_showWindow = true;
  std::string m_filterText = "";
  std::string m_selectedCategory = "All";
  std::string m_selectedNodeId = "";

  // Component references
  MotionConfigManager* m_configManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;
  PIControllerManager* m_piManager = nullptr;
  ACSControllerManager* m_acsManager = nullptr;

  // Node data
  std::vector<NodeInfo> m_nodes;
  std::vector<std::string> m_categories;
  std::map<std::string, NodeInfo> m_nodeMap;

  // Movement state tracking
  bool m_isMoving = false;
  std::string m_movingToNode = "";
  float m_moveProgress = 0.0f;
  std::chrono::steady_clock::time_point m_moveStartTime;

  // UI settings
  bool m_showDetailsPanel = true;
  bool m_confirmBeforeMove = true;
  bool m_showOnlyReachable = false;

  // ============================================================================
  // CORE MOVEMENT METHODS - Following UIConfigVisualizer Pattern EXACTLY
  // ============================================================================

  /**
   * @brief Navigate to a specific node using UIConfigVisualizer pattern
   * @param nodeId The ID of the target node
   */
  void NavigateToNode(const std::string& nodeId);

  /**
   * @brief Execute movement using the exact same methods as UIConfigVisualizer
   * @param targetNode The Node structure from MotionConfigManager
   * @param graphName The graph containing this node
   */
  void ExecuteMovementToNode(const Node& targetNode, const std::string& graphName);

  /**
   * @brief Stop any ongoing movement
   */
  void StopMovement();

  /**
   * @brief Reset all movement-related state variables
   */
  void ResetMovementState();

  /**
   * @brief Check if navigation to a node is currently possible
   * @param nodeId The node to check
   * @return true if navigation is possible
   */
  bool CanNavigateToNode(const std::string& nodeId);

  // ============================================================================
  // NODE DATA MANAGEMENT - Updated for actual Node structure
  // ============================================================================

  void RefreshNodeList();
  void LoadNodesFromConfig();
  void UpdateNodeReachability();
  void UpdateCurrentPosition();
  void UpdateMovementProgress();
  double EstimateTimeToNode(const NodeInfo& nodeInfo);

  // ============================================================================
  // UI RENDERING METHODS
  // ============================================================================

  void RenderMainPanel();
  void RenderFilterControls();
  void RenderNodeList();
  void RenderNodeItem(const NodeInfo& node);
  void RenderDetailsPanel();
  void RenderNavigationControls();
  void RenderStatusBar();

  // ============================================================================
  // FILTERING AND SEARCH
  // ============================================================================

  std::vector<NodeInfo> GetFilteredNodes();
  bool NodeMatchesFilter(const NodeInfo& node);
  void UpdateCategories();

  // ============================================================================
  // HELPER METHODS
  // ============================================================================

  std::string FormatPosition(const std::map<std::string, double>& positions);
  std::string FormatTime(double seconds);
  ImVec4 GetNodeStatusColor(const NodeInfo& node);
  const char* GetNodeStatusIcon(const NodeInfo& node);
};