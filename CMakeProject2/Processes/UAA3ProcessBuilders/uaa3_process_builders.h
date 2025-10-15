#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "ProcessConfiguration.h"  // <-- Add this include
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>

// Add this forward declaration at the top
class IDisplayOutput;

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


  // NEW: Parameterized builder functions for Core operations
  std::unique_ptr<SequenceStep> createCorePickPlace(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceName,        // 3
    const std::string& graphName,         // 4
    const std::string& pickNode,          // 5
    const std::string& placeNode,         // 6
    const std::string& cameraGantry,      // 7
    const std::string& cameraViewPickNode,// 8
    const std::string& cameraViewPlaceNode,// 9
    const std::string& gripperOutputDevice,// 10
    const std::string& gripperPinName,    // 11
    float gripperHoldDelay,               // 12
    float speed,                          // 13
    bool enableCameraView,                 // 14 ← ADD THIS
		bool enableUserConfirm = true         // 15 ← ADD THIS
  );

  std::unique_ptr<SequenceStep> createCorePickOnly(
    MachineOperations& machineOps,        //1
    UserPromptUI& promptUI,               //2
    const std::string& deviceName,        //3
    const std::string& graphName,         // 4
    const std::string& pickNode,          //5
    const std::string& cameraGantry,      //6
    const std::string& cameraViewNode,    //7
    const std::string& gripperOutputDevice,// 8
    const std::string& gripperPinName,    // 9
    float gripperHoldDelay = 0.5f,       //10
    float speed=5.0f,                          //11
    bool enableCameraView = true,                 //12
		bool slowDownOnApproach = true,       //13
		float approachDistance = 1.0f,          //14
		float speedOnApproach = 1.0f       //15

  );

  std::unique_ptr<SequenceStep> createCorePlaceOnly(
    MachineOperations& machineOps,        //1
    UserPromptUI& promptUI,               //2
    const std::string& deviceName,        //3
    const std::string& graphName,         // 4
    const std::string& placeNode,          //5
    const std::string& cameraGantry,      //6
    const std::string& cameraViewNode,    //7
    const std::string& gripperOutputDevice,// 8
    const std::string& gripperPinName,    // 9
    float gripperHoldDelay = 0.5f,       //10
    float speed = 5.0f,                          //11
    bool enableCameraView=true,                 //12
    bool slowDownOnApproach = true,       //13
    float approachDistance = 1.0f,          //14
    float speedOnApproach = 1.0f       //15
  );


  std::unique_ptr<SequenceStep> createCoreUVOnly(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceName,        // 3
    const std::string& graphName,         // 4
    const std::string& uvNode,            // 5
    const std::string& pneumaticUVDevice, // 6
    const std::string& ioDevice,          // 7
    const std::string& uvTriggerPinName,  // 8
    float uvDurationSeconds = 210.0f,     // 9 (default from your UV sequence)
    float speed = 5.0f,                    // 10
		bool fineAlignement_enable_1 = true, // 11
		const std::string& fineAlignmentDevice_1 = "hex-left", // 12
		const std::string& feedBackChannelName_1 = "GPIB-Current", // 13
    bool fineAlignement_enable_2 = true, // 14
    const std::string& fineAlignmentDevice_2 = "hex-left", // 15
    const std::string& feedBackChannelName_2 = "GPIB-Current" // 16
  );


  std::unique_ptr<SequenceStep> createCoreUnload(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceName,        // 3
    const std::string& graphName,         // 4
    const std::string& homeNode,          // 5
    const std::string& ioDeviceVacuum,    // 6
    const std::string& vacuumPinName,     // 7
    const std::string& ioDeviceGripper,   // 8
    const std::string& gripperPinName     // 9
  );

  std::unique_ptr<SequenceStep> createCoreUnloadTwoGrippers(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceNameLeft,    // 3
    const std::string& graphNameLeft,     // 4
    const std::string& homeNodeLeft,      // 5
    const std::string& deviceNameRight,   // 6
    const std::string& graphNameRight,    // 7
    const std::string& homeNodeRight,     // 8
    const std::string& ioDeviceVacuum,    // 9
    const std::string& vacuumPinName,     // 10
    const std::string& ioDeviceGripperLeft,   // 11
    const std::string& gripperPinNameLeft,    // 12
    const std::string& ioDeviceGripperRight,  // 13
    const std::string& gripperPinNameRight    // 14
  );


  // Add these after the createCoreUnloadTwoGrippers declaration:

  std::unique_ptr<SequenceStep> createCoreMoveToNode(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceName,        // 3
    const std::string& graphName,         // 4
    const std::string& targetNode,        // 5
    float speed                           // 6
  );

  // Update the createCoreDispense signature:

  std::unique_ptr<SequenceStep> createCoreDispense(
    MachineOperations& machineOps,        // 1
    UserPromptUI& promptUI,               // 2
    const std::string& deviceName,        // 3
    const std::string& graphName,         // 4
    const std::string& dispensePointName, // 5
    float safeDispenseZOffset,            // 6 - NEW: Z offset for safe position
    const std::string& homeNode,          // 7
    const std::string& ioDeviceDispense,  // 8
    const std::string& dispensePinName,   // 9
    const std::string& pneumaticDispenseDevice, // 10
    float dispenseDurationSeconds,        // 11
    float moveSpeed,                      // 12
    float touchDownSpeed,                 // 13
    float liftOffSpeed                    // 14
  );


  // ============================================================================
  // CORE PROCESSES
  // ============================================================================

  /// <summary>
  /// Build initialization sequence for UAA3
  /// </summary>
  std::unique_ptr<SequenceStep> BuildInitializationSequence_uaa3(
    MachineOperations& machineOps,
    UserPromptUI& promptUI);  // That's it!

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