// Calibration/uaa3_needle_calibration.cpp
#include "../uaa3_process_builders.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildNeedleXYCalibrationSequenceEnhanced_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Enhanced Needle XY Calibration", machineOps);

    // Always move to safe
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5531")); // reject position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5190")); // reject position

    // Clear any old stored positions at start
    sequence->AddOperation(std::make_shared<ClearStoredPositionsOperation>());

    // 0. Display current needle offset (if any)
    sequence->AddOperation(std::make_shared<DisplayNeedleOffsetOperation>());

    // 1. Move gantry-main to see dot position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "SeeCaldot"));

    // 2. Prompt user to confirm calibration
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Calibration Start",
      "Ready to start needle XY calibration? Make sure workspace is clear.",
      promptUI));

    // 5. Move gantry-main to needle caldot position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "caldot"));

    // 5.1 offset back -2mm
    sequence->AddOperation(std::make_shared<MoveRelativeAxisOperation>(
      "gantry-main", "Z", -2));

    // 7. Extend dispenser
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("Dispenser_Head"));

    // 8. Wait for dispenser to extend
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // 6. Prompt user before dispensing
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Nozzle Position",
      "Adjust tip of nozzle to touch the surface, continue when ready?",
      promptUI));

    // save current position of gantry to caldot.
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "caldot"));
    // 4. Log the reference position
    sequence->AddOperation(std::make_shared<LogPositionDistanceOperation>(
      "gantry-main", "caldot", "Reference Z height captured"));

    // 9. Set output IOBottom dispenser (activate)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 15, true)); // Assuming pin 15 for dispenser

    // 10. Wait for dispensing
    sequence->AddOperation(std::make_shared<WaitOperation>(100));

    // 11. Clear output IOBottom dispenser (deactivate)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 15, false));

    // 12. Wait before retracting
    sequence->AddOperation(std::make_shared<WaitOperation>(200));

    // 13. Retract dispenser
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));

    // 3. Store gantry-main position as pos1 (reference position)
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "pos1"));

    // 4. Log the reference position
    sequence->AddOperation(std::make_shared<LogPositionDistanceOperation>(
      "gantry-main", "pos1", "Reference position captured"));

    // 14. Move gantry-main back to see dot position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "SeeCaldot")); // camera to see node_caldot

    // 15. Prompt user to adjust crosshair to dot center
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Crosshair Adjustment",
      "Use the camera view to center the crosshair on the dispensed dot, then confirm.",
      promptUI));

    // 16. Store gantry-main position as pos2 (adjusted position)
    sequence->AddOperation(std::make_shared<CapturePositionOperation>(
      "gantry-main", "pos2"));

    // 17. Calculate and display offset
    sequence->AddOperation(std::make_shared<CalculateNeedleOffsetOperation>(
      "gantry-main", "pos1", "pos2"));

    // 18. Log the movement distance for verification
    sequence->AddOperation(std::make_shared<LogPositionDistanceOperation>(
      "gantry-main", "pos1", "Total adjustment distance from reference"));

    // 19. Prompt user to save to config
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Save Configuration",
      "Save the calculated needle offset to configuration file?",
      promptUI));

    // 20. Save needle offset to camera_to_object_offset.json
    sequence->AddOperation(std::make_shared<SaveNeedleOffsetOperation>(
      "gantry-main", "pos1", "pos2"));

    // 21. Move back to safe position
    sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
      "gantry-main", "g_safe")); // Safe position

    // 21. Move back to safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position

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