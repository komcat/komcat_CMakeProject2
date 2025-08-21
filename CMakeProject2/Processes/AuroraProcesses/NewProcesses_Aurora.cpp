// ============================================================================
// NewProcesses_Aurora.cpp - FIXED VERSION
// ============================================================================

#include "NewProcesses_Aurora.h"
#include "ProcessRegistry.h"
#include <memory>

namespace AuroraProcesses {

  std::unique_ptr<SequenceStep> BuildAuroraSimpleSMUTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    std::shared_ptr<Keithley2400Operations> smuOps) {

    auto sequence = std::make_unique<SequenceStep>("Aurora Simple SMU Test", machineOps);

    // 1. User prompt - Start simple SMU test
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Aurora Simple SMU Test",
      "Ready to start simple SMU test?\n\n"
      "This will:\n"
      "1. Reset SMU\n"
      "2. Basic on/off test\n"
      "3. Simple voltage sweep (0-5V)\n"
      "4. Simple current sweep (0-10mA)\n\n"
      "Click Yes to continue.",
      promptUI));

    // 2. Reset SMU instrument
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(""));

    // 3. Basic on/off test
    sequence->AddOperation(std::make_shared<SetupKeithleyVoltageSourceOperation>(
      0.0, 0.1, "AUTO", ""));

    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(true, ""));
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(1000, "Output ON test"));
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(""));
    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(false, ""));

    // 4. User confirmation for voltage sweep
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Voltage Sweep",
      "Basic test completed.\n\n"
      "Next: Simple voltage sweep (0-5V, 21 steps)\n"
      "Click Yes to start.",
      promptUI));

    // 5. Simple voltage sweep
    sequence->AddOperation(std::make_shared<SimpleVoltageSweepOperation>(
      0.0, 5.0, 21, 0.1, 100, ""));

    // 6. User confirmation for current sweep
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Current Sweep",
      "Voltage sweep completed!\n\n"
      "Next: Current sweep (0-10mA, 11 steps)\n"
      "Click Yes to start.",
      promptUI));

    // 7. Quick current sweep
    sequence->AddOperation(std::make_shared<QuickCurrentSweepOperation>(
      QuickCurrentSweepOperation::MILLI_CURRENT, ""));

    // 8. Safety shutdown
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(""));

    // 9. Completion message
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Simple SMU Test Complete",
      "Simple SMU test completed successfully!\n\n"
      "✓ Basic functionality test\n"
      "✓ Voltage sweep (0-5V, 21 steps)\n"
      "✓ Current sweep (0-10mA, 11 steps)\n"
      "✓ SMU safely reset\n\n"
      "Check logs for measurement data.",
      promptUI));

    return sequence;
  }

  // ============================================================================
    // VERY BASIC TEST - Minimal SMU operations with specified client name
    // ============================================================================
  std::unique_ptr<SequenceStep> BuildAuroraBasicSMUTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    std::shared_ptr<Keithley2400Operations> smuOps) {

    auto sequence = std::make_unique<SequenceStep>("Aurora Basic SMU Test", machineOps);

    // Define SMU client name
    const std::string smuClientName = "SMU1";  // Your configured SMU name

    // 1. Simple start prompt
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Basic SMU Test",
      "Starting basic SMU functionality test.\n\n"
      "Target SMU: " + smuClientName + "\n\n"
      "This will test:\n"
      "• SMU reset\n"
      "• Output enable/disable\n"
      "• Basic measurements\n\n"
      "Click Yes to start.",
      promptUI));

    // 2. Reset SMU
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(smuClientName));

    // 3. Setup 1V output
    sequence->AddOperation(std::make_shared<SetupKeithleyVoltageSourceOperation>(
      1.0, 0.1, "AUTO", smuClientName));

    // 4. Enable output and test
    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(true, smuClientName));
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(2000, "Testing 1V output on " + smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(smuClientName));

    // 5. Test different voltage
    sequence->AddOperation(std::make_shared<SetupKeithleyVoltageSourceOperation>(
      2.5, 0.1, "AUTO", smuClientName));
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(1000, "Testing 2.5V output on " + smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(smuClientName));

    // 6. Disable output
    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(false, smuClientName));

    // 7. Test current source mode
    sequence->AddOperation(std::make_shared<SetupKeithleyCurrentSourceOperation>(
      0.001, 10.0, "AUTO", smuClientName));
    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(true, smuClientName));
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(1000, "Testing 1mA output on " + smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(smuClientName));
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(smuClientName));

    // 8. Final cleanup
    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(false, smuClientName));
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(smuClientName));

    // 9. Completion
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Basic Test Complete",
      "Basic SMU test completed!\n\n"
      "Target SMU: " + smuClientName + "\n\n"
      "All basic functions tested successfully.\n"
      "Check logs for measurement values.",
      promptUI));

    return sequence;
  }

  // ============================================================================
  // WRAPPER FUNCTIONS - Match ProcessRegistry signature
  // ============================================================================
  std::unique_ptr<SequenceStep> WrapperAuroraSimpleSMU(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildAuroraSimpleSMUTest(machineOps, promptUI, nullptr);
  }

  std::unique_ptr<SequenceStep> WrapperAuroraBasicSMU(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildAuroraBasicSMUTest(machineOps, promptUI, nullptr);
  }

  // ============================================================================
  // UPDATED REGISTRATION with Both Aurora Processes
  // ============================================================================
  void RegisterAllAuroraProcesses() {
    auto& registry = ProcessRegistry::GetInstance();

    // Register Aurora Simple SMU Test (using wrapper)
    registry.RegisterProcess(
      "Aurora_SimpleSMU",
      "Aurora_Core",
      "Aurora Simple SMU test with basic sweeps",
      true,
      WrapperAuroraSimpleSMU
    );

    // Register Aurora Basic SMU Test (using wrapper)
    registry.RegisterProcess(
      "Aurora_BasicSMU",
      "Aurora_Core",
      "Aurora Basic SMU functionality test",
      true,
      WrapperAuroraBasicSMU
    );

    printf("Aurora Processes: Successfully registered 2 Aurora SMU processes\n");
  }

  size_t GetAuroraProcessCount() {
    return 2; // Now we have two processes
  }

  bool AreAuroraProcessesRegistered() {
    auto& registry = ProcessRegistry::GetInstance();
    return registry.HasProcess("Aurora_SimpleSMU") &&
      registry.HasProcess("Aurora_BasicSMU");
  }

  // ========================================================================
  // AUTO-REGISTRATION - Registers all Aurora processes automatically
  // ========================================================================
  static struct AuroraAutoRegister {
    AuroraAutoRegister() {
      RegisterAllAuroraProcesses();
    }
  } g_auroraAutoRegister;

} // namespace AuroraProcesses