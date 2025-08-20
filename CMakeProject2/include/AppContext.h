#pragma once

#include <memory>
#include <functional>
#include "logger.h"
#include "../include/ops/vision_ops.h"  // Add actual header instead of forward declaration
#include "../include/ops/io_ops.h"  // Add actual header instead of forward declaration
#include "../include/ops/motion_ops.h"  // Add actual header instead of forward declaration
#include "../include/SMU/keithley2400_manager.h"
#include "../include/camera/CameraConfigManager.h"

// Forward declarations for all your managers/services
class MotionConfigManager;
class PIControllerManager;
class ACSControllerManager;
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

/**
 * Application Context - Centralized service container
 * This class holds all application services and provides global access
 */
class AppContext {
public:
  // === Core System Services ===
  // Note: Logger uses singleton pattern, so we don't store it here
  std::unique_ptr<MotionConfigManager> motionConfigManager;
  std::unique_ptr<ConfigFileWatchdog> configWatchdog;

  // === Hardware Managers ===
  std::unique_ptr<PIControllerManager> piControllerManager;
  std::unique_ptr<ACSControllerManager> acsControllerManager;
  std::unique_ptr<EziIOManager> ioManager;
  std::unique_ptr<IOConfigManager> ioConfigManager;
  std::unique_ptr<PneumaticManager> pneumaticManager;

  // === Vision System ===
  std::unique_ptr<CameraManager> cameraManager;
  std::unique_ptr<CameraConfigManager> cameraConfigManager;

  // === Data & Communication ===
  std::unique_ptr<DataClientManager> dataClientManager;
  std::unique_ptr<DatabaseManager> databaseManager;
  std::unique_ptr<OperationResultsManager> resultsManager;

  // === Equipment Managers ===
  std::unique_ptr<CLD101xManager> cld101xManager;
  std::unique_ptr<Keithley2400Manager> keithleyManager;

  // === High-Level Operations ===
  std::unique_ptr<MachineOperations> machineOperations;
  std::unique_ptr<MotionOps> motionOps;
  std::unique_ptr<IOOps> ioOps;
  std::unique_ptr<VisionOps> visionOps;

  // === Singleton Access ===
  static AppContext& GetInstance() {
    static AppContext instance;
    return instance;
  }

  // === Service Registration & Access ===

  // Register core services
  // Note: Logger is singleton, accessed via Logger::GetInstance()

  void RegisterMotionConfig(std::unique_ptr<MotionConfigManager> service) {
    motionConfigManager = std::move(service);
  }

  // NEW: Register existing services by raw pointer (for parallel migration)
  void RegisterMotionConfigPtr(MotionConfigManager* service) {
    // Store as non-owning pointer - main app keeps ownership
    motionConfigManager.reset(service);
    // TODO: Need to track that we don't own this for proper cleanup
  }

  // BETTER: Individual registration methods for existing services (easier to use)
  void RegisterExistingMotionConfig(MotionConfigManager* service) { m_motionConfigPtr = service; }
  void RegisterExistingPIController(PIControllerManager* service) { m_piControllerPtr = service; }
  void RegisterExistingACSController(ACSControllerManager* service) { m_acsControllerPtr = service; }
  void RegisterExistingIOManager(EziIOManager* service) { m_ioManagerPtr = service; }
  void RegisterExistingIOConfig(IOConfigManager* service) { m_ioConfigPtr = service; }
  void RegisterExistingPneumatic(PneumaticManager* service) { m_pneumaticPtr = service; }
  void RegisterExistingCameraManager(CameraManager* service) { m_cameraManagerPtr = service; }
  void RegisterExistingCameraConfig(CameraConfigManager* service) { m_cameraConfigPtr = service; }
  void RegisterExistingDataClient(DataClientManager* service) { m_dataClientPtr = service; }
  void RegisterExistingCLD101x(CLD101xManager* service) { m_cld101xPtr = service; }
  void RegisterExistingKeithley(Keithley2400Manager* service) { m_keithleyPtr = service; }
  void RegisterExistingMachineOps(MachineOperations* service) { m_machineOperationsPtr = service; }
  void RegisterExistingMotionOps(MotionOps* service) { m_motionOpsPtr = service; }
  void RegisterExistingIOOps(IOOps* service) { m_ioOpsPtr = service; }
  void RegisterExistingVisionOps(VisionOps* service) { m_visionOpsPtr = service; }

  // Register hardware managers
  void RegisterPIController(std::unique_ptr<PIControllerManager> service) {
    piControllerManager = std::move(service);
  }

  void RegisterACSController(std::unique_ptr<ACSControllerManager> service) {
    acsControllerManager = std::move(service);
  }

  void RegisterIOManager(std::unique_ptr<EziIOManager> service) {
    ioManager = std::move(service);
  }

  void RegisterIOConfig(std::unique_ptr<IOConfigManager> service) {
    ioConfigManager = std::move(service);
  }

  void RegisterPneumaticManager(std::unique_ptr<PneumaticManager> service) {
    pneumaticManager = std::move(service);
  }

  // Register vision services
  void RegisterCameraManager(std::unique_ptr<CameraManager> service) {
    cameraManager = std::move(service);
  }

  void RegisterCameraConfig(std::unique_ptr<CameraConfigManager> service) {
    cameraConfigManager = std::move(service);
  }

  // Register data services
  void RegisterDataClient(std::unique_ptr<DataClientManager> service) {
    dataClientManager = std::move(service);
  }

  void RegisterDatabase(std::unique_ptr<DatabaseManager> service) {
    databaseManager = std::move(service);
  }

  void RegisterResultsManager(std::unique_ptr<OperationResultsManager> service) {
    resultsManager = std::move(service);
  }

  // Register equipment managers
  void RegisterCLD101x(std::unique_ptr<CLD101xManager> service) {
    cld101xManager = std::move(service);
  }

  void RegisterKeithley(std::unique_ptr<Keithley2400Manager> service) {
    keithleyManager = std::move(service);
  }

  // Register high-level operations
  void RegisterMachineOperations(std::unique_ptr<MachineOperations> service) {
    machineOperations = std::move(service);
  }

  void RegisterMotionOps(std::unique_ptr<MotionOps> service) {
    motionOps = std::move(service);
  }

  void RegisterIOOps(std::unique_ptr<IOOps> service) {
    ioOps = std::move(service);
  }

  void RegisterVisionOps(std::unique_ptr<VisionOps> service) {
    visionOps = std::move(service);
  }

  void RegisterConfigWatchdog(std::unique_ptr<ConfigFileWatchdog> service) {
    configWatchdog = std::move(service);
  }

  // === Safe Access Methods ===

  // Core services
  Logger* GetLogger() const { return Logger::GetInstance(); }  // Access singleton
  MotionConfigManager* GetMotionConfig() const {
    return motionConfigManager ? motionConfigManager.get() : m_motionConfigPtr;
  }
  ConfigFileWatchdog* GetConfigWatchdog() const { return configWatchdog.get(); }

  // Hardware managers - check both owned and non-owned pointers
  PIControllerManager* GetPIController() const {
    return piControllerManager ? piControllerManager.get() : m_piControllerPtr;
  }
  ACSControllerManager* GetACSController() const {
    return acsControllerManager ? acsControllerManager.get() : m_acsControllerPtr;
  }
  EziIOManager* GetIOManager() const {
    return ioManager ? ioManager.get() : m_ioManagerPtr;
  }
  IOConfigManager* GetIOConfig() const {
    return ioConfigManager ? ioConfigManager.get() : m_ioConfigPtr;
  }
  PneumaticManager* GetPneumaticManager() const {
    return pneumaticManager ? pneumaticManager.get() : m_pneumaticPtr;
  }

  // Vision services
  CameraManager* GetCameraManager() const {
    return cameraManager ? cameraManager.get() : m_cameraManagerPtr;
  }
  CameraConfigManager* GetCameraConfig() const {
    return cameraConfigManager ? cameraConfigManager.get() : m_cameraConfigPtr;
  }

  // Data services
  DataClientManager* GetDataClient() const {
    return dataClientManager ? dataClientManager.get() : m_dataClientPtr;
  }
  DatabaseManager* GetDatabase() const { return databaseManager.get(); }
  OperationResultsManager* GetResultsManager() const { return resultsManager.get(); }

  // Equipment managers
  CLD101xManager* GetCLD101x() const {
    return cld101xManager ? cld101xManager.get() : m_cld101xPtr;
  }
  Keithley2400Manager* GetKeithley() const {
    return keithleyManager ? keithleyManager.get() : m_keithleyPtr;
  }

  // High-level operations
  MachineOperations* GetMachineOperations() const {
    return machineOperations ? machineOperations.get() : m_machineOperationsPtr;
  }
  MotionOps* GetMotionOps() const {
    return motionOps ? motionOps.get() : m_motionOpsPtr;
  }
  IOOps* GetIOOps() const {
    return ioOps ? ioOps.get() : m_ioOpsPtr;
  }
  VisionOps* GetVisionOps() const {
    return visionOps ? visionOps.get() : m_visionOpsPtr;
  }

  // === Service Status Check ===
  bool IsInitialized() const {
    return Logger::GetInstance() != nullptr && motionConfigManager != nullptr;
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
    log->LogInfo("  - MotionConfig: " + std::string(motionConfigManager ? "✓" : "✗"));
    log->LogInfo("  - ConfigWatchdog: " + std::string(configWatchdog ? "✓" : "✗"));

    log->LogInfo("Hardware Managers:");
    log->LogInfo("  - PI Controller: " + std::string(piControllerManager ? "✓" : "✗"));
    log->LogInfo("  - ACS Controller: " + std::string(acsControllerManager ? "✓" : "✗"));
    log->LogInfo("  - IO Manager: " + std::string(ioManager ? "✓" : "✗"));
    log->LogInfo("  - Pneumatic: " + std::string(pneumaticManager ? "✓" : "✗"));

    log->LogInfo("Vision System:");
    log->LogInfo("  - Camera Manager: " + std::string(cameraManager ? "✓" : "✗"));
    log->LogInfo("  - Camera Config: " + std::string(cameraConfigManager ? "✓" : "✗"));

    log->LogInfo("Data Services:");
    log->LogInfo("  - Data Client: " + std::string(dataClientManager ? "✓" : "✗"));
    log->LogInfo("  - Database: " + std::string(databaseManager ? "✓" : "✗"));

    log->LogInfo("Equipment Managers:");
    log->LogInfo("  - CLD101x: " + std::string(cld101xManager ? "✓" : "✗"));
    log->LogInfo("  - Keithley: " + std::string(keithleyManager ? "✓" : "✗"));

    log->LogInfo("High-Level Operations:");
    log->LogInfo("  - Machine Ops: " + std::string(machineOperations ? "✓" : "✗"));
    log->LogInfo("  - Motion Ops: " + std::string(motionOps ? "✓" : "✗"));
    log->LogInfo("  - IO Ops: " + std::string(ioOps ? "✓" : "✗"));
    log->LogInfo("  - Vision Ops: " + std::string(visionOps ? "✓" : "✗"));
  }

private:
  // Private constructor for singleton
  AppContext() = default;

  // Prevent copying
  AppContext(const AppContext&) = delete;
  AppContext& operator=(const AppContext&) = delete;

  // === Non-owning pointers for parallel migration ===
  // These allow registration of existing services without transferring ownership
  MotionConfigManager* m_motionConfigPtr = nullptr;
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
};