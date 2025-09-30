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

  // Auto-register on startup using static initialization
  static struct UAA3AutoRegister {
    UAA3AutoRegister() {
      RegisterExistingUAA3Processes();
    }
  } g_uaa3AutoRegister;

} // namespace UAA3ProcessRegistration