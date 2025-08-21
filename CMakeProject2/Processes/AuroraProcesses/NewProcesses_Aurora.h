// ============================================================================
// NewProcesses_Aurora.h - FIXED VERSION
// ============================================================================
#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>

namespace AuroraProcesses {


  std::unique_ptr<SequenceStep> BuildAuroraSimpleSMUTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    std::shared_ptr<Keithley2400Operations> smuOps = nullptr);


  std::unique_ptr<SequenceStep> BuildAuroraBasicSMUTest(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    std::shared_ptr<Keithley2400Operations> smuOps = nullptr);

  // ========================================================================
  // Registration Functions
  // ========================================================================

  /// <summary>
  /// Register all Aurora processes with the ProcessRegistry
  /// Call this function during application initialization
  /// </summary>
  void RegisterAllAuroraProcesses();

  /// <summary>
  /// Get count of Aurora processes available
  /// </summary>
  /// <returns>Number of Aurora processes registered</returns>
  size_t GetAuroraProcessCount();

  /// <summary>
  /// Check if Aurora processes are registered
  /// </summary>
  /// <returns>True if Aurora processes are available</returns>
  bool AreAuroraProcessesRegistered();

} // namespace AuroraProcesses