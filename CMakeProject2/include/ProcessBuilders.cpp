#include "ProcessBuilders.h"

namespace ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildInitializationSequence(MachineOperations& machineOps) {
    auto sequence = std::make_unique<SequenceStep>("Initialization", machineOps);

    // 1. Move gantry-main to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027"));

    // 2. Move hex-left to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));

    // 3. Move hex-right to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    // 4. Clear output L_Gripper (pin 0)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false));

    // 5. Clear output R_Gripper (pin 2)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false));

    // 6. Retract UV_Head pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "UV_Head"));

    // 7. Retract Dispenser_Head pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "Dispenser_Head"));

    // 8. Retract Pick_Up_Tool pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "Pick_Up_Tool"));

    // 9. Set output Vacuum_Base (pin 10)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, true));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildProbingSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager) {

    auto sequence = std::make_unique<SequenceStep>("Probing", machineOps);

    // 1. Move gantry to see sled position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4083"));

    // 2. Wait for user to confirm sled position
    sequence->AddOperation(std::make_shared<UserConfirmOperation>(
      "Please check sled position and confirm to continue", uiManager));


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
    sequence->AddOperation(std::make_shared<WaitOperation>(5000)); // 100ms

    // 5. Turn off laser and TEC
    //sequence->AddOperation(std::make_shared<LaserOffOperation>());
    //sequence->AddOperation(std::make_shared<TECOffOperation>());

    // 3. Move gantry to see PIC position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4107"));

    // 4. Wait for user to confirm PIC position
    sequence->AddOperation(std::make_shared<UserConfirmOperation>(
      "Please check PIC position and confirm to continue", uiManager));

    // 5. Move back to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027"));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Left Lens", machineOps);

    // 1. Move hex-left to pick lens approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5557"));

    // 2. Move hex-left to pick lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5647"));

    // 3. Set output L-gripper (pin 0) to grab the lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true));

    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(std::make_shared<UserConfirmOperation>(
      "Confirm left lens is successfully gripped", uiManager));

    // 5. Release the lens temporarily (clear output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false));

    // 6. Wait 1.5 seconds
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    // 7. Grip the lens again (set output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true));

    // 8. Move back to approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5557"));

    // 9. Move to placement approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5620"));

    // 10. Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5662"));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Right Lens", machineOps);

    // 1. Move hex-right to pick lens approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5211"));

    // 2. Move hex-right to pick lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5245"));

    // 3. Set output R-gripper (pin 2) to grab the lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true));

    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(std::make_shared<UserConfirmOperation>(
      "Confirm right lens is successfully gripped", uiManager));

    // 5. Release the lens temporarily (clear output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false));

    // 6. Wait 1.5 seconds
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    // 7. Grip the lens again (set output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true));

    // 8. Move back to approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5211"));

    // 9. Move to placement approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5235"));

    // 10. Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5263"));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildUVCuringSequence(MachineOperations& machineOps) {
    auto sequence = std::make_unique<SequenceStep>("UV Curing", machineOps);

    // 1. Move gantry to UV position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4426"));

    // 2. Extend UV_Head pneumatic
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("UV_Head"));

    // 3. Toggle UV_PLC1 (pin 14) - Clear output
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 14, false));

    // 4. Wait 50ms
    sequence->AddOperation(std::make_shared<WaitOperation>(50));

    // 5. Toggle UV_PLC1 (pin 14) - Set output
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 14, true));

    // 6. Wait 150ms
    sequence->AddOperation(std::make_shared<WaitOperation>(150));

    // 7. Wait for curing (200 seconds)
    sequence->AddOperation(std::make_shared<WaitOperation>(210000));

    // 8. Retract UV_Head
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));

    // 9. Clear left and right grippers
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false)); // Left gripper

    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false)); // Right gripper

    // 6. Wait 150ms
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    // 10. Move hex-left to approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5620"));

    // 11. Move hex-right to approach position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5235"));

    // 12. Move hex-left to home
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));

    // 13. Move hex-right to home
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    // 14. Move gantry to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027"));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildCompleteProcessSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager) {

    auto sequence = std::make_unique<SequenceStep>("Complete Process", machineOps);

    // Add all sub-sequences as steps in the complete process

    // 1. Initialization
    auto initSequence = BuildInitializationSequence(machineOps);
    for (const auto& op : initSequence->GetOperations()) {
      sequence->AddOperation(op);
    }

    // 2. Probing
    auto probingSequence = BuildProbingSequence(machineOps, uiManager);
    for (const auto& op : probingSequence->GetOperations()) {
      sequence->AddOperation(op);
    }

    // 3. Pick and Place Left Lens
    auto leftLensSequence = BuildPickPlaceLeftLensSequence(machineOps, uiManager);
    for (const auto& op : leftLensSequence->GetOperations()) {
      sequence->AddOperation(op);
    }

    // 4. Pick and Place Right Lens
    auto rightLensSequence = BuildPickPlaceRightLensSequence(machineOps, uiManager);
    for (const auto& op : rightLensSequence->GetOperations()) {
      sequence->AddOperation(op);
    }

    // 5. UV Curing
    auto uvCuringSequence = BuildUVCuringSequence(machineOps);
    for (const auto& op : uvCuringSequence->GetOperations()) {
      sequence->AddOperation(op);
    }

    return sequence;
  }

} // namespace ProcessBuilders