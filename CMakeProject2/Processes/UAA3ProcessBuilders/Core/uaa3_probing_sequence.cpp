// Core/uaa3_probing_sequence.cpp
#include "../uaa3_process_builders.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildProbingSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Probing", machineOps);

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

    // 2. Wait for temperature to stabilize
    sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
      25.0f, 1.0f, 5000)); //100ms

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

    return sequence;
  }

}