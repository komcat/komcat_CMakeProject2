// ============================================================================
// NewProcesses_SAA3.h - SAA3 Machine Process Declarations
// ============================================================================
#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>

namespace SAA3Processes {

    // ========================================================================
    // SAA3 Process Function Declarations
    // ========================================================================

    /// <summary>
    /// Build SAA3 initialization sequence
    /// Initializes all SAA3 systems and moves devices to safe positions
    /// </summary>
    /// <param name="machineOps">Machine operations interface</param>
    /// <param name="promptUI">User prompt interface</param>
    /// <returns>Unique pointer to the initialization sequence</returns>
    std::unique_ptr<SequenceStep> BuildSAA3Initial(
        MachineOperations& machineOps,
        UserPromptUI& promptUI);

    /// <summary>
    /// Build SAA3 pick and place FAU sequence
    /// Handles pickup, transport, and placement of FAU components
    /// </summary>
    /// <param name="machineOps">Machine operations interface</param>
    /// <param name="promptUI">User prompt interface</param>
    /// <returns>Unique pointer to the pick and place sequence</returns>
    std::unique_ptr<SequenceStep> BuildSAA3PickPlaceFAU(
        MachineOperations& machineOps,
        UserPromptUI& promptUI);

    /// <summary>
    /// Build SAA3 epoxy dispensing sequence for FAU
    /// Applies precise epoxy patterns to FAU components
    /// </summary>
    /// <param name="machineOps">Machine operations interface</param>
    /// <param name="promptUI">User prompt interface</param>
    /// <returns>Unique pointer to the dispensing sequence</returns>
    std::unique_ptr<SequenceStep> BuildSAA3DispenseEpoxyFAU(
        MachineOperations& machineOps,
        UserPromptUI& promptUI);

    /// <summary>
    /// Build SAA3 UV curing sequence
    /// Performs UV curing with temperature control and safety monitoring
    /// </summary>
    /// <param name="machineOps">Machine operations interface</param>
    /// <param name="promptUI">User prompt interface</param>
    /// <returns>Unique pointer to the UV curing sequence</returns>
    std::unique_ptr<SequenceStep> BuildSAA3UVCuring(
        MachineOperations& machineOps,
        UserPromptUI& promptUI);

    /// <summary>
    /// Build SAA3 FAU rejection sequence
    /// Safely removes and disposes of defective FAU components
    /// </summary>
    /// <param name="machineOps">Machine operations interface</param>
    /// <param name="promptUI">User prompt interface</param>
    /// <returns>Unique pointer to the rejection sequence</returns>
    std::unique_ptr<SequenceStep> BuildSAA3RejectFAU(
        MachineOperations& machineOps,
        UserPromptUI& promptUI);

    // ========================================================================
    // Registration Functions
    // ========================================================================

    /// <summary>
    /// Register all SAA3 processes with the ProcessRegistry
    /// Call this function during application initialization
    /// </summary>
    void RegisterAllSAA3Processes();

    /// <summary>
    /// Get count of SAA3 processes available
    /// </summary>
    /// <returns>Number of SAA3 processes registered</returns>
    size_t GetSAA3ProcessCount();

    /// <summary>
    /// Check if SAA3 processes are registered
    /// </summary>
    /// <returns>True if SAA3 processes are available</returns>
    bool AreSAA3ProcessesRegistered();

} // namespace SAA3Processes