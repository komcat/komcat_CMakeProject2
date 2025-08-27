// ApplicationInitializer.cpp
#include "ApplicationInitializer.h"
#include "logger.h"
#include "ConfigDatabaseUtils.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/motion_control_layer.h"
#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "IOConfigManager.h"
#include "include/camera/CameraManager.h"
#include "CameraConfigManager.h"
#include "include/data/data_client_manager.h"
#include "include/data/global_data_store.h"
#include "include/data/DUTDataRecorder.h"
#include "include/cld101x/cld101x_manager.h"
#include "include/cld101x/cld101x_operations.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_operations.h"
#include "include/PowerSupply/SPDPowerSupplyManager.h"
#include "include/machine_operations.h"
#include "include/ops/motion_ops.h"
#include "include/ops/io_ops.h"
#include "include/ops/vision_ops.h"
#include "ConfigFileWatchdog.h"
#include <iostream>
#include <sstream>
#include <iomanip>

ApplicationInitializer::ApplicationInitializer(Logger* log)
  : logger(log), context(AppContext::GetInstance()) {
  logger->LogInfo("ApplicationInitializer created");
  currentProgress.startTime = std::chrono::steady_clock::now();
}

void ApplicationInitializer::SetProgressCallback(ProgressCallback callback) {
  progressCallback = callback;
}

void ApplicationInitializer::PrepareInitializationSteps(HardwareManagers& hw, Operations& ops) {
  initSteps.clear();

  // Build initialization sequence
  initSteps = {
    // Phase 1: Configuration
    {"Database", "Initializing database system",
     [this]() { return InitDatabase(); }, false},  // Not required

    {"Config Scan", "Scanning configuration files",
     [this]() { return ScanConfigurationFiles(); }, true},

    {"Config Load", "Loading configurations",
     [this]() { return LoadConfigurations(); }, true},

     // Phase 2: Core Hardware
     {"Motion Controllers", "Connecting to motion controllers (PI & ACS)",
      WrapInitMotionControllers(hw), true},

     {"IO Systems", "Initializing IO and pneumatic systems",
      WrapInitIOSystems(hw), true},

      // Phase 3: Optional Hardware
      {"Cameras", "Connecting to camera systems",
       WrapInitCameras(hw), false},  // Optional

      {"Instruments", "Connecting to laser and SMU",
       WrapInitInstruments(hw), false},  // Optional

      {"Data Clients", "Initializing TCP data clients",
       WrapInitDataClients(hw), false},  // Optional

       // Phase 4: High-Level Components
       {"Motion Control", "Creating motion control layer",
        WrapCreateMotionControl(hw), true},

       {"Machine Ops", "Creating machine operations",
        WrapCreateMachineOps(hw, ops), true},

       {"Operations", "Initializing operation managers",
        WrapInitOperations(hw, ops), true},

        // Phase 5: Final Setup
        {"AppContext", "Registering services with AppContext",
         WrapRegisterWithContext(hw, ops), true},

        {"Config Watchdog", "Starting configuration file monitor",
         WrapInitConfigWatchdog(hw), false},  // Optional
  };

  currentProgress.totalSteps = static_cast<int>(initSteps.size());
}

bool ApplicationInitializer::ExecuteInitialization(HardwareManagers& hw, Operations& ops) {
  auto startTime = std::chrono::steady_clock::now();

  logger->LogInfo("=== SYSTEM INITIALIZATION STARTING ===");
  logger->LogInfo("Total initialization steps: " + std::to_string(initSteps.size()));
  UpdateProgress("Starting initialization...", 0.0f);

  for (size_t i = 0; i < initSteps.size(); ++i) {
    auto& step = initSteps[i];
    currentProgress.currentStep = static_cast<int>(i + 1);

    // Calculate percentage
    float percentage = (float)(i) / initSteps.size() * 100.0f;

    // Update progress
    std::string progressMsg = "[" + std::to_string(i + 1) + "/" +
      std::to_string(initSteps.size()) + "] " +
      step.description;
    UpdateProgress(progressMsg, percentage);

    // Log step start
    logger->LogInfo("");
    logger->LogInfo(">>> STEP " + std::to_string(i + 1) + ": " + step.name);
    logger->LogInfo("    " + step.description);

    // Execute step
    bool success = ExecuteStep(step);

    // Handle failure
    if (!success && step.required) {
      currentProgress.hasError = true;
      currentProgress.errorMessage = step.name + " failed: " + step.errorMessage;
      UpdateProgress("FAILED: " + step.name, percentage);

      logger->LogError("=============================================");
      logger->LogError("INITIALIZATION FAILED AT STEP: " + step.name);
      logger->LogError("Error: " + step.errorMessage);
      logger->LogError("This is a required component - cannot continue");
      logger->LogError("=============================================");
      return false;
    }

    // Log result
    if (success) {
      logger->LogInfo("    ✅ " + step.name + " completed successfully (" +
        std::to_string(step.duration.count()) + "ms)");
    }
    else {
      logger->LogWarning("    ⚠️ " + step.name + " skipped (optional component)");
    }
  }

  // Calculate total time
  auto endTime = std::chrono::steady_clock::now();
  auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
    endTime - startTime);

  UpdateProgress("Initialization complete!", 100.0f);

  // Log summary
  logger->LogInfo("");
  logger->LogInfo("===========================================");
  logger->LogInfo("=== INITIALIZATION COMPLETE ===");
  logger->LogInfo("Total time: " + std::to_string(totalTime.count()) + "ms");

  // Count successful steps
  int successCount = 0;
  int optionalSkipped = 0;
  for (const auto& step : initSteps) {
    if (step.success) successCount++;
    else if (!step.required) optionalSkipped++;
  }

  logger->LogInfo("Successful: " + std::to_string(successCount) + "/" +
    std::to_string(initSteps.size()));
  if (optionalSkipped > 0) {
    logger->LogInfo("Optional components skipped: " + std::to_string(optionalSkipped));
  }
  logger->LogInfo("===========================================");

  return true;
}

bool ApplicationInitializer::ExecuteStep(InitStep& step) {
  auto startTime = std::chrono::steady_clock::now();

  try {
    step.success = step.execute();
    step.completed = true;

    if (!step.success && !step.errorMessage.empty()) {
      logger->LogDebug("Step failed with message: " + step.errorMessage);
    }
  }
  catch (const std::exception& e) {
    step.success = false;
    step.errorMessage = e.what();
    logger->LogError("Exception in " + step.name + ": " + e.what());
  }
  catch (...) {
    step.success = false;
    step.errorMessage = "Unknown exception";
    logger->LogError("Unknown exception in " + step.name);
  }

  auto endTime = std::chrono::steady_clock::now();
  step.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    endTime - startTime);

  return step.success;
}

void ApplicationInitializer::UpdateProgress(const std::string& operation, float percentage) {
  currentProgress.currentOperation = operation;
  currentProgress.percentage = percentage;
  currentProgress.status = operation;

  if (progressCallback) {
    progressCallback(currentProgress);
  }
}

// =======================
// Wrapper Functions
// =======================

std::function<bool()> ApplicationInitializer::WrapInitMotionControllers(HardwareManagers& hw) {
  return [this, &hw]() { return InitMotionControllers(hw); };
}

std::function<bool()> ApplicationInitializer::WrapInitIOSystems(HardwareManagers& hw) {
  return [this, &hw]() { return InitIOSystems(hw); };
}

std::function<bool()> ApplicationInitializer::WrapInitCameras(HardwareManagers& hw) {
  return [this, &hw]() { return InitCameras(hw); };
}

std::function<bool()> ApplicationInitializer::WrapInitInstruments(HardwareManagers& hw) {
  return [this, &hw]() { return InitInstruments(hw); };
}

std::function<bool()> ApplicationInitializer::WrapInitDataClients(HardwareManagers& hw) {
  return [this, &hw]() { return InitDataClients(hw); };
}

std::function<bool()> ApplicationInitializer::WrapCreateMotionControl(HardwareManagers& hw) {
  return [this, &hw]() { return CreateMotionControl(hw); };
}

std::function<bool()> ApplicationInitializer::WrapCreateMachineOps(HardwareManagers& hw, Operations& ops) {
  return [this, &hw, &ops]() { return CreateMachineOps(hw, ops); };
}

std::function<bool()> ApplicationInitializer::WrapInitOperations(HardwareManagers& hw, Operations& ops) {
  return [this, &hw, &ops]() { return InitOperations(hw, ops); };
}

std::function<bool()> ApplicationInitializer::WrapRegisterWithContext(HardwareManagers& hw, Operations& ops) {
  return [this, &hw, &ops]() { return RegisterWithContext(hw, ops); };
}

std::function<bool()> ApplicationInitializer::WrapInitConfigWatchdog(HardwareManagers& hw) {
  return [this, &hw]() { return InitConfigWatchdog(hw); };
}

std::function<bool()> ApplicationInitializer::WarpInitConfigSPDPowerSupply(HardwareManagers& hw) {
  return [this, &hw]() { return InitConfigSPDPowerSupply(hw); };
}

// =======================
// Implementation Methods
// =======================

bool ApplicationInitializer::InitDatabase() {
  logger->LogInfo("Initializing database with migration check...");

  // Initialize database
  if (!ConfigDatabaseUtils::InitializeDatabase("", logger)) {
    logger->LogWarning("Database initialization failed - will use file-based configs");
    databaseAvailable = false;
    return false;
  }

  // Check for migration needs
  UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
  auto tables = dbManager.GetAllConfigTables();
  bool needsMigration = false;

  for (const auto& table : tables) {
    if (dbManager.IsOldFormatTable(table.tableName)) {
      needsMigration = true;
      break;
    }
  }

  if (needsMigration) {
    logger->LogInfo("Database migration needed - converting to simplified format");

    // Create backup
    UAA3::DatabaseResult backupResult = dbManager.CreateBackup();
    if (backupResult.success) {
      logger->LogInfo("Backup created: " + backupResult.details);
    }

    // Migrate
    auto migrationResults = dbManager.MigrateAllTablesToNewFormat();
    int successCount = 0;
    for (const auto& result : migrationResults) {
      if (result.success) successCount++;
    }

    logger->LogInfo("Migration completed: " + std::to_string(successCount) +
      " tables migrated");
  }

  databaseAvailable = true;
  logger->LogInfo("✅ Database configuration system ready");
  return true;
}

bool ApplicationInitializer::ScanConfigurationFiles() {
  logger->LogInfo("Scanning for configuration files...");

  // System configuration files
  configFiles = {
      "camera_calibration.json",
      "camera_exposure_config.json",
      "camera_to_object_offset.json",
      "data_display_config.json",
      "DataServerConfig.json",
      "io_panel_config.json",
      "IOConfig.json",
      "motion_config.json",
      "program.json",
      "script_runner_config.json",
      "smu_config.json",
      "toolbar_state.json",
      "transformation_matrix.json",
      "camera_config.json",
  };

  // Scan presets folder
  presetFiles = ScanPresetsFolder("presets");

  // Combine all files
  configFiles.insert(configFiles.end(), presetFiles.begin(), presetFiles.end());

  logger->LogInfo("Found " + std::to_string(configFiles.size()) +
    " configuration files (" + std::to_string(presetFiles.size()) +
    " presets)");
  return true;
}

bool ApplicationInitializer::LoadConfigurations() {
  if (!databaseAvailable) {
    logger->LogInfo("Loading configurations from files (database not available)");
    return true;  // File-based configs will be loaded by individual components
  }

  logger->LogInfo("Loading configurations with database integration...");

  std::vector<ConfigDatabaseUtils::ConfigLoadResult> configResults =
    ConfigDatabaseUtils::LoadMultipleConfigs(configFiles, logger);

  // Count results
  int fromDatabase = 0, fromFile = 0, savedToDb = 0, failed = 0;
  for (const auto& result : configResults) {
    if (result.success) {
      if (result.loadedFromDatabase) fromDatabase++;
      if (result.source == "file") fromFile++;
      if (result.savedToDatabase) savedToDb++;
    }
    else {
      failed++;
    }
  }

  logger->LogInfo("Configuration loading summary:");
  logger->LogInfo("  From database: " + std::to_string(fromDatabase));
  logger->LogInfo("  From files: " + std::to_string(fromFile));
  logger->LogInfo("  Saved to database: " + std::to_string(savedToDb));
  if (failed > 0) {
    logger->LogWarning("  Failed: " + std::to_string(failed));
  }

  return failed == 0 || failed < configFiles.size() / 2;  // Allow some failures
}

bool ApplicationInitializer::InitMotionControllers(HardwareManagers& hw) {
  // Create MotionConfigManager
  try {
    hw.motionConfig = std::make_unique<MotionConfigManager>("motion_config.json");
    logger->LogInfo("MotionConfigManager created successfully");
  }
  catch (const std::exception& e) {
    logger->LogError("Failed to create MotionConfigManager: " + std::string(e.what()));
    return false;
  }

  // Initialize PI Controller
  hw.piController = std::make_unique<PIControllerManager>(*hw.motionConfig);
  if (hw.piController->ConnectAll()) {
    logger->LogInfo("Connected to all enabled PI controllers");
  }
  else {
    logger->LogWarning("Failed to connect to some PI controllers");
  }

  // Initialize ACS Controller
  hw.acsController = std::make_unique<ACSControllerManager>(*hw.motionConfig);
  if (hw.acsController->ConnectAll()) {
    logger->LogInfo("Connected to all enabled ACS controllers");
  }
  else {
    logger->LogWarning("Failed to connect to some ACS controllers");
  }

  // Log velocities
  LogHardwareVelocities(hw.piController.get(), hw.acsController.get());

  return true;
}


bool ApplicationInitializer::InitIOSystems(HardwareManagers& hw) {
  // Initialize EziIO
  hw.ioManager = std::make_unique<EziIOManager>();
  if (!hw.ioManager->initialize()) {
    logger->LogError("Failed to initialize EziIO manager");
    return false;
  }

  // Load IO configuration
  hw.ioConfig = std::make_unique<IOConfigManager>();
  if (!hw.ioConfig->loadConfig("IOConfig.json")) {
    logger->LogWarning("Failed to load IO configuration, using defaults");
  }

  hw.ioConfig->initializeIOManager(*hw.ioManager);

  if (!hw.ioManager->connectAll()) {
    logger->LogWarning("Failed to connect to all IO devices");
  }

  hw.ioManager->startPolling(100);
  logger->LogInfo("EziIO system initialized");

  // Initialize Pneumatic System
  hw.pneumatic = std::make_unique<PneumaticManager>(*hw.ioManager);

  // IMPORTANT: Configure BEFORE initialize!
  if (!hw.ioConfig->initializePneumaticManager(*hw.pneumatic)) {
    logger->LogWarning("Failed to configure pneumatic manager");
    return false;
  }

  // NOW initialize after configuration is loaded
  if (!hw.pneumatic->initialize()) {
    logger->LogWarning("Failed to initialize Pneumatic Manager");
    return false;
  }

  hw.pneumatic->startPolling(100);

  // Set up pneumatic callbacks
  hw.pneumatic->setStateChangeCallback([this](const std::string& slideName, SlideState state) {
    std::string stateStr;
    switch (state) {
    case SlideState::EXTENDED: stateStr = "Extended"; break;
    case SlideState::RETRACTED: stateStr = "Retracted"; break;
    case SlideState::MOVING: stateStr = "Moving"; break;
    case SlideState::P_ERROR: stateStr = "ERROR"; break;
    default: stateStr = "Unknown";
    }
    logger->LogInfo("Pneumatic '" + slideName + "' -> " + stateStr);
  });

  logger->LogInfo("Pneumatic system initialized");
  return true;
}


bool ApplicationInitializer::InitCameras(HardwareManagers& hw) {
  hw.camera = std::make_unique<CameraManager>();

  try {
    hw.cameraConfig = std::make_unique<CameraConfigManager>("camera_config.json");
    hw.cameraConfig->SetLogger(logger);

    if (hw.cameraConfig->LoadConfig()) {
      logger->LogInfo("Camera configuration loaded successfully");

      if (hw.cameraConfig->InitializeCameraManager(*hw.camera)) {
        logger->LogInfo("CameraManager initialized from configuration");

        auto enabledCameras = hw.cameraConfig->GetEnabledCameraIds();
        logger->LogInfo("Enabled cameras: " + std::to_string(enabledCameras.size()));
        for (const auto& cameraId : enabledCameras) {
          logger->LogInfo("  - " + cameraId);
        }
        return true;
      }
    }
  }
  catch (const std::exception& e) {
    logger->LogError("Exception during camera initialization: " + std::string(e.what()));
  }

  // Fallback
  logger->LogInfo("Using fallback camera configuration");
  auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Top view camera");
  hw.camera->AddCamera(camera1);
  hw.camera->InitializeAllCameras();

  return true;
}

bool ApplicationInitializer::InitInstruments(HardwareManagers& hw) {
  bool anySuccess = false;

  // Initialize CLD101x (Laser)
  hw.laser = std::make_unique<CLD101xManager>();
  if (hw.laser) {
    logger->LogInfo("Initializing CLD101x laser controller...");
    hw.laser->EnableGlobalDataStoreForAll(true);

    bool laserAdded = hw.laser->AddClient("CLD101x", "127.0.0.88", 65432);
    logger->LogInfo("Laser client added: " + std::string(laserAdded ? "SUCCESS" : "FAILED"));

    if (hw.laser->ConnectAll()) {
      logger->LogInfo("Connected to CLD101x laser controller");
      hw.laserOps = std::make_unique<CLD101xOperations>(*hw.laser);
      anySuccess = true;
    }
    else {
      logger->LogWarning("Could not connect to CLD101x - laser operations disabled");
    }
  }

  // Initialize Keithley 2400 (SMU)
  hw.keithley = std::make_unique<Keithley2400Manager>();
  if (hw.keithley->Initialize("smu_config.json")) {
    logger->LogInfo("Keithley2400Manager initialized from config");

    if (hw.keithley->ConnectAll()) {
      logger->LogInfo("Connected to Keithley 2400 SMU");
      hw.keithleyOps = std::make_unique<Keithley2400Operations>(*hw.keithley);
      anySuccess = true;
    }
    else {
      logger->LogWarning("Could not connect to Keithley 2400 - SMU operations disabled");
    }
  }
  else {
    logger->LogWarning("Failed to load Keithley config, trying fallback");
    hw.keithley->AddClient("Keithley-Main", "127.0.0.101", 8888);

    if (hw.keithley->ConnectAll()) {
      logger->LogInfo("Connected to Keithley 2400 (fallback)");
      hw.keithleyOps = std::make_unique<Keithley2400Operations>(*hw.keithley);
      anySuccess = true;
    }
    else {
      logger->LogInfo("No Keithley hardware available - SMU operations disabled");
    }
  }


  //Initialize SPDPowerSupply
  InitConfigSPDPowerSupply(hw);

  return anySuccess;  // Return true if at least one instrument connected
}

bool ApplicationInitializer::InitDataClients(HardwareManagers& hw) {
  try {
    hw.dataClient = std::make_unique<DataClientManager>("DataServerConfig.json");
    logger->LogInfo("DataClientManager initialized successfully");
    return true;
  }
  catch (const std::exception& e) {
    logger->LogError("Failed to initialize DataClientManager: " + std::string(e.what()));
    return false;
  }
}

bool ApplicationInitializer::CreateMotionControl(HardwareManagers& hw) {
  if (hw.piController && hw.acsController && hw.motionConfig) {
    hw.motionControl = std::make_unique<MotionControlLayer>(
      *hw.motionConfig, *hw.piController, *hw.acsController);

    hw.motionControl->SetPathCompletionCallback([this](bool success) {
      if (success) {
        logger->LogInfo("Path execution completed successfully");
      }
      else {
        logger->LogWarning("Path execution failed or was cancelled");
      }
    });

    logger->LogInfo("MotionControlLayer created");
    return true;
  }

  logger->LogError("Cannot create MotionControlLayer - missing dependencies");
  return false;
}

bool ApplicationInitializer::CreateMachineOps(HardwareManagers& hw, Operations& ops) {
  if (hw.motionControl && hw.piController && hw.ioManager && hw.pneumatic) {
    ops.machine = std::make_unique<MachineOperations>(
      *hw.motionControl,
      *hw.piController,
      *hw.ioManager,
      *hw.pneumatic,
      hw.laserOps.get(),      // Can be nullptr
      hw.camera.get(),
      hw.keithleyOps.get()    // Can be nullptr
    );

    // Set optional operations if available
    if (hw.laserOps) {
      ops.machine->SetLaserOperations(hw.laserOps.get());
      logger->LogInfo("MachineOperations created WITH laser support");
    }
    else {
      logger->LogInfo("MachineOperations created WITHOUT laser support");
    }

    if (hw.keithleyOps) {
      ops.machine->SetSMUOperations(hw.keithleyOps.get());
      logger->LogInfo("SMU operations enabled");
    }

    return true;
  }

  logger->LogError("Cannot create MachineOperations - missing core components");
  return false;
}

bool ApplicationInitializer::InitOperations(HardwareManagers& hw, Operations& ops) {
  bool allSuccess = true;

  // Initialize MotionOps
  if (context.GetMotionControlLayer() && context.GetPIController()) {
    ops.motion = std::make_unique<MotionOps>(
      *context.GetMotionControlLayer(),
      *context.GetPIController(),
      context.GetDatabaseManagerShared(),
      context.GetResultsManagerShared()
    );

    if (ops.motion->Initialize()) {
      logger->LogInfo("MotionOps initialized successfully");
    }
    else {
      logger->LogError("MotionOps initialization failed");
      allSuccess = false;
    }
  }

  // Initialize IOOps
  if (context.GetIOManager() && context.GetPneumaticManager()) {
    ops.io = std::make_unique<IOOps>(
      *context.GetIOManager(),
      *context.GetPneumaticManager(),
      context.GetDatabaseManagerShared(),
      context.GetResultsManagerShared()
    );

    if (ops.io->Initialize()) {
      logger->LogInfo("IOOps initialized successfully");
    }
    else {
      logger->LogError("IOOps initialization failed");
      allSuccess = false;
    }
  }

  // Initialize VisionOps
  if (context.GetCameraManager()) {
    ops.vision = std::make_unique<VisionOps>(
      context.GetCameraManager(),
      context.GetDatabaseManagerShared(),
      context.GetResultsManagerShared()
    );

    if (ops.vision->Initialize()) {
      logger->LogInfo("VisionOps initialized successfully");
    }
    else {
      logger->LogError("VisionOps initialization failed");
      allSuccess = false;
    }
  }

  // Create DUT Data Recorder
  hw.dutRecorder = std::make_unique<DUTDataRecorder>();
  logger->LogInfo("DUTDataRecorder created");

  return allSuccess;
}

bool ApplicationInitializer::RegisterWithContext(HardwareManagers& hw, Operations& ops) {
  logger->LogInfo("Registering services with AppContext...");

  // Register logger first
  if (logger) {
    context.RegisterLogger(logger);
  }

  // Register hardware managers
  if (hw.motionConfig) {
    context.RegisterExistingMotionConfig(hw.motionConfig.get());
  }
  if (hw.motionControl) {
    context.RegisterExistingMotionControlLayer(hw.motionControl.get());
  }
  if (hw.piController) {
    context.RegisterExistingPIController(hw.piController.get());
  }
  if (hw.acsController) {
    context.RegisterExistingACSController(hw.acsController.get());
  }
  if (hw.ioManager) {
    context.RegisterExistingIOManager(hw.ioManager.get());
  }
  if (hw.ioConfig) {
    context.RegisterExistingIOConfig(hw.ioConfig.get());
  }
  if (hw.pneumatic) {
    context.RegisterExistingPneumatic(hw.pneumatic.get());
  }
  if (hw.camera) {
    context.RegisterExistingCameraManager(hw.camera.get());
  }
  if (hw.cameraConfig) {
    context.RegisterExistingCameraConfig(hw.cameraConfig.get());
  }
  if (hw.dataClient) {
    context.RegisterExistingDataClient(hw.dataClient.get());
  }
  if (hw.laser) {
    context.RegisterExistingCLD101x(hw.laser.get());
  }
  if (hw.keithley) {
    context.RegisterExistingKeithley(hw.keithley.get());
  }
  if (hw.spdPowerSupply)  {
    context.RegisterExistingSPDPowerSupplyManager(hw.spdPowerSupply.get());
  }

  // Register operations
  if (ops.machine) {
    context.RegisterExistingMachineOps(ops.machine.get());

    // Register database managers from MachineOperations
    if (ops.machine->GetDatabaseManager()) {
      context.RegisterExistingDatabaseManager(ops.machine->GetDatabaseManager().get());
    }
    if (ops.machine->GetResultsManager()) {
      context.RegisterExistingOperationResultsManager(ops.machine->GetResultsManager().get());
    }
  }
  if (ops.motion) {
    context.RegisterExistingMotionOps(ops.motion.get());
  }
  if (ops.io) {
    context.RegisterExistingIOOps(ops.io.get());
  }
  if (ops.vision) {
    context.RegisterExistingVisionOps(ops.vision.get());
  }
  if (hw.dutRecorder) {
    context.RegisterExistingDUTDataRecorder(hw.dutRecorder.get());
  }
  if (hw.configWatchdog) {
    context.RegisterConfigWatchdog(hw.configWatchdog.get());
  }

  // Log final status
  context.LogInitializationStatus();

  return true;
}

bool ApplicationInitializer::InitConfigWatchdog(HardwareManagers& hw) {
  if (!databaseAvailable) {
    logger->LogInfo("Skipping config watchdog - database not available");
    return false;
  }

  logger->LogInfo("Starting configuration file watchdog...");

  hw.configWatchdog = std::make_unique<ConfigFileWatchdog>(1000, true, logger);

  // Add all configuration files
  int addedCount = hw.configWatchdog->AddFiles(configFiles);
  logger->LogInfo("Added " + std::to_string(addedCount) + " files to watchdog");

  // Add callback
  hw.configWatchdog->AddChangeCallback([this](const ConfigFileWatchdog::FileChangeEvent& event) {
    if (event.updateSuccess) {
      logger->LogInfo("Config file auto-synced: " + event.filename);
    }
    else if (!event.errorMessage.empty()) {
      logger->LogWarning("Auto-sync failed for " + event.filename);
    }
  });

  // Start watchdog
  if (hw.configWatchdog->Start()) {
    logger->LogInfo("Configuration watchdog started");
    return true;
  }
  else {
    logger->LogError("Failed to start configuration watchdog");
    hw.configWatchdog.reset();
    return false;
  }
}


bool ApplicationInitializer::InitConfigSPDPowerSupply(HardwareManagers& hw) {
  // Create the SPD Power Supply Manager
  hw.spdPowerSupply = std::make_unique<SPDPowerSupplyManager>();

  // Try to initialize with the config file
  if (hw.spdPowerSupply->Initialize("spd_devices_config.json")) {
    // Config file found and loaded successfully
    logger->LogInfo("✅ SPD Power Supply initialized from config file");

    // Log device count after initialization
    logger->LogInfo("SPD Power Supply initialized, device count: " +
      std::to_string(hw.spdPowerSupply->GetDeviceNames().size()));

    // Auto-discover any additional devices not in config
    hw.spdPowerSupply->AddDiscoveredDevices(false); // false = don't auto-connect yet

    // Connect all devices
    hw.spdPowerSupply->ConnectAll();

    // Optional: Test first device if available
    auto deviceNames = hw.spdPowerSupply->GetDeviceNames();
    if (!deviceNames.empty()) {
      try {
        std::string spdID = hw.spdPowerSupply->GetDevice(deviceNames[0])->getInstrumentID();
        logger->LogDebug("SPD Power Supply IDN: " + spdID);
      }
      catch (const std::exception& e) {
        logger->LogWarning("Could not get instrument ID: " + std::string(e.what()));
      }
    }

    // Optional: Start polling (uncomment if needed)
    // hw.spdPowerSupply->StartAllPolling(2000); // Poll every 2 seconds

    logger->LogInfo("SPD Power Supply Manager ready");
    return true;
  }
  else {
    // Config file not found, try loading default configuration
    logger->LogInfo("📄 SPD config file not found, loading default configuration");

    try {
      hw.spdPowerSupply->LoadDefaultConfiguration();

      // Auto-discover devices
      hw.spdPowerSupply->AddDiscoveredDevices(false);

      // Connect all devices
      hw.spdPowerSupply->ConnectAll();

      logger->LogInfo("SPD Power Supply initialized with default configuration, device count: " +
        std::to_string(hw.spdPowerSupply->GetDeviceNames().size()));

      return true;
    }
    catch (const std::exception& e) {
      logger->LogError("Failed to initialize SPD Power Supply with default config: " + std::string(e.what()));
      hw.spdPowerSupply.reset(); // Clean up on failure
      return false;
    }
  }
}


void ApplicationInitializer::LogHardwareVelocities(PIControllerManager* pi, ACSControllerManager* acs) {
  // Log PI velocities
  if (pi) {
    logger->LogInfo("Reading PI controller velocities...");
    std::vector<std::string> piNames = { "hex-left", "hex-right", "hex-bottom" };

    for (const auto& name : piNames) {
      auto* controller = pi->GetController(name);
      if (controller && controller->IsConnected()) {
        double velocity = 0.0;
        if (controller->GetSystemVelocity(velocity)) {
          logger->LogInfo("  " + name + ": " + std::to_string(velocity) + " mm/s");
        }
      }
    }
  }

  // Log ACS velocities
  if (acs) {
    logger->LogInfo("Reading ACS controller velocities...");
    auto* controller = acs->GetController("gantry-main");

    if (controller && controller->IsConnected()) {
      std::vector<std::string> axes = { "X", "Y", "Z" };
      for (const auto& axis : axes) {
        double velocity = 0.0;
        if (controller->GetVelocity(axis, velocity)) {
          logger->LogInfo("  gantry-main " + axis + ": " +
            std::to_string(velocity) + " mm/s");
        }
      }
    }
  }
}

std::vector<std::string> ApplicationInitializer::ScanPresetsFolder(const std::string& folder) {
  std::vector<std::string> presets;

  try {
    if (std::filesystem::exists(folder) && std::filesystem::is_directory(folder)) {
      for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
          std::string extension = entry.path().extension().string();
          std::transform(extension.begin(), extension.end(),
            extension.begin(), ::tolower);

          if (extension == ".json") {
            presets.push_back(entry.path().string());
            logger->LogInfo("  Found preset: " + entry.path().filename().string());
          }
        }
      }
    }
  }
  catch (const std::exception& e) {
    logger->LogError("Error scanning presets: " + std::string(e.what()));
  }

  return presets;
}

std::string ApplicationInitializer::GetInitializationReport() const {
  std::stringstream report;

  report << "=== Initialization Report ===\n";
  report << "Total Steps: " << initSteps.size() << "\n\n";

  for (const auto& step : initSteps) {
    report << std::left << std::setw(20) << step.name << ": ";

    if (step.completed) {
      if (step.success) {
        report << "SUCCESS (" << step.duration.count() << "ms)";
      }
      else {
        report << "FAILED";
        if (!step.errorMessage.empty()) {
          report << " - " << step.errorMessage;
        }
      }
    }
    else {
      report << "NOT EXECUTED";
    }

    if (!step.required) {
      report << " [OPTIONAL]";
    }

    report << "\n";
  }

  return report.str();
}