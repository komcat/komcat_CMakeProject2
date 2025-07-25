// uaa3_utility_functions.cpp - Utility functions for UAA3 ProcessBuilders
#include "uaa3_process_builders.h"
#include "logger.h"
#include <iostream>

namespace UAA3ProcessBuilders {

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