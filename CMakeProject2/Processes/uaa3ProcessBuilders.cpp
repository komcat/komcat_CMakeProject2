// uaa3ProcessBuilders.cpp - Modern process sequences using UserPromptUI
#include "uaa3ProcessBuilders.h"
#include "logger.h"
#include <iostream>

namespace UAA3ProcessBuilders {



  std::unique_ptr<SequenceStep> BuildInitializationSequence_uaa3(MachineOperations& machineOps) {
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

    sequence->AddOperation(std::make_shared<ClearOutputOperationDedicated>(
      "IOBottom", 10));  // Clear L_Gripper (pin 0)

    //// 9. Set output Vacuum_Base (pin 10)
    //sequence->AddOperation(std::make_shared<SetOutputOperation>(
    //	"IOBottom", 10, true));

    return sequence;
  }

  std::unique_ptr<SequenceStep> PickPlaceLeftLens_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
		auto sequence = std::make_unique<SequenceStep>("UAA3 Pick Place Left Lens", machineOps);



    // 2. Move hex-left to pick lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5647"));


    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4186"));	//see pick collimate lens


    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Check lens is ok",
      "Please check if the lens is placed at correct position.",
      promptUI));



    // 3. Set output L-gripper (pin 0) to grab the lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true));

    // 4. Start camera grabbing
    sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());

    // 5. Wait for camera to stabilize
    sequence->AddOperation(std::make_shared<WaitOperation>(500));  // 500ms delay

    // 6. Capture image
    sequence->AddOperation(std::make_shared<CaptureImageOperation>());




    // 5. Release the lens temporarily (clear output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false));

    // 6. Wait 1.5 seconds
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    // 7. Grip the lens again (set output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true, 500));


    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Check if lens is gripped OK?",
      "Please check if the lens is gripped and not tilted.",
      promptUI));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4137"));	//see collimate lens



    // 10. Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5662"));

    return sequence;
  }



  std::unique_ptr<SequenceStep> PickPlaceRightLens_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("UAA3 Pick Place Right Lens", machineOps);


    // Move hex-right to pick lens position (verify this is correct for RIGHT lens)
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5245"));

    // Move gantry to see the RIGHT lens pick position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4209"));  // Verify this is correct for focus lens


    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Check lens is ok",
      "Please check if the lens is placed at correct position.",
      promptUI));

    // Use R-gripper (pin 2) for RIGHT lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true));  // Set RIGHT gripper

    // 4. Start camera grabbing
    sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());

    // 5. Wait for camera to stabilize
    sequence->AddOperation(std::make_shared<WaitOperation>(500));  // 500ms delay

    // 6. Capture image
    sequence->AddOperation(std::make_shared<CaptureImageOperation>());

    // Release and re-grip cycle for the RIGHT lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false));  // Release RIGHT gripper
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));



    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true, 500));  // Re-grip RIGHT lens


    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Check if lens is gripped OK?",
      "Please check if the lens is gripped and not tilted.",
      promptUI));

    // Move gantry to see the RIGHT lens
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4156"));  // Verify this is for focus lens

    // Move to RIGHT lens placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5263"));  // Verify this is correct for RIGHT placement


    return sequence;
  }



  std::unique_ptr<SequenceStep> BuildModernProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Modern Probing", machineOps);

    // Set vacuum base
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, true));  // Set output Vacuum_Base (pin 10)

    // 1. Move gantry to see sled position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4083"));

    // 2. Modern user prompt for sled position
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Sled Position Check",
      "Please check sled position and confirm to continue.\n\n"
      "Verify:\n"
      "• Sled is properly positioned\n"
      "• No obstructions visible\n"
      "• Camera view is clear\n\n"
      "Click YES to continue or NO to abort.",
      promptUI));

    // 3. Turn on TEC and set temperature
    sequence->AddOperation(std::make_shared<TECOnOperation>());
    sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(25.0f));

    // 4. Wait for temperature to stabilize
    sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
      25.0f, 1.0f, 5000)); // Wait for 25°C ±1°C, timeout 5000ms

    // 5. Set laser current and turn on laser
    sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.250f)); // 250mA
    sequence->AddOperation(std::make_shared<LaserOnOperation>());

    // 6. Wait for processing time
    sequence->AddOperation(std::make_shared<WaitOperation>(500)); // 500ms

    // 7. Move gantry to see PIC position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4107"));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5211"));

    // 8. Modern user prompt for PIC position
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "PIC Position Check",
      "Please check PIC position and confirm to continue.\n\n"
      "Verify:\n"
      "• PIC is correctly positioned\n"
      "• Alignment looks good\n"
      "• No damage visible\n\n"
      "Click YES to continue or NO to abort.",
      promptUI));

    // 9. Log laser readings
    sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
      "", "Read laser current during probing"));
    sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
      "", "Read laser temperature during probing"));
    sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
      "GPIB-Current", "(GPIB-Current) Probing measurement"));

    // 10. Turn off laser and TEC
    sequence->AddOperation(std::make_shared<LaserOffOperation>());
    sequence->AddOperation(std::make_shared<TECOffOperation>());

    // 11. Return to safe positions
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027")); // Safe position

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136")); // Home position

    // 12. Clear vacuum base
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, false));  // Clear output Vacuum_Base (pin 10)

    return sequence;
  }


  // ============================================================================
  // UTILITY FUNCTIONS
  // ============================================================================

  std::unique_ptr<UserPromptOperation> CreateLaserSafetyPrompt(
    UserPromptUI& promptUI, float current, float temperature, int processingTimeMs) {

    return UserPromptOperation::CreateWithTimeout(
      "⚠️ LASER ACTIVATION SAFETY",
      "🔴 LASER SYSTEM ACTIVATION\n\n"
      "The system will now activate the laser with:\n"
      "• Target Temperature: " + std::to_string(temperature) + "°C\n"
      "• Laser Current: " + std::to_string(current * 1000) + "mA\n"
      "• Processing Time: " + std::to_string(processingTimeMs) + "ms\n\n"
      "⚠️ SAFETY REQUIREMENTS ⚠️\n"
      "• Laser safety glasses must be worn\n"
      "• Work area must be clear of personnel\n"
      "• Emergency stop must be accessible\n"
      "• All safety protocols must be followed\n\n"
      "🚨 Do not look directly at laser output 🚨\n\n"
      "Click YES only when all safety measures are in place.",
      promptUI, 45);
  }

  std::unique_ptr<UserPromptOperation> CreatePositionVerificationPrompt(
    const std::string& componentName, const std::string& details, UserPromptUI& promptUI) {

    return UserPromptOperation::CreateWithTimeout(
      componentName + " Position Check",
      "📍 POSITION VERIFICATION\n\n"
      "Component: " + componentName + "\n\n" +
      details + "\n\n"
      "Verify position is correct before proceeding.\n"
      "Manual adjustments may be made if necessary.\n\n"
      "Click YES when position is verified.",
      promptUI, 90);
  }

  std::unique_ptr<UserPromptOperation> CreateCompletionPrompt(
    const std::string& sequenceName, const std::string& results, UserPromptUI& promptUI) {

    return UserPromptOperation::CreateWithTimeout(
      sequenceName + " Complete",
      "✅ SEQUENCE COMPLETED\n\n"
      "Sequence: " + sequenceName + "\n\n" +
      results + "\n\n"
      "All operations completed successfully.\n"
      "System returning to safe state.\n\n"
      "Click YES to acknowledge completion.",
      promptUI, 30);
  }

  void DebugPrintModernSequence(const std::string& name, const std::unique_ptr<SequenceStep>& sequence) {
    Logger* logger = Logger::GetInstance();

    logger->LogProcess("=== UAA3 DEBUG SEQUENCE: " + name + " ===");
    logger->LogProcess("Sequence type: Modern (UserPromptUI-based)");
    logger->LogProcess("Operation count: " + std::to_string(sequence->GetOperations().size()));

    int promptCount = 0;
    int i = 1;
    for (const auto& op : sequence->GetOperations()) {
      std::string desc = op->GetDescription();
      if (desc.find("prompt") != std::string::npos || desc.find("Prompt") != std::string::npos) {
        promptCount++;
        logger->LogProcess(std::to_string(i++) + ": [PROMPT] " + desc);
      }
      else {
        logger->LogProcess(std::to_string(i++) + ": " + desc);
      }
    }

    logger->LogProcess("Total user prompts: " + std::to_string(promptCount));
    logger->LogProcess("=== END UAA3 DEBUG SEQUENCE ===");
  }

} // namespace UAA3ProcessBuilders