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

  // ============================================================================
  // MODERN PROBING SEQUENCES
  // ============================================================================

  /// <summary>
  /// Modern probing sequence using UserPromptUI instead of legacy UserConfirmOperation
  /// Includes basic sled and PIC position checks with laser activation
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Enhanced probing sequence with detailed prompts, timeouts, and safety checks
  /// Recommended for production use with comprehensive user guidance
  /// </summary>
  std::unique_ptr<SequenceStep> BuildEnhancedProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Quick probing sequence for testing with minimal user interaction
  /// Useful for automated testing or when user supervision is minimal
  /// </summary>
  std::unique_ptr<SequenceStep> BuildQuickProbingSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // MODERN CALIBRATION SEQUENCES
  // ============================================================================

  /// <summary>
  /// Modern needle calibration sequence with enhanced user prompts
  /// Replaces legacy needle calibration with better user guidance
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernNeedleCalibrationSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Modern dispense calibration sequence for location 1
  /// Enhanced version with better user interaction and error handling
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernDispenseCalibration1Sequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Modern dispense calibration sequence for location 2
  /// Enhanced version with better user interaction and error handling
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernDispenseCalibration2Sequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // MODERN PICK AND PLACE SEQUENCES
  // ============================================================================

  /// <summary>
  /// Modern pick and place sequence for left lens with enhanced prompts
  /// Includes safety checks and detailed user guidance
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernPickPlaceLeftLensSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Modern pick and place sequence for right lens with enhanced prompts
  /// Includes safety checks and detailed user guidance
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernPickPlaceRightLensSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // MODERN UV CURING SEQUENCES
  // ============================================================================

  /// <summary>
  /// Modern UV curing sequence with enhanced safety prompts and monitoring
  /// Includes pre-cure checks and post-cure verification
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernUVCuringSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // MODERN COMPLETE PROCESS SEQUENCES
  // ============================================================================

  /// <summary>
  /// Modern complete process sequence combining all modern sub-sequences
  /// Uses UserPromptUI throughout for consistent user experience
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernCompleteProcessSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Modern automated process sequence with minimal user interaction
  /// For production runs where user supervision is reduced
  /// </summary>
  std::unique_ptr<SequenceStep> BuildModernAutomatedProcessSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // UTILITY FUNCTIONS
  // ============================================================================

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