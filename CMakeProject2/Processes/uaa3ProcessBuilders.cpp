// uaa3ProcessBuilders.cpp - Modern process sequences using UserPromptUI
#include "uaa3ProcessBuilders.h"
#include "logger.h"
#include <iostream>

namespace UAA3ProcessBuilders {

  // ============================================================================
  // MODERN PROBING SEQUENCES
  // ============================================================================

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

  std::unique_ptr<SequenceStep> BuildEnhancedProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Enhanced Probing", machineOps);

    // Pre-sequence safety check
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "UAA3 Probing Sequence Start",
      "🔬 UAA3 PROBING SEQUENCE\n\n"
      "This sequence will perform:\n"
      "1. Sled position verification\n"
      "2. Laser and TEC activation (25°C, 250mA)\n"
      "3. PIC position verification\n"
      "4. Optical measurements\n"
      "5. System shutdown and safe return\n\n"
      "⚠️ SAFETY REQUIREMENTS ⚠️\n"
      "• Ensure work area is clear\n"
      "• Verify laser safety protocols\n"
      "• Confirm all components are properly seated\n\n"
      "Estimated time: 2-3 minutes",
      promptUI, 60)); // 60 second timeout

    // Set vacuum base
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, true));

    // Move to sled check position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4083"));

    // Enhanced sled position check
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "Sled Position Verification",
      "📍 SLED POSITION CHECK\n\n"
      "Camera is positioned to view the sled component.\n\n"
      "Verification Checklist:\n"
      "✓ Sled is properly seated in fixture\n"
      "✓ No foreign objects or debris visible\n"
      "✓ Component alignment appears correct\n"
      "✓ Camera view is clear and unobstructed\n\n"
      "💡 TIP: Manual adjustments can be made if needed.\n"
      "Take time to ensure proper positioning.\n\n"
      "Click YES when ready to proceed with laser activation.",
      promptUI, 120)); // 2 minute timeout for manual adjustments

    // Laser activation warning and setup
    sequence->AddOperation(CreateLaserSafetyPrompt(promptUI, 0.250f, 25.0f, 500));

    // Activate TEC and laser
    sequence->AddOperation(std::make_shared<TECOnOperation>());
    sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(25.0f));

    sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
      25.0f, 1.0f, 5000));

    sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.250f));
    sequence->AddOperation(std::make_shared<LaserOnOperation>());

    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    // Move to PIC position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4107"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5211"));

    // Enhanced PIC position check
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "PIC Position Verification",
      "🔍 PIC COMPONENT CHECK\n\n"
      "Camera is now positioned to view the PIC (Photonic Integrated Circuit).\n\n"
      "Critical Verification Points:\n"
      "✓ PIC is visible and properly centered\n"
      "✓ No physical damage or contamination\n"
      "✓ Alignment with mounting fixture is correct\n"
      "✓ Optical connections appear intact\n"
      "✓ No loose particles or debris\n\n"
      "🎯 This is the final visual check before measurements.\n"
      "Ensure everything looks correct before proceeding.\n\n"
      "Click YES to proceed with optical data collection.",
      promptUI, 120));

    // Data collection with progress indication
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "Data Collection in Progress",
      "📊 COLLECTING OPTICAL DATA\n\n"
      "The system is now collecting measurements:\n"
      "• Laser current readings\n"
      "• Temperature monitoring\n"
      "• GPIB current measurements\n\n"
      "⏱️ Please wait while data is collected...\n"
      "This process will complete automatically.\n\n"
      "Click YES to acknowledge.",
      promptUI, 15));

    sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
      "", "UAA3 Probing: Laser current measurement"));
    sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
      "", "UAA3 Probing: Laser temperature measurement"));
    sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
      "GPIB-Current", "UAA3 Probing: GPIB current measurement"));

    // Completion notification with results summary
    sequence->AddOperation(CreateCompletionPrompt(
      "UAA3 Probing",
      "Data collection completed successfully.\n"
      "Laser and TEC systems will now be safely shut down.",
      promptUI));

    // Shutdown and return to safe positions
    sequence->AddOperation(std::make_shared<LaserOffOperation>());
    sequence->AddOperation(std::make_shared<TECOffOperation>());

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4027"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5136"));

    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, false));

    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildQuickProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Quick Probing", machineOps);

    // Minimal startup prompt
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "Quick Probing Start",
      "🚀 QUICK PROBING MODE\n\n"
      "Minimal user interaction mode.\n"
      "Automated positioning and measurements.\n\n"
      "Click YES to start automated sequence.",
      promptUI, 30));

    // Quick sequence execution
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 10, true));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4083"));

    // Quick position check
    sequence->AddOperation(UserPromptOperation::CreateWithTimeout(
      "Position Check",
      "Verify sled position - Click YES to continue.",
      promptUI, 15));

    sequence->AddOperation(std::make_shared<TECOnOperation>());
    sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(25.0f));
    sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(25.0f, 1.0f, 5000));
    sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.250f));
    sequence->AddOperation(std::make_shared<LaserOnOperation>());
    sequence->AddOperation(std::make_shared<WaitOperation>(500));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4107"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-right", "Process_Flow", "node_5211"));

    // Quick measurements
    sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>("", "Quick probing: Current"));
    sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>("", "Quick probing: Temperature"));
    sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>("GPIB-Current", "Quick probing: GPIB"));

    // Quick shutdown
    sequence->AddOperation(std::make_shared<LaserOffOperation>());
    sequence->AddOperation(std::make_shared<TECOffOperation>());
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027"));
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>("hex-right", "Process_Flow", "node_5136"));
    sequence->AddOperation(std::make_shared<SetOutputOperation>("IOBottom", 10, false));

    return sequence;
  }

  // ============================================================================
  // MODERN CALIBRATION SEQUENCES
  // ============================================================================

  std::unique_ptr<SequenceStep> BuildModernNeedleCalibrationSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Modern Needle Calibration", machineOps);

    // Implementation placeholder - can be expanded based on existing needle calibration
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Needle Calibration Start",
      "🎯 NEEDLE CALIBRATION SEQUENCE\n\n"
      "This sequence will calibrate needle positioning.\n"
      "Please ensure the calibration target is properly positioned.\n\n"
      "Click YES to begin calibration.",
      promptUI));

    // Add needle calibration operations here
    // (This would be implemented based on your existing needle calibration logic)

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