#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>

// UAA3 Process Builders - Updated sequences using UserPromptUI
// This file contains modernized versions of process sequences that use
// the new UserPromptUI system instead of the legacy UserInteractionManager
namespace UAA3ProcessBuilders {


  /// <summary>
  /// Modern probing sequence using UserPromptUI instead of legacy UserConfirmOperation
  /// Includes basic sled and PIC position checks with laser activation
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  std::unique_ptr<SequenceStep> PickPlaceLeftLens_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  std::unique_ptr<SequenceStep> PickPlaceRightLens_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Create a standardized safety prompt for laser operations
  /// Reusable prompt for any sequence that involves laser activation
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreateLaserSafetyPrompt(
    UserPromptUI& promptUI, float current, float temperature, int processingTimeMs);

  /// <summary>
  /// Create a standardized position verification prompt
  /// Reusable prompt for position checking steps
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreatePositionVerificationPrompt(
    const std::string& componentName, const std::string& details, UserPromptUI& promptUI);

  /// <summary>
  /// Create a standardized completion notification prompt
  /// Reusable prompt for sequence completion notifications
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreateCompletionPrompt(
    const std::string& sequenceName, const std::string& results, UserPromptUI& promptUI);

  // ============================================================================
  // DEBUG AND TESTING
  // ============================================================================

  /// <summary>
  /// Debug method to print a modern sequence without executing it
  /// Enhanced version with more detailed operation analysis
  /// </summary>
  void DebugPrintModernSequence(const std::string& name, const std::unique_ptr<SequenceStep>& sequence);

} // namespace UAA3ProcessBuilders