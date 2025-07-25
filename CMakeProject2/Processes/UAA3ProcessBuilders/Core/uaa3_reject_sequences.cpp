// Utility/uaa3_reject_sequences.cpp
#include "../uaa3_process_builders.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> RejectLeftLensSequence_uaa3(MachineOperations& machineOps,
    UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Reject Left Lens Process", machineOps);

    // 1. Retract all pneumatics
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Pick_Up_Tool"));

    // 2. Move gantry to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position

    // 3. Move hex-left to reject lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject lens position

    // 4. Release left gripper (pin 0)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false)); // Clear output L_Gripper (pin 0)

    // 5. Wait for 3 seconds to ensure lens is dropped
    sequence->AddOperation(std::make_shared<WaitOperation>(3000)); // 3 seconds

    // 6. Move hex-left back to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480")); // Home position

    return sequence;
  }

  std::unique_ptr<SequenceStep> RejectRightLensSequence_uaa3(MachineOperations& machineOps,
    UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Reject Right Lens Process", machineOps);

    // 1. Retract all pneumatics
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Pick_Up_Tool"));

    // 2. Move gantry to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position

    // 3. Move hex-right to reject lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject lens position

    // 4. Release right gripper (pin 2)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false)); // Clear output R_Gripper (pin 2)

    // 5. Wait for 3 seconds to ensure lens is dropped
    sequence->AddOperation(std::make_shared<WaitOperation>(3000)); // 3 seconds

    // 6. Move hex-right back to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136")); // Home position

    return sequence;
  }

}