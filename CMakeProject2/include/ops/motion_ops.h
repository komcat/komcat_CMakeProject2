// motion_ops.h
#pragma once

#include "include/motions/motion_control_layer.h"
#include "include/motions/pi_controller_manager.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include "include/data/DatabaseManager.h"
#include "include/data/OperationResultsManager.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <map>
#include <mutex>

class MotionOps {
public:
  MotionOps(
    MotionControlLayer& motionLayer,
    PIControllerManager& piControllerManager,
    std::shared_ptr<DatabaseManager> dbManager = nullptr,
    std::shared_ptr<OperationResultsManager> resultsManager = nullptr
  );
  ~MotionOps();

  // Motion control methods with caller context tracking
  bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
    const std::string& targetNodeId, bool blocking = true,
    const std::string& callerContext = "");

  bool MoveToPointName(const std::string& deviceName, const std::string& positionName,
    bool blocking = true, const std::string& callerContext = "");

  bool MoveRelative(const std::string& deviceName, const std::string& axis,
    double distance, bool blocking = true,
    const std::string& callerContext = "");

  bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
    const std::string& startNodeId, const std::string& endNodeId,
    bool blocking = true, const std::string& callerContext = "");

  // Speed control
  bool SetDeviceSpeed(const std::string& deviceName, double velocity,
    const std::string& callerContext = "");
  bool GetDeviceSpeed(const std::string& deviceName, double& speed,
    const std::string& callerContext = "");

  // ACS-specific buffer program methods
  bool acsc_RunBuffer(const std::string& deviceName, int bufferNumber,
    const std::string& labelName = "", const std::string& callerContext = "");
  bool acsc_StopBuffer(const std::string& deviceName, int bufferNumber,
    const std::string& callerContext = "");
  bool acsc_StopAllBuffers(const std::string& deviceName,
    const std::string& callerContext = "");
  bool acsc_IsBufferRunning(const std::string& deviceName, int bufferNumber);

  // Scanning methods
  bool PerformScan(const std::string& deviceName, const std::string& dataChannel,
    const std::vector<double>& stepSizes, int settlingTimeMs,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" },
    const std::string& callerContext = "");

  bool StartScan(const std::string& deviceName, const std::string& dataChannel,
    const std::vector<double>& stepSizes, int settlingTimeMs,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" },
    const std::string& callerContext = "");

  bool StopScan(const std::string& deviceName,
    const std::string& callerContext = "");

  // Scan status
  bool IsScanActive(const std::string& deviceName) const;
  double GetScanProgress(const std::string& deviceName) const;
  std::string GetScanStatus(const std::string& deviceName) const;
  bool GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const;

  // Device status methods
  bool IsDeviceConnected(const std::string& deviceName);
  bool IsDeviceMoving(const std::string& deviceName);
  bool IsDevicePIController(const std::string& deviceName) const;
  bool WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs);

  // Position methods
  std::string GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName);
  std::string GetDeviceCurrentPositionName(const std::string& deviceName);
  bool GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position);
  double GetDistanceBetweenPositions(const PositionStruct& pos1, const PositionStruct& pos2,
    bool includeRotation = false) const;

  // Position storage for process sequences
  bool CaptureCurrentPosition(const std::string& deviceName, const std::string& label);
  bool GetStoredPosition(const std::string& label, PositionStruct& position) const;
  std::vector<std::string> GetStoredPositionLabels(const std::string& deviceName = "") const;
  double CalculateDistanceFromStored(const std::string& deviceName, const std::string& storedLabel);
  bool HasMovedFromStored(const std::string& deviceName, const std::string& storedLabel,
    double tolerance = 0.001);
  void ClearStoredPositions(const std::string& deviceNameFilter = "");
  void ClearOldStoredPositions(int maxAgeMinutes = 60);
  bool GetStoredPositionInfo(const std::string& label, std::string& deviceName,
    std::chrono::steady_clock::time_point& timestamp) const;

  // Position monitoring and cache
  std::map<std::string, PositionStruct> GetCurrentPositions();
  bool UpdateAllCurrentPositions();
  const std::map<std::string, PositionStruct>& GetCachedPositions() const;
  void RefreshPositionCache();

  // Configuration management
  bool SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName);
  bool UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName);
  bool SaveAllCurrentPositionsToConfig(const std::string& prefix = "current_");
  bool ReloadMotionConfig();
  bool BackupMotionConfig(const std::string& backupSuffix = "");
  bool RestoreMotionConfigFromBackup(const std::string& backupSuffix);
  bool SaveCurrentPositionForNode(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId);

  // Scanner cleanup
  bool CleanupAllScanners();
  bool ResetScanState(const std::string& deviceName);
  bool SafelyCleanupScanner(const std::string& deviceName);

  // Access to underlying managers
  PIControllerManager* GetPIControllerManager() { return &m_piControllerManager; }
  MotionControlLayer* GetMotionControlLayer() { return &m_motionLayer; }
  ACSControllerManager* GetACSControllerManager() { return &m_motionLayer.GetACSControllerManager(); }

  // Logging methods
  void LogInfo(const std::string& message) const;
  void LogWarning(const std::string& message) const;
  void LogError(const std::string& message) const;

private:
  Logger* m_logger;
  bool m_enableDebug = false;

  // Core system references
  MotionControlLayer& m_motionLayer;
  PIControllerManager& m_piControllerManager;

  // Database and result tracking
  std::shared_ptr<DatabaseManager> m_dbManager;
  std::shared_ptr<OperationResultsManager> m_resultsManager;

  // Scanning support
  std::map<std::string, std::unique_ptr<ScanningAlgorithm>> m_activeScans;
  std::mutex m_scanMutex;

  // Scan status information (tracked per device)
  struct ScanInfo {
    std::atomic<bool> isActive{ false };
    std::atomic<double> progress{ 0.0 };
    std::string status;
    mutable std::mutex statusMutex;
    double peakValue{ 0.0 };
    PositionStruct peakPosition;
    mutable std::mutex peakMutex;
  };
  std::map<std::string, ScanInfo> m_scanInfo;

  // Position storage for process calculations
  struct StoredPositionInfo {
    std::string deviceName;
    PositionStruct position;
    std::chrono::steady_clock::time_point timestamp;

    StoredPositionInfo() = default;
    StoredPositionInfo(const std::string& device, const PositionStruct& pos)
      : deviceName(device), position(pos), timestamp(std::chrono::steady_clock::now()) {
    }
  };

  std::map<std::string, StoredPositionInfo> m_storedPositions;
  mutable std::mutex m_positionStorageMutex;

  // Current position cache for all controllers
  std::map<std::string, PositionStruct> m_currentPositions;
  mutable std::mutex m_currentPositionsMutex;
  std::chrono::steady_clock::time_point m_lastPositionUpdate;
  static constexpr std::chrono::milliseconds POSITION_CACHE_TIMEOUT{ 100 };

  // Helper methods
  void StorePositionResult(const std::string& operationId, const std::string& prefix,
    const PositionStruct& position);
};