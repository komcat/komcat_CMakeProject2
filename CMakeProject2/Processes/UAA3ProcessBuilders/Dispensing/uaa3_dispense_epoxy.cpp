// Dispensing/uaa3_dispense_epoxy.cpp
#include "../uaa3_process_builders.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildDispenseEpoxy1Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Dispense Epoxy at Location 1", machineOps);

    // 1. Retract dispenser head first for safety
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 2. Move gantry-main to safe node 4027
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618")); // Safe Left position

    // 3. Move to safe dispense position (blocking)
    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main", "dispense1safe", 30000)); // 30 second timeout

    // 4. Extend dispenser head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // 5. Store current speed for later restoration
    sequence->AddOperation(std::make_shared<StoreCurrentSpeedOperation>("gantry-main", "dispense_original_speed"));

    // 6. Set slow speed for precise dispensing (0.5 mm/s)
    sequence->AddOperation(std::make_shared<SetDeviceSpeedOperation>("gantry-main", 0.5));

    // 7. Move to dispense position at slow speed
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>("gantry-main", "dispense1"));

    // 8. Wait 1 sec
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 9. Dispense epoxy
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, true)); // Dispenser_Shot pin 15
    sequence->AddOperation(std::make_shared<WaitOperation>(50));
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, false)); // Clear Dispenser_Shot
    sequence->AddOperation(std::make_shared<WaitOperation>(50));

    // 10. Move back to safe position (still at slow speed)
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>("gantry-main", "dispense1safe"));

    // 11. Restore original speed
    sequence->AddOperation(std::make_shared<RestoreStoredSpeedOperation>("gantry-main", "dispense_original_speed"));

    // 12. Retract dispenser head (non-blocking)
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618")); // Safe Left position
    sequence->AddOperation(std::make_shared<WaitOperation>(500));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027")); // Safe Left position

    // Move hex stages to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-left", "Process_Flow", "node_5480"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-right", "Process_Flow", "node_5136"));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildDispenseEpoxy2Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Dispense Epoxy at Location 2", machineOps);

    // 1. Retract dispenser head first for safety
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 2. Move gantry-main to safe node 4027
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618")); // Safe Left position

    // 3. Move to safe dispense position (blocking)
    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main", "dispense2safe", 30000)); // 30 second timeout

    // 4. Extend dispenser head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // 5. Store current speed for later restoration
    sequence->AddOperation(std::make_shared<StoreCurrentSpeedOperation>("gantry-main", "dispense2_original_speed"));

    // 6. Set slow speed for precise dispensing (0.5 mm/s)
    sequence->AddOperation(std::make_shared<SetDeviceSpeedOperation>("gantry-main", 0.5));

    // 7. Move to dispense position at slow speed
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>("gantry-main", "dispense2"));

    // 8. Wait 1 sec
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 9. Dispense epoxy
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, true)); // Dispenser_Shot pin 15
    sequence->AddOperation(std::make_shared<WaitOperation>(50));
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, false)); // Clear Dispenser_Shot
    sequence->AddOperation(std::make_shared<WaitOperation>(50));

    // 10. Move back to safe position (still at slow speed)
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>("gantry-main", "dispense2safe"));

    // 11. Restore original speed
    sequence->AddOperation(std::make_shared<RestoreStoredSpeedOperation>("gantry-main", "dispense2_original_speed"));

    // 12. Retract dispenser head (non-blocking)
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618")); // Safe Left position
    sequence->AddOperation(std::make_shared<WaitOperation>(500));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027")); // Safe Left position

    // Move hex stages to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-left", "Process_Flow", "node_5480"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-right", "Process_Flow", "node_5136"));

    return sequence;
  }

}