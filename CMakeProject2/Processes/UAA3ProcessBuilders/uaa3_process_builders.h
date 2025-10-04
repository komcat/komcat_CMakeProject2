#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "ProcessConfiguration.h"  // <-- Add this include
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>

// UAA3 Process Builders - Main Header
// This is the main header that includes all UAA3 process sequences
namespace UAA3ProcessBuilders {

  // ============================================================================
  // Dev
  // ============================================================================


  std::unique_ptr<SequenceStep> BuildDevSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  std::unique_ptr<SequenceStep> CorePickPlace(
		MachineOperations& machineOps, UserPromptUI& promptUI);

  std::unique_ptr<SequenceStep> CorePickOnly(
    MachineOperations& machineOps, UserPromptUI& promptUI);

	std::unique_ptr<SequenceStep> CorePlaceOnly(
		MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // CORE PROCESSES
  // ============================================================================

  /// <summary>
  /// Build initialization sequence for UAA3
  /// </summary>
  std::unique_ptr<SequenceStep> BuildInitializationSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build probing sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildProbingSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build pick and place left lens sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // New configurable version
  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence_uaa3_Configurable(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const ProcessConfiguration& config);

  /// <summary>
  /// Build pick and place right lens sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // New configurable version
  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence_uaa3_Configurable(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const ProcessConfiguration& config);

  /// <summary>
  /// Build UV curing sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildUVCuringSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);


  std::unique_ptr<SequenceStep> BuildUVCuringSequence_uaa3_Configurable(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
		const ProcessConfiguration& config);

  // ============================================================================
  // UTILITY SEQUENCES
  // ============================================================================

  /// <summary>
  /// Build reject left lens sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> RejectLeftLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build reject right lens sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> RejectRightLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // CALIBRATION SEQUENCES
  // ============================================================================

  /// <summary>
  /// Build enhanced needle XY calibration sequence using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildNeedleXYCalibrationSequenceEnhanced_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build dispense calibration sequence (location 1) using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildDispenseCalibrationSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build dispense calibration sequence (location 2) using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildDispenseCalibration2Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // DISPENSING SEQUENCES
  // ============================================================================

  /// <summary>
  /// Build dispense epoxy sequence (location 1) using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildDispenseEpoxy1Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  /// <summary>
  /// Build dispense epoxy sequence (location 2) using UserPromptUI
  /// </summary>
  std::unique_ptr<SequenceStep> BuildDispenseEpoxy2Sequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  // ============================================================================
  // UTILITY FUNCTIONS
  // ============================================================================

  /// <summary>
  /// Create a standardized safety prompt for laser operations
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreateLaserSafetyPrompt(
    UserPromptUI& promptUI, float current, float temperature, int processingTimeMs);

  /// <summary>
  /// Create a standardized position verification prompt
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreatePositionVerificationPrompt(
    const std::string& componentName, const std::string& details, UserPromptUI& promptUI);

  /// <summary>
  /// Create a standardized completion notification prompt
  /// </summary>
  std::unique_ptr<UserPromptOperation> CreateCompletionPrompt(
    const std::string& sequenceName, const std::string& results, UserPromptUI& promptUI);


  // System test sequences
  std::unique_ptr<SequenceStep> BuildSystemTestSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);

  std::unique_ptr<SequenceStep> BuildExtendedSystemTestSequence(
    MachineOperations& machineOps, UserPromptUI& promptUI);





  /// <summary>
  /// Debug method to print a modern sequence without executing it
  /// </summary>
  void DebugPrintModernSequence(const std::string& name, const std::unique_ptr<SequenceStep>& sequence);

} // namespace UAA3ProcessBuilders