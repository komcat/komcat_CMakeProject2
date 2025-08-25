// Core/uaa3_pick_place_sequences.cpp
#include "../uaa3_process_builders.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildInitializationSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Initialization", machineOps);
    // 6. Retract UV_Head pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "UV_Head"));

    // 7. Retract Dispenser_Head pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "Dispenser_Head"));

    // 8. Retract Pick_Up_Tool pneumatic
    sequence->AddOperation(std::make_shared<RetractSlideOperation>(
      "Pick_Up_Tool"));

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



    // 9. Clear dedicated output (pin 10) - Vacuum_Base
    sequence->AddOperation(std::make_shared<ClearOutputOperationDedicated>(
      "IOBottom", 10));  // Clear Vacuum_Base (pin 10)

    // Optional: User confirmation after initialization (UAA3 enhancement)
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Initialization Complete",
      "System initialization completed successfully.\n\n"
      "All devices moved to safe positions:\n"
      "• Gantry: Safe position\n"
      "• Hex-left: Home position\n"
      "• Hex-right: Home position\n"
      "• All grippers: Released\n"
      "• All pneumatics: Retracted\n\n"
      "Click YES to continue with operations.",
      promptUI));

    //// Optional: Set output Vacuum_Base (pin 10) - commented out as in original
    //sequence->AddOperation(std::make_shared<SetOutputOperation>(
    //  "IOBottom", 10, true));  // Set output Vacuum_Base (pin 10)

    return sequence;
  }

}