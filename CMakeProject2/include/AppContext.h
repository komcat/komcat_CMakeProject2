#pragma once

// === WINSOCK CONFLICT PREVENTION ===
// Include winsock headers in correct order to prevent conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <string>
#include "logger.h"
#include "include/data/DUTDataRecorder.h"
// === FORWARD DECLARATIONS ===
// Use forward declarations to minimize include dependencies
class MotionConfigManager;
class PIControllerManager;
class ACSControllerManager;
class MotionControlLayer;
class EziIOManager;
class IOConfigManager;
class PneumaticManager;
class CameraManager;
class CameraConfigManager;
class DataClientManager;
class CLD101xManager;
class Keithley2400Manager;
class MachineOperations;
class MotionOps;
class IOOps;
class VisionOps;
class DatabaseManager;
class OperationResultsManager;
class Logger;
class ConfigFileWatchdog;
class IOperations;
class DUTDataRecorder;
class Logger;

/**
 * Application Context - Centralized service container
 * This class holds all application services and provides global access
 */
class AppContext {
public:
	//Logger instance
  Logger* logger = nullptr;

  // === Core System Services ===
  MotionConfigManager* motionConfigManager = nullptr;
  ConfigFileWatchdog* configWatchdog = nullptr;

  // === Motion Control ===
  MotionControlLayer* motionControlLayer = nullptr;

  // === Hardware Managers ===
  PIControllerManager* piControllerManager = nullptr;
  ACSControllerManager* acsControllerManager = nullptr;
  EziIOManager* ioManager = nullptr;
  IOConfigManager* ioConfigManager = nullptr;
  PneumaticManager* pneumaticManager = nullptr;

  // === Vision System ===
  CameraManager* cameraManager = nullptr;
  CameraConfigManager* cameraConfigManager = nullptr;

  // === Data & Communication ===
  DataClientManager* dataClientManager = nullptr;
  DatabaseManager* databaseManager = nullptr;
  OperationResultsManager* resultsManager = nullptr;

  // === Equipment Managers ===
  CLD101xManager* cld101xManager = nullptr;
  Keithley2400Manager* keithleyManager = nullptr;

  // === High-Level Operations ===
  // Use raw pointers for ops classes to avoid incomplete type issues
  MachineOperations* machineOperations = nullptr;
  MotionOps* motionOps = nullptr;
  IOOps* ioOps = nullptr;
  VisionOps* visionOps = nullptr;

  // DUT
  DUTDataRecorder* dutDataRecorder = nullptr;

private:
  // === Operations Registry for Scripting Interface ===
  std::map<std::string, IOperations*> m_operationsRegistry;


	//logger instance
  Logger* m_logger = nullptr;
  // === Non-owning pointers for parallel migration ===
  MotionConfigManager* m_motionConfigPtr = nullptr;
  MotionControlLayer* m_motionControlLayerPtr = nullptr;
  DatabaseManager* m_databaseManagerPtr = nullptr;
  OperationResultsManager* m_resultsManagerPtr = nullptr;
  PIControllerManager* m_piControllerPtr = nullptr;
  ACSControllerManager* m_acsControllerPtr = nullptr;
  EziIOManager* m_ioManagerPtr = nullptr;
  IOConfigManager* m_ioConfigPtr = nullptr;
  PneumaticManager* m_pneumaticPtr = nullptr;
  CameraManager* m_cameraManagerPtr = nullptr;
  CameraConfigManager* m_cameraConfigPtr = nullptr;
  DataClientManager* m_dataClientPtr = nullptr;
  CLD101xManager* m_cld101xPtr = nullptr;
  Keithley2400Manager* m_keithleyPtr = nullptr;
  MachineOperations* m_machineOperationsPtr = nullptr;
  MotionOps* m_motionOpsPtr = nullptr;
  IOOps* m_ioOpsPtr = nullptr;
  VisionOps* m_visionOpsPtr = nullptr;
  DUTDataRecorder* m_dutDataRecorderPtr = nullptr;


public:
  // === Singleton Access ===
  static AppContext& GetInstance() {
    static AppContext instance;
    return instance;
  }

  void RegisterLogger(Logger* log) {
    logger = log;
    m_logger = log; // Also store in parallel pointer
    if (logger) {
      GetLogger()->LogInfo("AppContext: Logger registered");
    }
	}


  // === Database and Results Manager Registration ===
  void RegisterDatabaseManager(DatabaseManager* dbManager) {
    databaseManager = dbManager;
    m_databaseManagerPtr = dbManager;
    if (databaseManager) {
      GetLogger()->LogInfo("AppContext: DatabaseManager registered");
    }
  }

  void RegisterOperationResultsManager(OperationResultsManager* resManager) {
    resultsManager = resManager;
    m_resultsManagerPtr = resManager;
    if (resultsManager) {
      GetLogger()->LogInfo("AppContext: OperationResultsManager registered");
    }
  }

  // === Enhanced Operations Registration ===
  void RegisterMotionOps(MotionOps* ops) {
    motionOps = ops;
    if (motionOps) {
      GetLogger()->LogInfo("AppContext: MotionOps registered");
    }
  }

  void RegisterIOOps(IOOps* ops) {
    ioOps = ops;
    if (ioOps) {
      GetLogger()->LogInfo("AppContext: IOOps registered");
    }
  }

  void RegisterVisionOps(VisionOps* ops) {
    visionOps = ops;
    if (visionOps) {
      GetLogger()->LogInfo("AppContext: VisionOps registered");
    }
  }

  // === Operations Registry Access (for Future Scripting) ===
  std::vector<std::string> GetAvailableOperationTypes() const {
    std::vector<std::string> types;
    if (motionOps) types.push_back("Motion");
    if (ioOps) types.push_back("IO");
    if (visionOps) types.push_back("Vision");
    return types;
  }

  // === Service Registration & Access ===
  void RegisterMotionConfig(MotionConfigManager* service) {
    motionConfigManager = service;
  }
  // === NEW: Motion Control Layer Registration ===
  void RegisterExistingMotionControlLayer(MotionControlLayer* layer) {
    motionControlLayer = layer;
    m_motionControlLayerPtr = layer;  // Also store in parallel pointer
    if (motionControlLayer) {
      GetLogger()->LogInfo("AppContext: MotionControlLayer registered");
    }
  }

  // Individual registration methods for existing services
  void RegisterExistingMotionConfig(MotionConfigManager* service) {
    motionConfigManager = service;
    m_motionConfigPtr = service;
  }
  void RegisterExistingPIController(PIControllerManager* service) {
    piControllerManager = service;
    m_piControllerPtr = service;
  }
  void RegisterExistingACSController(ACSControllerManager* service) {
    acsControllerManager = service;
    m_acsControllerPtr = service;
  }
  void RegisterExistingDatabaseManager(DatabaseManager* service) {
    databaseManager = service;
    m_databaseManagerPtr = service;
  }
  void RegisterExistingOperationResultsManager(OperationResultsManager* service) {
    resultsManager = service;
    m_resultsManagerPtr = service;
  }
  void RegisterExistingIOManager(EziIOManager* service) {
    ioManager = service;
    m_ioManagerPtr = service;
  }
  void RegisterExistingIOConfig(IOConfigManager* service) {
    ioConfigManager = service;
    m_ioConfigPtr = service;
  }
  void RegisterExistingPneumatic(PneumaticManager* service) {
    pneumaticManager = service;
    m_pneumaticPtr = service;
  }
  void RegisterExistingCameraManager(CameraManager* service) {
    cameraManager = service;
    m_cameraManagerPtr = service;
  }
  void RegisterExistingCameraConfig(CameraConfigManager* service) {
    cameraConfigManager = service;
    m_cameraConfigPtr = service;
  }
  void RegisterExistingDataClient(DataClientManager* service) {
    dataClientManager = service;
    m_dataClientPtr = service;
  }
  void RegisterExistingCLD101x(CLD101xManager* service) {
    cld101xManager = service;
    m_cld101xPtr = service;
  }
  void RegisterExistingKeithley(Keithley2400Manager* service) {
    keithleyManager = service;
    m_keithleyPtr = service;
  }
  void RegisterExistingMachineOps(MachineOperations* service) {
    machineOperations = service;
    m_machineOperationsPtr = service;
  }
  void RegisterExistingMotionOps(MotionOps* service) {
    motionOps = service;
    m_motionOpsPtr = service;
  }
  void RegisterExistingIOOps(IOOps* service) {
    ioOps = service;
    m_ioOpsPtr = service;
  }
  void RegisterExistingVisionOps(VisionOps* service) {
    visionOps = service;
    m_visionOpsPtr = service;
  }

  // Register hardware managers
  void RegisterPIController(PIControllerManager* service) {
    piControllerManager = service;
  }

  void RegisterACSController(ACSControllerManager* service) {
    acsControllerManager = service;
  }

  void RegisterIOManager(EziIOManager* service) {
    ioManager = service;
  }

  void RegisterIOConfig(IOConfigManager* service) {
    ioConfigManager = service;
  }

  void RegisterPneumaticManager(PneumaticManager* service) {
    pneumaticManager = service;
  }

  // Register vision services
  void RegisterCameraManager(CameraManager* service) {
    cameraManager = service;
  }

  void RegisterCameraConfig(CameraConfigManager* service) {
    cameraConfigManager = service;
  }

  // Register data services
  void RegisterDataClient(DataClientManager* service) {
    dataClientManager = service;
  }

  void RegisterDatabase(DatabaseManager* service) {
    databaseManager = service;
  }

  void RegisterResultsManager(OperationResultsManager* service) {
    resultsManager = service;
  }

  // Register equipment managers
  void RegisterCLD101x(CLD101xManager* service) {
    cld101xManager = service;
  }

  void RegisterKeithley(Keithley2400Manager* service) {
    keithleyManager = service;
  }

  // Register high-level operations
  void RegisterMachineOperations(MachineOperations* service) {
    machineOperations = service;
  }

  void RegisterConfigWatchdog(ConfigFileWatchdog* service) {
    configWatchdog = service;
  }
  void RegisterExistingDUTDataRecorder(DUTDataRecorder* service) {
    dutDataRecorder = service;
    m_dutDataRecorderPtr = service;
    if (dutDataRecorder) {
      GetLogger()->LogInfo("AppContext: DUTDataRecorder registered");
    }
  }


  // === Safe Access Methods ===
  Logger* GetLogger() const {
    return logger ? logger : m_logger;
	}

  MotionConfigManager* GetMotionConfig() const {
    return motionConfigManager ? motionConfigManager : m_motionConfigPtr;
  }

  MotionControlLayer* GetMotionControlLayer() const {
    return motionControlLayer ? motionControlLayer : m_motionControlLayerPtr;
  }

  DatabaseManager* GetDatabaseManager() const {
    return databaseManager ? databaseManager : m_databaseManagerPtr;
  }

  OperationResultsManager* GetResultsManager() const {
    return resultsManager ? resultsManager : m_resultsManagerPtr;
  }

  // MOVED TO .CPP: Shared pointer getters for database services (for compatibility with MachineOperations)
  std::shared_ptr<DatabaseManager> GetDatabaseManagerShared() const;
  std::shared_ptr<OperationResultsManager> GetResultsManagerShared() const;

  ConfigFileWatchdog* GetConfigWatchdog() const { return configWatchdog; }

  // Hardware managers - check both owned and non-owned pointers
  PIControllerManager* GetPIController() const {
    return piControllerManager ? piControllerManager : m_piControllerPtr;
  }

  ACSControllerManager* GetACSController() const {
    return acsControllerManager ? acsControllerManager : m_acsControllerPtr;
  }

  EziIOManager* GetIOManager() const {
    return ioManager ? ioManager : m_ioManagerPtr;
  }

  IOConfigManager* GetIOConfig() const {
    return ioConfigManager ? ioConfigManager : m_ioConfigPtr;
  }

  PneumaticManager* GetPneumaticManager() const {
    return pneumaticManager ? pneumaticManager : m_pneumaticPtr;
  }

  // Vision services
  CameraManager* GetCameraManager() const {
    return cameraManager ? cameraManager : m_cameraManagerPtr;
  }

  CameraConfigManager* GetCameraConfig() const {
    return cameraConfigManager ? cameraConfigManager : m_cameraConfigPtr;
  }

  // Data services
  DataClientManager* GetDataClient() const {
    return dataClientManager ? dataClientManager : m_dataClientPtr;
  }

  DatabaseManager* GetDatabase() const { return databaseManager; }

  // Equipment managers
  CLD101xManager* GetCLD101x() const {
    return cld101xManager ? cld101xManager : m_cld101xPtr;
  }

  Keithley2400Manager* GetKeithley() const {
    return keithleyManager ? keithleyManager : m_keithleyPtr;
  }

  // High-level operations
  MachineOperations* GetMachineOperations() const {
    return machineOperations ? machineOperations : m_machineOperationsPtr;
  }

  MotionOps* GetMotionOps() const {
    return motionOps ? motionOps : m_motionOpsPtr;
  }

  IOOps* GetIOOps() const {
    return ioOps ? ioOps : m_ioOpsPtr;
  }

  VisionOps* GetVisionOps() const {
    return visionOps ? visionOps : m_visionOpsPtr;
  }

  // 5. Add safe access method (following your GetXXX pattern):
  DUTDataRecorder* GetDUTDataRecorder() const {
    return dutDataRecorder ? dutDataRecorder : m_dutDataRecorderPtr;
  }


  // === Service Status Check ===
  bool IsInitialized() const {
    return Logger::GetInstance() != nullptr && motionConfigManager != nullptr;
  }

  bool HasMotionControl() const {
    return motionControlLayer != nullptr &&
      piControllerManager != nullptr &&
      acsControllerManager != nullptr;
	}

  bool HasMotionConfig() const {
    return motionConfigManager != nullptr;
	}

  bool HasMachineOperations() const {
    return machineOperations != nullptr;
	}
  bool HasDUTDataRecorder() const {
    return dutDataRecorder != nullptr;
  }
  bool HasConfigWatchdog() const {
    return configWatchdog != nullptr;
	}
  bool HasCLD101xManagers() const {
    return cld101xManager != nullptr || keithleyManager != nullptr;
	}
  bool HasLogger() const {
    return logger != nullptr;
	}
  bool HasHardwareManagers() const {
    return piControllerManager != nullptr ||
      acsControllerManager != nullptr ||
      ioManager != nullptr ||
      pneumaticManager != nullptr;
  }

  bool HasVisionSystem() const {
    return cameraManager != nullptr;
  }

  bool HasDataServices() const {
    return dataClientManager != nullptr || databaseManager != nullptr;
  }

  // === Initialization Helper ===
  void LogInitializationStatus() const {
    auto* log = GetLogger();
    if (!log) return;

    log->LogInfo("=== AppContext Initialization Status ===");
    log->LogInfo("Core Services:");
    log->LogInfo("  - Logger: " + std::string(Logger::GetInstance() ? "✓" : "✗"));
    log->LogInfo("  - MotionConfig: " + std::string(GetMotionConfig() ? "✓" : "✗"));
    log->LogInfo("  - MotionControlLayer: " + std::string(GetMotionControlLayer() ? "✓" : "✗"));
    log->LogInfo("  - ConfigWatchdog: " + std::string(configWatchdog ? "✓" : "✗"));

    log->LogInfo("Hardware Managers:");
    log->LogInfo("  - PI Controller: " + std::string(GetPIController() ? "✓" : "✗"));
    log->LogInfo("  - ACS Controller: " + std::string(GetACSController() ? "✓" : "✗"));
    log->LogInfo("  - IO Manager: " + std::string(GetIOManager() ? "✓" : "✗"));
    log->LogInfo("  - Pneumatic: " + std::string(GetPneumaticManager() ? "✓" : "✗"));

    log->LogInfo("Vision System:");
    log->LogInfo("  - Camera Manager: " + std::string(GetCameraManager() ? "✓" : "✗"));
    log->LogInfo("  - Camera Config: " + std::string(GetCameraConfig() ? "✓" : "✗"));

    log->LogInfo("Data Services:");
    log->LogInfo("  - Data Client: " + std::string(GetDataClient() ? "✓" : "✗"));
    log->LogInfo("  - Database: " + std::string(GetDatabaseManager() ? "✓" : "✗"));
    log->LogInfo("  - Results Manager: " + std::string(GetResultsManager() ? "✓" : "✗"));

    log->LogInfo("Equipment Managers:");
    log->LogInfo("  - CLD101x: " + std::string(GetCLD101x() ? "✓" : "✗"));
    log->LogInfo("  - Keithley: " + std::string(GetKeithley() ? "✓" : "✗"));

    log->LogInfo("High-Level Operations:");
    log->LogInfo("  - Machine Ops: " + std::string(GetMachineOperations() ? "✓" : "✗"));
    log->LogInfo("  - Motion Ops: " + std::string(GetMotionOps() ? "✓" : "✗"));
    log->LogInfo("  - IO Ops: " + std::string(GetIOOps() ? "✓" : "✗"));
    log->LogInfo("  - Vision Ops: " + std::string(GetVisionOps() ? "✓" : "✗"));

    // ADD THIS SECTION after "High-Level Operations:" section:
    log->LogInfo("Data Recording:");
    log->LogInfo("  - DUT Data Recorder: " + std::string(GetDUTDataRecorder() ? "✓" : "✗"));


  }

  // === Enhanced Operations Status (Future) ===
  void LogOperationsStatus() const {
    auto* log = GetLogger();
    if (!log) return;

    log->LogInfo("=== Operations Status ===");

    auto types = GetAvailableOperationTypes();
    if (types.empty()) {
      log->LogInfo("No operations registered");
      return;
    }

    for (const auto& type : types) {
      log->LogInfo("  - " + type + " Ops: ✓ Registered");
    }

    log->LogInfo("Summary: " + std::to_string(types.size()) + " operations registered");
  }

private:
  // Private constructor for singleton
  AppContext() = default;

  // Prevent copying
  AppContext(const AppContext&) = delete;
  AppContext& operator=(const AppContext&) = delete;
};