// ============================================================================
// ProcessInitializer.cpp - Register existing UAA3 processes
// ============================================================================

#include "ProcessRegistry.h"
#include "uaa3_process_builders.h"
#include "ProcessConfiguration.h"
#include "ProcessConfigBuilders.h"


using namespace UAA3ProcessBuilders;

namespace UAA3ProcessRegistration {

  void RegisterExistingUAA3Processes() {
    auto& registry = ProcessRegistry::GetInstance();

    registry.RegisterProcess(
      "uaa3 dev mock up",
      "Core",
      "just do something",
      true,
      UAA3ProcessBuilders::BuildDevSequence_uaa3
    );

    // ========================================================================
    // CORE PROCESSES
    // ========================================================================
    registry.RegisterProcess(
      "UAA3_Initialization",
      "Core",
      "System initialization sequence - moves all devices to safe positions",
      true,
      UAA3ProcessBuilders::BuildInitializationSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_Probing",
      "Core",
      "Probing sequence with laser operation and position verification",
      true,
      UAA3ProcessBuilders::BuildProbingSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_PickPlaceLeftLens",
      "Core",
      "Pick and place left lens operation with grip verification",
      true,
      UAA3ProcessBuilders::BuildPickPlaceLeftLensSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_PickPlaceLeftLens_Configurable",
      "Core",
      "Configurable Pick and place left lens operation",
      true,
      [](MachineOperations& ops, UserPromptUI& ui) {
      // Create or load the configuration here
      ProcessConfiguration config = ProcessConfigBuilders::createPickPlaceConfig();

      // Try to load from last saved config
      if (std::filesystem::exists("last_pickplace_config.json")) {
        config.loadFromFile("last_pickplace_config.json");
      }

      return UAA3ProcessBuilders::BuildPickPlaceLeftLensSequence_uaa3_Configurable(
        ops, ui, config);
    }
    );

    registry.RegisterProcess(
      "UAA3_PickPlaceRightLens",
      "Core",
      "Pick and place right lens operation with grip verification",
      true,
      UAA3ProcessBuilders::BuildPickPlaceRightLensSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_PickPlaceRightLens_Configurable",
      "Core",
      "Configurable Pick and place right lens operation",
      true,
      [](MachineOperations& ops, UserPromptUI& ui) {
      // Create or load the configuration here
      ProcessConfiguration config = ProcessConfigBuilders::createPickPlaceConfig();

      // Try to load from last saved config
      if (std::filesystem::exists("last_pickplace_config.json")) {
        config.loadFromFile("last_pickplace_config.json");
      }

      return UAA3ProcessBuilders::BuildPickPlaceRightLensSequence_uaa3_Configurable(
        ops, ui, config);
    }
    );

    registry.RegisterProcess(
      "UAA3_UVCuring",
      "Core",
      "UV curing process with temperature and safety controls",
      true,
      UAA3ProcessBuilders::BuildUVCuringSequence_uaa3
    );


    registry.RegisterProcess(
      "UAA3_UVCuring_Configurable",
      "Core",
      "Configurable UV curing process with temperature and safety controls",
      true,
      [](MachineOperations& ops, UserPromptUI& ui) {
      // Create or load the configuration
      ProcessConfiguration config = ProcessConfigBuilders::createUVCuringConfig();

      // Try to load from last saved UV config
      if (std::filesystem::exists("last_uvcuring_config.json")) {
        config.loadFromFile("last_uvcuring_config.json");
      }

      return UAA3ProcessBuilders::BuildUVCuringSequence_uaa3_Configurable(
        ops, ui, config);
    }
    );


    // ========================================================================
    // UTILITY SEQUENCES
    // ========================================================================
    registry.RegisterProcess(
      "UAA3_RejectLeftLens",
      "Utility",
      "Reject left lens operation - safe disposal sequence",
      true,
      UAA3ProcessBuilders::RejectLeftLensSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_RejectRightLens",
      "Utility",
      "Reject right lens operation - safe disposal sequence",
      true,
      UAA3ProcessBuilders::RejectRightLensSequence_uaa3
    );

    // ========================================================================
    // CALIBRATION SEQUENCES
    // ========================================================================
    registry.RegisterProcess(
      "UAA3_NeedleCalibration",
      "Calibration",
      "Enhanced needle XY calibration with precision positioning",
      true,
      UAA3ProcessBuilders::BuildNeedleXYCalibrationSequenceEnhanced_uaa3
    );

    registry.RegisterProcess(
      "UAA3_DispenseCalibration1",
      "Calibration",
      "Dispense calibration sequence for location 1",
      true,
      UAA3ProcessBuilders::BuildDispenseCalibrationSequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_DispenseCalibration2",
      "Calibration",
      "Dispense calibration sequence for location 2",
      true,
      UAA3ProcessBuilders::BuildDispenseCalibration2Sequence_uaa3
    );

    // ========================================================================
    // DISPENSING SEQUENCES
    // ========================================================================
    registry.RegisterProcess(
      "UAA3_DispenseEpoxy1",
      "Dispensing",
      "Dispense epoxy at location 1 with precision control",
      true,
      UAA3ProcessBuilders::BuildDispenseEpoxy1Sequence_uaa3
    );

    registry.RegisterProcess(
      "UAA3_DispenseEpoxy2",
      "Dispensing",
      "Dispense epoxy at location 2 with precision control",
      true,
      UAA3ProcessBuilders::BuildDispenseEpoxy2Sequence_uaa3
    );

    registry.RegisterProcess(
      "Siphog_CoordinateTest",
      "Utility",
      "System test sequence for coordinate system verification",
      true,
      UAA3ProcessBuilders::BuildSystemTestSequence
    );

    registry.RegisterProcess(
      "Siphog_CoordinateTestExtended",
      "Utility",
      "Extended system test sequence with advanced movements",
      true,
      UAA3ProcessBuilders::BuildExtendedSystemTestSequence
    );

    // Print registration summary
    printf("UAA3 Process Registration: Successfully registered %zu processes\n",
      registry.GetProcessCount());
  }

  void RegisterCoreProcesses() {
    auto& registry = ProcessRegistry::GetInstance();

    // ========================================================================
// Core_PickPlace - PARAMETERIZED VERSION
// ========================================================================
    registry.RegisterProcessWithParams(
      "Core_PickPlace",
      "Core",
      "High precision pick and place with camera view",
      true,
      [](MachineOperations& ops, UserPromptUI& ui,
        const std::map<std::string, std::string>& params) -> std::unique_ptr<SequenceStep> {

      // Helper functions
      auto getParam = [&params](const std::string& key, const std::string& defaultValue) {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : defaultValue;
      };

      auto getFloatParam = [&params](const std::string& key, float defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          try { return std::stof(it->second); }
          catch (...) { return defaultValue; }
        }
        return defaultValue;
      };

      auto getBoolParam = [&params](const std::string& key, bool defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          return (it->second == "true" || it->second == "1");
        }
        return defaultValue;
      };

      // Extract ALL parameters
      std::string deviceName = getParam("deviceName", "hex-right");
      std::string graphName = getParam("graphName", "Process_Flow");
      std::string pickNode = getParam("pickNode", "node_5245");
      std::string placeNode = getParam("placeNode", "node_5263");
      std::string cameraGantry = getParam("cameraGantryDevice", "gantry-main");
      std::string cameraViewPickNode = getParam("cameraViewPickNode", "node_4209");
      std::string cameraViewPlaceNode = getParam("cameraViewPlaceNode", "node_4209");
      std::string gripperOutputDevice = getParam("gripperOutputDevice", "IOBottom");
      std::string gripperPinName = getParam("gripperPinName", "gripper-right");  // NEW: pin name
      float gripperHoldDelay = getFloatParam("gripperHoldDelay", 1.5f);
      float speed = getFloatParam("speed", 10.0f);
      bool enableCameraView = getBoolParam("enableCameraView", true);

      // Call builder with ALL parameters including pin name
      return UAA3ProcessBuilders::createCorePickPlace(
        ops, ui,
        deviceName,
        graphName,
        pickNode,
        placeNode,
        cameraGantry,
        cameraViewPickNode,
        cameraViewPlaceNode,
        gripperOutputDevice,
        gripperPinName,         // Pin name instead of pin number
        gripperHoldDelay,
        speed,
        enableCameraView
      );
    }
    );

    // ========================================================================
    // Core_PickOnly - PARAMETERIZED VERSION
    // ========================================================================
    registry.RegisterProcessWithParams(
      "Core_PickOnly",
      "Core",
      "Quality pick operation with camera verification",
      true,
      [](MachineOperations& ops, UserPromptUI& ui,
        const std::map<std::string, std::string>& params) -> std::unique_ptr<SequenceStep> {

      auto getParam = [&params](const std::string& key, const std::string& defaultValue) {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : defaultValue;
      };

      auto getFloatParam = [&params](const std::string& key, float defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          try { return std::stof(it->second); }
          catch (...) { return defaultValue; }
        }
        return defaultValue;
      };

      auto getBoolParam = [&params](const std::string& key, bool defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          return (it->second == "true" || it->second == "1");
        }
        return defaultValue;
      };

      std::string deviceName = getParam("deviceName", "Precision_Manipulator");
      std::string pickNode = getParam("pickNode", "Precision_Pick_Point");
      std::string cameraGantry = getParam("cameraGantryDevice", "gantry-main");
      std::string cameraViewNode = getParam("cameraViewNode", "Precision_View_Point");
      float speed = getFloatParam("speed", 50.0f);
      bool enableCameraView = getBoolParam("enableCameraView", true);

      return UAA3ProcessBuilders::createCorePickOnly(
        ops, ui, deviceName, pickNode,
        cameraGantry, cameraViewNode, speed, enableCameraView
      );
    }
    );

    // ========================================================================
    // Core_PlaceOnly - PARAMETERIZED VERSION
    // ========================================================================
    registry.RegisterProcessWithParams(
      "Core_PlaceOnly",
      "Core",
      "Quality place operation with camera verification",
      true,
      [](MachineOperations& ops, UserPromptUI& ui,
        const std::map<std::string, std::string>& params) -> std::unique_ptr<SequenceStep> {

      auto getParam = [&params](const std::string& key, const std::string& defaultValue) {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : defaultValue;
      };

      auto getFloatParam = [&params](const std::string& key, float defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          try { return std::stof(it->second); }
          catch (...) { return defaultValue; }
        }
        return defaultValue;
      };

      auto getBoolParam = [&params](const std::string& key, bool defaultValue) {
        auto it = params.find(key);
        if (it != params.end()) {
          return (it->second == "true" || it->second == "1");
        }
        return defaultValue;
      };

      std::string deviceName = getParam("deviceName", "hex-right");
      std::string placeNode = getParam("placeNode", "node_5263");
      std::string cameraGantry = getParam("cameraGantryDevice", "gantry-main");
      std::string cameraViewNode = getParam("cameraViewNode", "QC_Camera_Position");
      float speed = getFloatParam("speed", 50.0f);
      bool enableCameraView = getBoolParam("enableCameraView", true);

      return UAA3ProcessBuilders::createCorePlaceOnly(
        ops, ui, deviceName, placeNode,
        cameraGantry, cameraViewNode, speed, enableCameraView
      );
    }
    );

    printf("RegisterCoreProcesses: Registered 3 parameterized core processes\n");
  }


  // Auto-register on startup using static initialization
  static struct UAA3AutoRegister {
    UAA3AutoRegister() {
      RegisterExistingUAA3Processes();
      RegisterCoreProcesses();
    }
  } g_uaa3AutoRegister;

} // namespace UAA3ProcessRegistration