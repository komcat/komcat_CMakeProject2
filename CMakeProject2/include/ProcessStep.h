#pragma once

#include "machine_operations.h"
#include <string>
#include <functional>
#include <vector>

// Forward declarations
class MotionControlLayer;
class PIControllerManager;
class EziIOManager;
class PneumaticManager;

// Base class for process steps
class ProcessStep {
public:
  ProcessStep(const std::string& name, MachineOperations& machineOps);
  virtual ~ProcessStep() = default;

  // Execute the process step
  virtual bool Execute() = 0;

  // Get step name
  const std::string& GetName() const { return m_name; }

  // Set callback for completion
  void SetCompletionCallback(std::function<void(bool)> callback) {
    m_completionCallback = callback;
  }

protected:
  std::string m_name;
  MachineOperations& m_machineOps;
  std::function<void(bool)> m_completionCallback;

  // Helper to call completion callback
  void NotifyCompletion(bool success);

  // Log helper
  void LogInfo(const std::string& message);
  void LogError(const std::string& message);
};

// Process step for system initialization
class InitializationStep : public ProcessStep {
public:
  InitializationStep(MachineOperations& machineOps);

  // Execute the initialization sequence
  bool Execute() override;
};