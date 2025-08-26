// Dispensing/uaa3_dispense_epoxy.cpp
#include "../uaa3_process_builders.h"
#include "ManualAdjustmentOperation.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildDispenseEpoxy1Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Dispense Epoxy at Location 1", machineOps);

    // 1. Retract dispenser head first for safety
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 2. Move to safe positions
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_3618")); // Safe Left position

    // 3. Clear any old stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    // 4. Move camera to nominal dispense position
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "_cam_dispense_1"));

    // 5. Manual adjustment for actual part position
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Adjust Dispense Position",                 // title
      "Position the camera crosshair exactly where epoxy should be dispensed.\n"
      "Adjust for any part variation from the nominal position.",  // instructions
      promptUI,                                    // UserPromptUI&
      true,                                        // enableX - allow X adjustment
      true,                                        // enableY - allow Y adjustment
      false                                        // enableZ - keep at camera height
    ));

    // 5a. Save the adjusted camera position back to config
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "_cam_dispense_1"));

    // 6. Capture adjusted camera position for needle offset calculation
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "adjusted_camera_pos"));

    // 7. Apply camera to needle offset and move to dispense position
    sequence->AddOperation(std::make_shared<ApplyNeedleOffsetAndMoveOperation>(
      "gantry-main", "adjusted_camera_pos"));



    // 9. Store current speed for later restoration
    sequence->AddOperation(std::make_shared<StoreCurrentSpeedOperation>(
      "gantry-main", "dispense_original_speed"));

    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main", "dispense1safe", 5000));

    // 10. Set slow speed for precise dispensing (0.5 mm/s)
    sequence->AddOperation(std::make_shared<SetDeviceSpeedOperation>("gantry-main", 5));

    // 11. Move to the calibrated dispense position (now has updated XY with calibrated Z)
    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main", "dispense1",5000));

    // 8. Extend dispenser head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // Wait for dispenser to extend
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // 12. Optional: Final Z adjustment to account for part height variation
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Verify Dispense Height",                   // title
      "Adjust Z height if needed for this specific part.\n"
      "Tip should be at optimal dispense distance.",  // instructions
      promptUI,                                    // UserPromptUI&
      false,                                       // enableX - no X adjustment
      false,                                       // enableY - no Y adjustment
      true                                         // enableZ - allow Z adjustment
    ));

    // 12a. Save the adjusted dispense position (including Z) back to config
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "dispense1"));

    //// 12b. Update the safe position based on new dispense position
    //sequence->AddOperation(std::make_shared<CreateSafeDispensePositionOperation>(
    //  "gantry-main", "dispense1", "dispense1safe", -0.5));

    // 13. Wait 1 sec before dispensing
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 14. Dispense epoxy
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, true)); // Dispenser_Shot pin 15
    sequence->AddOperation(std::make_shared<WaitOperation>(50));
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, false)); // Clear Dispenser_Shot
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 15. Move Z up to safe height first (relative move)
    sequence->AddOperation(std::make_shared<MoveRelativeOperation>("gantry-main", "Z", -0.5)); // Move up 0.5mm

    // 16. Restore original speed
    sequence->AddOperation(std::make_shared<RestoreStoredSpeedOperation>(
      "gantry-main", "dispense_original_speed"));

    // 17. Retract dispenser head
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 18. Move to safe positions
    // Move to safe_left first to avoid directly to safe
    sequence->AddOperationWithFallback(
      std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618"),
      std::make_shared<MoveToPointNameOperation>("gantry-main", "safe_left")
    );
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // Move to safe position
    sequence->AddOperationWithFallback(
      std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027"),
      std::make_shared<MoveToPointNameOperation>("gantry-main", "safe")
    );

    // Move hex stages to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    // 19. Clear stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildDispenseEpoxy2Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("Dispense Epoxy at Location 2", machineOps);

    // 1. Retract dispenser head first for safety
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 2. Move to safe positions
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_3618")); // Safe Left position

    // 3. Clear any old stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    // 4. Move camera to nominal dispense position
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "_cam_dispense_2"));

    // 5. Manual adjustment for actual part position
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Adjust Dispense Position",                 // title
      "Position the camera crosshair exactly where epoxy should be dispensed.\n"
      "Adjust for any part variation from the nominal position.",  // instructions
      promptUI,                                    // UserPromptUI&
      true,                                        // enableX - allow X adjustment
      true,                                        // enableY - allow Y adjustment
      false                                        // enableZ - keep at camera height
    ));

    // 5a. Save the adjusted camera position back to config
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "_cam_dispense_2"));

    // 6. Capture adjusted camera position for needle offset calculation
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "adjusted_camera_pos"));

    // 7. Apply camera to needle offset and move to dispense position
    sequence->AddOperation(std::make_shared<ApplyNeedleOffsetAndMoveOperation>(
      "gantry-main", "adjusted_camera_pos"));



    // 9. Store current speed for later restoration
    sequence->AddOperation(std::make_shared<StoreCurrentSpeedOperation>(
      "gantry-main", "dispense_original_speed"));


    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main","dispense2safe",5000));

    // 10. Set slow speed for precise dispensing (0.5 mm/s)
    sequence->AddOperation(std::make_shared<SetDeviceSpeedOperation>("gantry-main", 5));



    // 11. Move to the calibrated dispense position (now has updated XY with calibrated Z)
    sequence->AddOperation(std::make_shared<BlockingMoveToPointNameOperation>(
      "gantry-main", "dispense2",5000));

    // 8. Extend dispenser head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // Wait for dispenser to extend
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // 12. Optional: Final Z adjustment to account for part height variation
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Verify Dispense Height",                   // title
      "Adjust Z height if needed for this specific part.\n"
      "Tip should be at optimal dispense distance.",  // instructions
      promptUI,                                    // UserPromptUI&
      false,                                       // enableX - no X adjustment
      false,                                       // enableY - no Y adjustment
      true                                         // enableZ - allow Z adjustment
    ));

    // 12a. Save the adjusted dispense position (including Z) back to config
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "dispense2"));

    //// 12b. Update the safe position based on new dispense position
    //sequence->AddOperation(std::make_shared<CreateSafeDispensePositionOperation>(
    //  "gantry-main", "dispense2", "dispense2safe", -0.5));

    // 13. Wait 1 sec before dispensing
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 14. Dispense epoxy
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, true)); // Dispenser_Shot pin 15
    sequence->AddOperation(std::make_shared<WaitOperation>(50));
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 15, false)); // Clear Dispenser_Shot
    sequence->AddOperation(std::make_shared<WaitOperation>(1000));

    // 15. Move Z up to safe height first (relative move)
    sequence->AddOperation(std::make_shared<MoveRelativeOperation>("gantry-main", "Z", -0.5)); // Move up 0.5mm

    // 16. Restore original speed
    sequence->AddOperation(std::make_shared<RestoreStoredSpeedOperation>(
      "gantry-main", "dispense_original_speed"));

    // 17. Retract dispenser head
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 18. Move to safe positions
    // Move to safe_left first to avoid directly to safe
    sequence->AddOperationWithFallback(
      std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_3618"),
      std::make_shared<MoveToPointNameOperation>("gantry-main", "safe_left")
    );
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // Move to safe position
    sequence->AddOperationWithFallback(
      std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027"),
      std::make_shared<MoveToPointNameOperation>("gantry-main", "safe")
    );

    // Move hex stages to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    // 19. Clear stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    return sequence;
  }

}