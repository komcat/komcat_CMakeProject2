// Core/uaa3_probing_sequence.cpp
#include "../uaa3_process_builders.h"
#include "UserInputOperations.h"


namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildProbingSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Probing", machineOps);

    // =========================================================
		// CLEAR STORED INPUT
    // =========================================================
		// Clear the stored input incase of re-run
    sequence->AddOperation(std::make_shared<ClearUserInputOperation>("dut_serial"));

    //CRITICAL DUT before anything else
    sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));

    //at this point unit is loaded and shall input serial number.
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
      "Enter device's Serial Number",             // title
      "Please enter the device's serial number:\n"
      "(Leave blank to use auto-generated, 5 minutes timeout)",  // prompt
      promptUI,
      defaultSerial,                         // default value (auto-generated)
      true,                                  // required (but has default)
      300                                    // 5 minute timeout
    ));


		// =========================================================
    // START DUT RECORDING - Use the user-provided serial number
    // =========================================================
    sequence->AddOperation(std::make_shared<UseStoredInputOperation>(
      "dut_serial",
      [](MachineOperations& ops, const std::string& dutSerialNumber) {
      // Create and execute DUTStartRecordingOperation with user input
      auto startRecording = std::make_shared<DUTStartRecordingOperation>(dutSerialNumber);
      return startRecording->Execute(ops);
    }
    ));






    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, true));  // Set output Vacuum_Base (pin 10)

    // 1. Move gantry to see sled position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4083"));

    // 2. Wait for user to confirm sled position
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Sled Position Check",
      "Please check sled position and confirm to continue",
      promptUI));

    // 1. Turn on TEC and set temperature
    sequence->AddOperation(std::make_shared<TECOnOperation>());
    sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(25.0f));

    //// 2. Wait for temperature to stabilize
    //sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
    //  25.0f, 1.0f, 5000)); //100ms

    // 3. Set laser current and turn on laser
    sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.250f)); // 150mA
    sequence->AddOperation(std::make_shared<LaserOnOperation>());

    // 4. Wait for processing time
    sequence->AddOperation(std::make_shared<WaitOperation>(500)); // 100ms

    // 5. Turn off laser and TEC
    //sequence->AddOperation(std::make_shared<LaserOffOperation>());
    //sequence->AddOperation(std::make_shared<TECOffOperation>());

    // 3. Move gantry to see PIC position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4107"));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5211"));

    // 4. Wait for user to confirm PIC position
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "PIC Position Check",
      "Please check PIC position and confirm to continue",
      promptUI));

    // 5. Move back to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); //safe

    // Record power and resistance at the end
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current"));
    //sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current2"));








    return sequence;
  }

}