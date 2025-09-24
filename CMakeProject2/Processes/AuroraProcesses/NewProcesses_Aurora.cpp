// ============================================================================
// NewProcesses_Aurora.cpp - FIXED VERSION
// ============================================================================

#include "NewProcesses_Aurora.h"
#include "ProcessRegistry.h"
#include "SequenceStep.h"  // Has DUT operations
#include "SPDOperations.h"
#include "UserInputOperations.h"
// Add this include at the top with the others
#include "PowerSupplyOperations.h"
#include <memory>
#include <chrono>       // For timestamp
#include <sstream>      // For stringstream
#include <iomanip>      // For put_time

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

    // NEW: Get DUT serial number from user instead of auto-generating
// Generate a default suggestion based on timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;

    // Use localtime_s for Windows
    std::tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    ss << "DUT_" << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
    std::string defaultSerial = ss.str();

    // Ask user for serial number with auto-generated default
    sequence->AddOperation(std::make_shared<UserInputOperation>(
      "dut_serial",                          // storage key
      "Enter DUT Serial Number",             // title
      "Please enter the DUT serial number:\n"
      "(Leave blank to use auto-generated)",  // prompt
      promptUI,
      defaultSerial,                         // default value (auto-generated)
      true,                                  // required (but has default)
      300                                    // 5 minute timeout
    ));

    // START DUT RECORDING - Use the user-provided serial number
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "dut_serial",
      [](MachineOperations& ops, const std::string& dutSerialNumber) {
      // Create and execute DUTStartRecordingOperation with user input
      auto startRecording = std::make_shared<DUTStartRecordingOperation>(dutSerialNumber);
      return startRecording->Execute(ops);
    }
    ));

    // 2. Reset SMU instrument
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(""));

    // 3. Basic on/off test
    sequence->AddOperation(std::make_shared<SetupKeithleyVoltageSourceOperation>(
      0.0, 0.1, "AUTO", ""));

    sequence->AddOperation(std::make_shared<SetKeithleyOutputOperation>(true, ""));
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(1000, "Output ON test"));

    // Read and record initial voltage/current using correct channel names
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Voltage"));

    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Current"));

    // Also record power and resistance if available
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Power"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Resistance"));

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

    // Record values after voltage sweep
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Voltage"));

    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Current"));

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

    // Record final values after current sweep
    sequence->AddOperation(std::make_shared<ReadKeithleyVoltageOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Voltage"));

    sequence->AddOperation(std::make_shared<ReadKeithleyCurrentOperation>(""));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Current"));

    // Record power and resistance at the end
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Power"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SMU1-Resistance"));

    // 8. Safety shutdown
    sequence->AddOperation(std::make_shared<ResetKeithleyOperation>(""));

    // END DUT RECORDING AND EXPORT DATA
    sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));

    // 9. Completion message with user's serial number
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "dut_serial",
      [&promptUI](MachineOperations& ops, const std::string& dutSerialNumber) {
      // Create completion message with actual serial number used
      auto completionPrompt = UserPromptOperation::CreateBasic(
        "Simple SMU Test Complete",
        "Simple SMU test completed successfully!\n\n"
        "✓ Basic functionality test\n"
        "✓ Voltage sweep (0-5V, 21 steps)\n"
        "✓ Current sweep (0-10mA, 11 steps)\n"
        "✓ SMU safely reset\n"
        "✓ Data saved to dut_saved/" + dutSerialNumber + "\n\n"
        "Serial Number: " + dutSerialNumber + "\n"
        "Check logs and saved data files.",
        promptUI);
      return completionPrompt->Execute(ops);
    }
    ));

    // Clear the stored input at the end
    sequence->AddOperation(std::make_shared<ClearUserInputOperation>("dut_serial"));

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




  std::unique_ptr<SequenceStep> BuildAuroraSPDTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Aurora SPD Power Supply Test", machineOps);

    // 1. Start prompt
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "SPD Power Supply Test",
      "Starting SPD Power Supply test.\n\n"
      "This will test:\n"
      "• CV mode: 3.3V with current limit\n"
      "• Voltage sweep: 0V to 3.3V (11 steps)\n"
      "• CC mode: 0.3A with voltage limit\n"
      "• Current sweep: 0A to 0.3A (11 steps)\n"
      "• Output enable/disable\n"
      "• DUT data recording\n\n"
      "Click Yes to start.",
      promptUI));

    // 2. Get DUT serial number from user - always ask, no default
    sequence->AddOperation(std::make_shared<UserInputOperation>(
      "spd_dut_serial",
      "Enter SPD DUT Serial Number",
      "Please enter the DUT serial number for this test:",
      promptUI,
      "",  // No default - force user to enter
      true,  // Required
      300    // 5 minute timeout
    ));

    // 3. START DUT RECORDING
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "spd_dut_serial",
      [](MachineOperations& ops, const std::string& dutSerialNumber) {
        auto startRecording = std::make_shared<DUTStartRecordingOperation>(dutSerialNumber);
        return startRecording->Execute(ops);
      }
    ));

    // 4. Test CV Mode - 3.3V
    sequence->AddOperation(std::make_shared<SPD_SetConstantVoltageOperation>(
      3.3, 0.3, "SPD1", "CV_3V3_Test"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      true, "SPD1", 1000));

    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "CV_Mode_Reading"));

    // Record CV mode data to DUT with step1 label
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step1_cv_mode"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step1_cv_mode"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      false, "SPD1", 500));

    // 5. Voltage sweep test
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Voltage Sweep Test",
      "CV mode test completed (3.3V).\n\n"
      "Next: Voltage sweep (0V to 3.3V, 11 steps)\n"
      "Current limit: 0.3A, Step delay: 200ms\n\n"
      "Click Yes to continue.",
      promptUI));

    sequence->AddOperation(std::make_shared<SPD_VoltageSweepOperation>(
      0.0, 3.3, 11, 0.3, "SPD1", 200.0, "Voltage_Sweep_0_to_3V3"));

    // Record post-sweep measurements with step2 label
    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "Post_Voltage_Sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step2_voltage_sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step2_voltage_sweep"));

    // 6. CC Mode test
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "CC Mode Test",
      "Voltage sweep completed!\n\n"
      "Next: CC mode test (0.3A, 3.3V limit)\n"
      "Click Yes to continue.",
      promptUI));

    sequence->AddOperation(std::make_shared<SPD_SetConstantCurrentOperation>(
      0.3, 3.3, "SPD1", "CC_300mA_Test"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      true, "SPD1", 1000));

    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "CC_Mode_Reading"));

    // Record CC mode data to DUT with step3 label
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step3_cc_mode"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step3_cc_mode"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      false, "SPD1", 500));

    // 7. Current sweep test
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Current Sweep Test",
      "CC mode test completed!\n\n"
      "Next: Current sweep (0A to 0.3A, 11 steps)\n"
      "Voltage limit: 3.3V, Step delay: 200ms\n\n"
      "Click Yes to continue.",
      promptUI));

    sequence->AddOperation(std::make_shared<SPD_CurrentSweepOperation>(
      0.0, 0.3, 11, 3.3, "SPD1", 200.0, "Current_Sweep_0_to_300mA"));

    // Record post-current-sweep measurements with step4 label
    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "Post_Current_Sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step4_current_sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step4_current_sweep"));

    // 8. END DUT RECORDING AND EXPORT DATA
    sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));

    // 9. Final completion message with serial number
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "spd_dut_serial",
      [&promptUI](MachineOperations& ops, const std::string& dutSerialNumber) {
        auto completionPrompt = UserPromptOperation::CreateBasic(
          "SPD Test Complete",
          "SPD Power Supply test completed!\n\n"
          "✓ CV mode: 3.3V test\n"
          "✓ Voltage sweep: 0V to 3.3V (11 steps)\n"
          "✓ CC mode: 0.3A test\n"
          "✓ Current sweep: 0A to 0.3A (11 steps)\n"
          "✓ All measurements recorded\n"
          "✓ Outputs safely disabled\n"
          "✓ DUT data saved to dut_saved/" + dutSerialNumber + "\n\n"
          "Serial Number: " + dutSerialNumber + "\n"
          "Check logs and saved data files.",
          promptUI);
        return completionPrompt->Execute(ops);
      }
    ));

    // Clear the stored input at the end
    sequence->AddOperation(std::make_shared<ClearUserInputOperation>("spd_dut_serial"));

    return sequence;
  }



  // Function 1: Basic SPD Test with DUT Recording Start
  std::unique_ptr<SequenceStep> BuildBasicSPDTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Basic SPD Test", machineOps);

    // 1. Start prompt
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "SPD Basic Test",
      "Starting SPD basic functionality test.\n\n"
      "This will test:\n"
      "• CV mode: 3.3V with current limit\n"
      "• CC mode: 0.3A with voltage limit\n"
      "• Basic measurements\n\n"
      "Click Yes to start.",
      promptUI));

    // 2. Get DUT serial number from user - always ask, no default
    sequence->AddOperation(std::make_shared<UserInputOperation>(
      "spd_dut_serial",
      "Enter SPD DUT Serial Number",
      "Please enter the DUT serial number for this test:",
      promptUI,
      "",  // No default - force user to enter
      true,  // Required
      300    // 5 minute timeout
    ));

    // 3. START DUT RECORDING
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "spd_dut_serial",
      [](MachineOperations& ops, const std::string& dutSerialNumber) {
        auto startRecording = std::make_shared<DUTStartRecordingOperation>(dutSerialNumber);
        return startRecording->Execute(ops);
      }
    ));

    // 4. Test CV Mode - 3.3V
    sequence->AddOperation(std::make_shared<SPD_SetConstantVoltageOperation>(
      3.3, 0.3, "SPD1", "CV_3V3_Test"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      true, "SPD1", 1000));

    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "CV_Mode_Reading"));

    // Record CV mode data with labels
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step1_cv_basic"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step1_cv_basic"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      false, "SPD1", 500));

    // 5. Test CC Mode
    sequence->AddOperation(std::make_shared<SPD_SetConstantCurrentOperation>(
      0.3, 3.3, "SPD1", "CC_300mA_Test"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      true, "SPD1", 1000));

    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "CC_Mode_Reading"));

    // Record CC mode data with labels
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step2_cc_basic"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step2_cc_basic"));

    sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(
      false, "SPD1", 500));

    // 6. Completion message - DUT recording continues
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Basic Test Complete",
      "Basic SPD test completed!\n\n"
      "✓ CV mode: 3.3V test\n"
      "✓ CC mode: 0.3A test\n"
      "✓ Basic measurements recorded\n\n"
      "DUT recording session is active.\n"
      "Ready for sweep tests.",
      promptUI));



  //// 5. END DUT RECORDING for this process
  //  sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));

    return sequence;
  }


  // Function 2: SPD Sweep Test (continues existing DUT session)
  std::unique_ptr<SequenceStep> BuildSweepSPDTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("SPD Sweep Test", machineOps);

    // 1. Start prompt - assumes DUT recording is already active
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "SPD Sweep Tests",
      "Starting SPD sweep tests.\n\n"
      "This will perform:\n"
      "• Voltage sweep: 0V to 3.3V (11 steps)\n"
      "• Current sweep: 0A to 0.3A (11 steps)\n\n"
      "DUT recording session will continue.\n"
      "Click Yes to start.",
      promptUI));

    // 2. Voltage sweep test
    sequence->AddOperation(std::make_shared<SPD_VoltageSweepOperation>(
      0.0, 3.3, 11, 0.3, "SPD1", 200.0, "Voltage_Sweep_0_to_3V3"));

    // Record post-sweep measurements
    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "Post_Voltage_Sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step3_voltage_sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step3_voltage_sweep"));

    // 3. Current sweep test
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Current Sweep Test",
      "Voltage sweep completed!\n\n"
      "Next: Current sweep (0A to 0.3A, 11 steps)\n"
      "Voltage limit: 3.3V, Step delay: 200ms\n\n"
      "Click Yes to continue.",
      promptUI));

    sequence->AddOperation(std::make_shared<SPD_CurrentSweepOperation>(
      0.0, 0.3, 11, 3.3, "SPD1", 200.0, "Current_Sweep_0_to_300mA"));

    // Record post-current-sweep measurements
    sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(
      "SPD1", true, "Post_Current_Sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Voltage", "step4_current_sweep"));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("SPD1-Current", "step4_current_sweep"));

    // 4. END DUT RECORDING AND EXPORT DATA
    sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));

    // 5. Final completion message with serial number
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "spd_dut_serial",
      [&promptUI](MachineOperations& ops, const std::string& dutSerialNumber) {
        auto completionPrompt = UserPromptOperation::CreateBasic(
          "SPD Test Complete",
          "SPD Power Supply test completed!\n\n"
          "✓ CV mode: 3.3V test\n"
          "✓ CC mode: 0.3A test\n"
          "✓ Voltage sweep: 0V to 3.3V (11 steps)\n"
          "✓ Current sweep: 0A to 0.3A (11 steps)\n"
          "✓ All measurements recorded\n"
          "✓ Outputs safely disabled\n"
          "✓ DUT data saved to dut_saved/" + dutSerialNumber + "\n\n"
          "Serial Number: " + dutSerialNumber + "\n"
          "Check logs and saved data files.",
          promptUI);
        return completionPrompt->Execute(ops);
      }
    ));

    // 6. Clear the stored input at the end
    sequence->AddOperation(std::make_shared<ClearUserInputOperation>("spd_dut_serial"));

    return sequence;
  }


  // Add this include at the top with the others
#include "PowerSupplyOperations.h"

// Add this new function in the namespace
  std::unique_ptr<SequenceStep> BuildAuroraSimplePowerSupplyTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Aurora Simple Power Supply Test", machineOps);

    // 1. Start prompt
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Simple Power Supply Test",
      "Starting Simple Power Supply test.\n\n"
      "This will:\n"
      "• Turn on power supply\n"
      "• Set voltage to 3.3V\n"
      "• Read voltage and current\n"
      "• Turn off power supply\n\n"
      "Device: PS1\n"
      "Click Yes to start.",
      promptUI));

    // 2. Set voltage to 3.3V with 0.5A current limit
    sequence->AddOperation(std::make_shared<PS_SetVoltageOperation>(
      3.3f, 0.5f, "KS-001", "Set_3V3"));

    // 3. Turn on power supply
    sequence->AddOperation(std::make_shared<PS_EnableOutputOperation>(
      true, "KS-001", 1000));  // 1 second delay after turn on

    // 4. Read measurements
    sequence->AddOperation(std::make_shared<PS_ReadMeasurementOperation>(
      "KS-001", true, "3V3_Measurement"));

    // 5. Wait a bit with output on
    sequence->AddOperation(std::make_shared<SMUWaitOperation>(2000, "Output stable at 3.3V"));

    // 6. Read measurements again
    sequence->AddOperation(std::make_shared<PS_ReadMeasurementOperation>(
      "KS-001", true, "3V3_Stable_Measurement"));

    // 7. Turn off power supply
    sequence->AddOperation(std::make_shared<PS_EnableOutputOperation>(
      false, "KS-001", 500));

    // 8. Completion message
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Simple Power Supply Test Complete",
      "Simple Power Supply test completed!\n\n"
      "✓ Power supply turned on\n"
      "✓ Voltage set to 3.3V\n"
      "✓ Measurements recorded\n"
      "✓ Power supply turned off\n\n"
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
  // Add wrapper function for registration
  std::unique_ptr<SequenceStep> WrapperAuroraSPD(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildAuroraSPDTest(machineOps, promptUI);
  }

  std::unique_ptr<SequenceStep> WrapperBasicSPD(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildBasicSPDTest(machineOps, promptUI);
	}

  std::unique_ptr<SequenceStep> WrapperSweepSPD(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildSweepSPDTest(machineOps, promptUI);
	}

  std::unique_ptr<SequenceStep> WrapperAuroraSimplePowerSupply(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    return BuildAuroraSimplePowerSupplyTest(machineOps, promptUI);
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

    registry.RegisterProcess(
      "Aurora_SPDTest",
      "Aurora_Core",
      "Aurora SPD Power Supply test - CV/CC modes",
      true,
      WrapperAuroraSPD
    );

    registry.RegisterProcess(
      "Aurora_BasicSPD",
      "Aurora_Core",
      "Aurora Basic SPD functionality test",
      true,
      WrapperBasicSPD
		);
    registry.RegisterProcess(
      "Aurora_SweepSPD",
      "Aurora_Core",
      "Aurora SPD sweep tests - voltage/current sweeps",
      true,
      WrapperSweepSPD
    );
    registry.RegisterProcess(
      "Aurora_SimplePowerSupply",
      "Aurora_Core",
      "Aurora Simple Power Supply test - basic on/off/measure",
      true,
      WrapperAuroraSimplePowerSupply
    );

    printf("Aurora Processes: Successfully Register Processes\n");
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