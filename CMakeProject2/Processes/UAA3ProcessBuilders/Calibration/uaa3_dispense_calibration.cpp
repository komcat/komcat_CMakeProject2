// Calibration/uaa3_dispense_calibration.cpp
#include "../uaa3_process_builders.h"
#include "ManualAdjustmentOperation.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildDispenseCalibrationSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Dispense Calibration - Location 1", machineOps);

    // Always move to safe positions first
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Clear any old stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    // 0. Display current needle offset (needed for calculations)
    sequence->AddOperation(std::make_shared<DisplayNeedleOffsetOperation>());

    // 1. Move gantry-main to _cam_dispense_1 location
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "_cam_dispense_1"));

    // 2. Manual adjustment for dispense position (replaces user prompt)
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Dispense Position Setup",                  // title
      "Position the camera crosshair exactly where you want to dispense.\n"
      "Use the adjustment controls to fine-tune the position.",  // instructions
      promptUI,                                    // UserPromptUI& (REQUIRED - you missed this)
      true,                                        // enableX
      true,                                        // enableY
      true                                         // enableZ
    ));

    // 3. Save current location to _cam_dispense_1
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "_cam_dispense_1"));

    // Store current position for offset calculation
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "camera_position"));

    // 4. Apply camera to needle offset and move to dispense position
    sequence->AddOperation(std::make_shared<ApplyNeedleOffsetAndMoveOperation>(
      "gantry-main", "camera_position"));
    sequence->AddOperation(std::make_shared<WaitOperation>(3000));
    // 5. Extend dispenser_head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // Wait for dispenser to extend
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // 6. Manual adjustment for dispense height
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Dispense Height Adjustment",               // title
      "Adjust the dispenser tip to the correct height for dispensing.\n"
      "Use Z-axis controls to set the proper tip-to-surface distance.",  // instructions
      promptUI,                                    // UserPromptUI&
      false,                                       // enableX - disabled for Z-only adjustment
      false,                                       // enableY - disabled for Z-only adjustment  
      true                                         // enableZ - enabled for height adjustment
    ));

    // 7. Save current location to dispense1
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "dispense1"));

    // 8. Create and save dispense1safe (dispense1's Z - 0.5mm)
    sequence->AddOperation(std::make_shared<CreateSafeDispensePositionOperation>(
      "gantry-main", "dispense1", "dispense1safe", -0.5));

    // Retract dispenser
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "dispense1safe")); // Safe Left position
    sequence->AddOperation(std::make_shared<WaitOperation>(500));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe Left position

    // 2. Move hex-left to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));

    // 3. Move hex-right to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));
    // Clear stored positions at end
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildDispenseCalibration2Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Dispense Calibration - Location 2", machineOps);

    // Always move to safe positions first
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // Reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // Reject position

    // Clear any old stored positions
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    // 0. Display current needle offset (needed for calculations)
    sequence->AddOperation(std::make_shared<DisplayNeedleOffsetOperation>());

    // 1. Move gantry-main to _cam_dispense_2 location
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "_cam_dispense_2"));

    // 2. Manual adjustment for dispense position (replaces user prompt)
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Dispense Position Setup",                  // title
      "Position the camera crosshair exactly where you want to dispense.\n"
      "Use the adjustment controls to fine-tune the position.",  // instructions
      promptUI,                                    // UserPromptUI& (REQUIRED - you missed this)
      true,                                        // enableX
      true,                                        // enableY
      true                                         // enableZ
    ));

    // 3. Save current location to _cam_dispense_2
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "_cam_dispense_2"));

    // Store current position for offset calculation
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "camera_position"));

    // 4. Apply camera to needle offset and move to dispense position
    sequence->AddOperation(std::make_shared<ApplyNeedleOffsetAndMoveOperation>(
      "gantry-main", "camera_position"));
    sequence->AddOperation(std::make_shared<WaitOperation>(3000));
    // 5. Extend dispenser_head
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // Wait for dispenser to extend
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // 6. Manual adjustment for dispense height
    sequence->AddOperation(std::make_shared<ManualAdjustmentOperation>(
      "gantry-main",                              // axisSystem
      "Dispense Height Adjustment",               // title
      "Adjust the dispenser tip to the correct height for dispensing.\n"
      "Use Z-axis controls to set the proper tip-to-surface distance.",  // instructions
      promptUI,                                    // UserPromptUI&
      false,                                       // enableX - disabled for Z-only adjustment
      false,                                       // enableY - disabled for Z-only adjustment  
      true                                         // enableZ - enabled for height adjustment
    ));

    // 7. Save current location to dispense2
    sequence->AddOperation(std::make_shared<SaveCurrentPositionToConfigOperation>(
      "gantry-main", "dispense2"));

    // 8. Create and save dispense2safe (dispense2's Z - 0.5mm)
    sequence->AddOperation(std::make_shared<CreateSafeDispensePositionOperation>(
      "gantry-main", "dispense2", "dispense2safe", -0.5));

    // Retract dispenser
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // Move to safe position
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "dispense2safe")); // Safe Left position

    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe Left position

    // 2. Move hex-left to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5480"));

    // 3. Move hex-right to home position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    // Clear stored positions at end
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    return sequence;
  }

}