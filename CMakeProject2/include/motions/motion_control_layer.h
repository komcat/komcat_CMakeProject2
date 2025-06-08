// motion_control_layer.h
#pragma once

#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/logger.h"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <condition_variable>

// Forward declaration of the adapter class
class MotionControlHierarchicalAdapter;

// Class to handle path planning and execution
class MotionControlLayer {
public:
  MotionControlLayer(MotionConfigManager& configManager,
    PIControllerManager& piControllerManager,
    ACSControllerManager& acsControllerManager);
  ~MotionControlLayer();

  // Path planning and execution
  bool PlanPath(const std::string& graphName, const std::string& startNodeId, const std::string& endNodeId);
  bool ExecutePath(bool blocking = false);
  void CancelExecution();

  // Callbacks for completion notifications
  using CompletionCallback = std::function<void(bool)>;
  void SetPathCompletionCallback(CompletionCallback callback);
  void SetSequenceCompletionCallback(CompletionCallback callback);

  // Status information
  bool IsExecuting() const { return m_isExecuting; }
  std::string GetCurrentNodeId() const;
  std::string GetNextNodeId() const;
  double GetPathProgress() const; // 0.0 to 1.0

  // UI visibility control for integration with VerticalToolbarMenu
  bool IsVisible() const { return m_isWindowVisible; }
  void ToggleWindow() { m_isWindowVisible = !m_isWindowVisible; }
  void SetWindowVisible(bool visible) { m_isWindowVisible = visible; }

  // UI rendering
  void RenderUI();

  // New methods to get current position information
  std::string GetNodeIdFromCurrentPosition(const std::string& graphName, const std::string& deviceName) const;
  bool IsDeviceAtNode(const std::string& graphName, const std::string& nodeId, double tolerance = 0.01) const;
  bool GetDeviceCurrentNode(const std::string& graphName, const std::string& deviceName, std::string& nodeId) const;

  // Helper to get node by ID
  const Node* GetNodeById(const std::string& graphName, const std::string& nodeId) const;

  // Get the current position of a device
  bool GetCurrentPosition(const std::string& deviceName, PositionStruct& position) const;

  // Get a list of available devices
  std::vector<std::string> GetAvailableDevices() const;

  // Move a device to an absolute position (non-blocking if specified)
  bool MoveToPosition(const std::string& deviceName, const PositionStruct& position, bool blocking = true);

  // Move a device relative to its current position (non-blocking if specified)
  bool MoveRelative(const std::string& deviceName, const std::string& axis, double distance, bool blocking = true);
  // Get current position
  bool GetCurrentPosition(const std::string& deviceName, PositionStruct& currentPosition);

  // Helper to determine which controller manager to use for a device
  bool IsDevicePIController(const std::string& deviceName) const;
  // Set debug verbose mode
  void SetDebugVerbose(bool enabled) { m_enableDebug = enabled; }

  // Get current debug verbose state
  bool GetDebugVerbose() const { return m_enableDebug; }
  const MotionConfigManager& GetConfigManager() const {
    return m_configManager;
  }


  const PIControllerManager& GetPIControllerManager() const {
    return m_piControllerManager;
  }

  const ACSControllerManager& GetACSControllerManager() const {
    return m_acsControllerManager;
  }

  // Non-const versions if needed for operations that modify the managers
  PIControllerManager& GetPIControllerManager() {
    return m_piControllerManager;
  }

  ACSControllerManager& GetACSControllerManager() {
    return m_acsControllerManager;
  }


  // Position management methods for saving current positions to configuration
  bool SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName);
  bool UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName);
  bool SaveAllCurrentPositionsToConfig(const std::string& prefix = "current_");

  // Configuration backup and management
  bool BackupMotionConfig(const std::string& backupSuffix = "");
  bool SaveMotionConfig();
  bool ReloadMotionConfig();


private:
  // References to managers
  MotionConfigManager& m_configManager;
  PIControllerManager& m_piControllerManager;
  ACSControllerManager& m_acsControllerManager;
  Logger* m_logger;
  bool m_enableDebug = false;
  // Path execution thread
  std::thread m_executionThread;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::atomic<bool> m_isExecuting;
  std::atomic<bool> m_cancelRequested;
  std::atomic<bool> m_threadRunning;

  // Path data
  std::string m_currentGraphName;
  std::vector<std::reference_wrapper<const Node>> m_plannedPath;
  size_t m_currentNodeIndex;

  // Callbacks
  CompletionCallback m_pathCompletionCallback;
  CompletionCallback m_sequenceCompletionCallback;

  // UI state
  bool m_isWindowVisible = false;

  // Private methods
  void ExecutionThreadFunc();
  bool MoveToNode(const Node& node);
  bool ValidateNodeTransition(const Node& fromNode, const Node& toNode);

  // Helper functions to get node information
  std::string GetNodeLabel(const std::string& graphName, const std::string& nodeId) const;
  std::string GetNodeLabelAndId(const std::string& graphName, const std::string& nodeId) const;
  std::vector<std::pair<std::string, std::string>> GetAllNodesWithLabels(const std::string& graphName) const;

  // Polling helpers
  bool WaitForPositionReached(const std::string& deviceName, const std::string& positionName,
    const PositionStruct& targetPosition, double timeoutSeconds = 30.0);
  bool IsPositionReached(const std::string& deviceName, const PositionStruct& targetPosition,
    double tolerance = 0.01);

  // Current position tracking
  std::map<std::string, PositionStruct> m_deviceCurrentPositions;
  void UpdateDevicePosition(const std::string& deviceName);

  // Helper to compare positions
  bool IsPositionsEqual(const PositionStruct& pos1, const PositionStruct& pos2, double tolerance) const {
    // Check if all axes are within tolerance
    bool xOk = std::abs(pos1.x - pos2.x) <= tolerance;
    bool yOk = std::abs(pos1.y - pos2.y) <= tolerance;
    bool zOk = std::abs(pos1.z - pos2.z) <= tolerance;

    // For rotation axes, only check if they are used (non-zero target)
    bool uOk = (pos2.u == 0) || (std::abs(pos1.u - pos2.u) <= tolerance);
    bool vOk = (pos2.v == 0) || (std::abs(pos1.v - pos2.v) <= tolerance);
    bool wOk = (pos2.w == 0) || (std::abs(pos1.w - pos2.w) <= tolerance);

    return xOk && yOk && zOk && uOk && vOk && wOk;
  }

  // Friend declaration for the adapter class
  friend class MotionControlHierarchicalAdapter;




};