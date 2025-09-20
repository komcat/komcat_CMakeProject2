// ApplicationInitializer.h
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <filesystem>
#include "AppContext.h"

// Forward declarations
class Logger;
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
class CLD101xOperations;
class Keithley2400Manager;
class Keithley2400Operations;
class MotionControlLayer;
class MachineOperations;
class MotionOps;
class IOOps;
class VisionOps;
class ConfigFileWatchdog;
class DUTDataRecorder;
class GlobalDataStore;
class SPDPowerSupplyManager;
class VisionCameraExposureManager;



class ApplicationInitializer {
public:
  // Progress tracking structures
  struct InitStep {
    std::string name;
    std::string description;
    std::function<bool()> execute;
    bool required;  // If true, failure stops initialization
    bool completed = false;
    bool success = false;
    std::chrono::milliseconds duration{ 0 };
    std::string errorMessage;
  };

  struct InitProgress {
    int currentStep = 0;
    int totalSteps = 0;
    float percentage = 0.0f;
    std::string currentOperation;
    std::string status;
    bool hasError = false;
    std::string errorMessage;
    std::chrono::steady_clock::time_point startTime;
  };

  // Hardware container structure
  struct HardwareManagers {
    std::unique_ptr<MotionConfigManager> motionConfig;
    std::unique_ptr<PIControllerManager> piController;
    std::unique_ptr<ACSControllerManager> acsController;
    std::unique_ptr<EziIOManager> ioManager;
    std::unique_ptr<IOConfigManager> ioConfig;
    std::unique_ptr<PneumaticManager> pneumatic;
    std::unique_ptr<CameraManager> camera;
    std::unique_ptr<CameraConfigManager> cameraConfig;
    std::unique_ptr<DataClientManager> dataClient;
    std::unique_ptr<CLD101xManager> laser;
    std::unique_ptr<CLD101xOperations> laserOps;
    std::unique_ptr<Keithley2400Manager> keithley;
    std::unique_ptr<Keithley2400Operations> keithleyOps;
    std::unique_ptr<MotionControlLayer> motionControl;
    std::unique_ptr<DUTDataRecorder> dutRecorder;
    std::unique_ptr<ConfigFileWatchdog> configWatchdog;
    std::unique_ptr<SPDPowerSupplyManager> spdPowerSupply;
    std::unique_ptr<VisionCameraExposureManager> visionExposureManager;

  };

  struct Operations {
    std::unique_ptr<MachineOperations> machine;
    std::unique_ptr<MotionOps> motion;
    std::unique_ptr<IOOps> io;
    std::unique_ptr<VisionOps> vision;
  };

  using ProgressCallback = std::function<void(const InitProgress&)>;

private:
  Logger* logger;
  AppContext& context;
  std::vector<InitStep> initSteps;
  ProgressCallback progressCallback;
  InitProgress currentProgress;

  // Configuration storage
  std::vector<std::string> configFiles;
  std::vector<std::string> presetFiles;
  bool databaseAvailable = false;

public:
  ApplicationInitializer(Logger* log);
  ~ApplicationInitializer() = default;

  // Set callback for progress updates
  void SetProgressCallback(ProgressCallback callback);

  // Main initialization sequence
  void PrepareInitializationSteps(HardwareManagers& hw, Operations& ops);
  bool ExecuteInitialization(HardwareManagers& hw, Operations& ops);

  // Get detailed report
  std::string GetInitializationReport() const;
  std::vector<std::string> GetConfigFiles() const { return configFiles; }
  bool IsDatabaseAvailable() const { return databaseAvailable; }

private:
  // Progress management
  void UpdateProgress(const std::string& operation, float percentage);
  bool ExecuteStep(InitStep& step);

  // Individual initialization methods
  bool InitDatabase();
  bool LoadConfigurations();
  bool ScanConfigurationFiles();

  // Hardware initialization wrapped for steps
  std::function<bool()> WrapInitMotionControllers(HardwareManagers& hw);
  std::function<bool()> WrapInitIOSystems(HardwareManagers& hw);
  std::function<bool()> WrapInitCameras(HardwareManagers& hw);
  std::function<bool()> WrapInitInstruments(HardwareManagers& hw);
  std::function<bool()> WrapInitDataClients(HardwareManagers& hw);
  std::function<bool()> WrapCreateMotionControl(HardwareManagers& hw);
  std::function<bool()> WrapCreateMachineOps(HardwareManagers& hw, Operations& ops);
  std::function<bool()> WrapInitOperations(HardwareManagers& hw, Operations& ops);
  std::function<bool()> WrapRegisterWithContext(HardwareManagers& hw, Operations& ops);
  std::function<bool()> WrapInitConfigWatchdog(HardwareManagers& hw);
  std::function<bool()> WarpInitConfigSPDPowerSupply(HardwareManagers& hw);



  // Actual implementation methods
  bool InitMotionControllers(HardwareManagers& hw);
  bool InitIOSystems(HardwareManagers& hw);
  bool InitCameras(HardwareManagers& hw);
  bool InitInstruments(HardwareManagers& hw);
  bool InitDataClients(HardwareManagers& hw);
  bool CreateMotionControl(HardwareManagers& hw);
  bool CreateMachineOps(HardwareManagers& hw, Operations& ops);
  bool InitOperations(HardwareManagers& hw, Operations& ops);
  bool RegisterWithContext(HardwareManagers& hw, Operations& ops);
  bool InitConfigWatchdog(HardwareManagers& hw);
  bool InitConfigSPDPowerSupply(HardwareManagers& hw);
  


  // Helper methods
  void LogHardwareVelocities(PIControllerManager* pi, ACSControllerManager* acs);
  std::vector<std::string> ScanPresetsFolder(const std::string& folder);
};