#include "ProcessStep.h"
#include "logger.h"

ProcessStep::ProcessStep(const std::string& name, MachineOperations& machineOps)
  : m_name(name),
  m_machineOps(machineOps)
{
  LogInfo("Created process step: " + m_name);
}

void ProcessStep::NotifyCompletion(bool success) {
  if (m_completionCallback) {
    m_completionCallback(success);
  }
}

void ProcessStep::LogInfo(const std::string& message) {
  Logger::GetInstance()->LogProcess("ProcessStep[" + m_name + "]: " + message);
}

void ProcessStep::LogError(const std::string& message) {
  Logger::GetInstance()->LogError("ProcessStep[" + m_name + "]: " + message);
}

// InitializationStep implementation
InitializationStep::InitializationStep(MachineOperations& machineOps)
  : ProcessStep("Initialization", machineOps)
{
}

bool InitializationStep::Execute() {
  LogInfo("Starting initialization sequence");

  // Step 1: Move gantry-main to "safe" from current position
  LogInfo("Moving gantry-main to safe position");
  if (!m_machineOps.MoveDeviceToNode("gantry-main", "Process_Flow", "node_4027", true)) {
    LogError("Failed to move gantry-main to safe position");
    NotifyCompletion(false);
    return false;
  }

  // Step 2: Move hex-left to home
  LogInfo("Moving hex-left to home position");
  if (!m_machineOps.MoveDeviceToNode("hex-left", "Process_Flow", "node_5480", true)) {
    LogError("Failed to move hex-left to home position");
    NotifyCompletion(false);
    return false;
  }

  // Step 3: Move hex-right to home
  LogInfo("Moving hex-right to home position");
  if (!m_machineOps.MoveDeviceToNode("hex-right", "Process_Flow", "node_5136", true)) {
    LogError("Failed to move hex-right to home position");
    NotifyCompletion(false);
    return false;
  }

  // Step 4: Clear output L_Gripper (assuming pin 0 based on the file)
  LogInfo("Clearing L_Gripper output");
  if (!m_machineOps.SetOutput("IOBottom", 0, false)) {
    LogError("Failed to clear L_Gripper output");
    NotifyCompletion(false);
    return false;
  }

  // Step 5: Clear output R_Gripper (assuming pin 2 based on the file)
  LogInfo("Clearing R_Gripper output");
  if (!m_machineOps.SetOutput("IOBottom", 2, false)) {
    LogError("Failed to clear R_Gripper output");
    NotifyCompletion(false);
    return false;
  }

  // Step 6: Set output Vacuum_Base (assuming pin 10 based on the file)
  LogInfo("Activating Vacuum_Base output");
  if (!m_machineOps.SetOutput("IOBottom", 10, true)) {
    LogError("Failed to activate Vacuum_Base output");
    NotifyCompletion(false);
    return false;
  }

  LogInfo("Initialization complete successfully");
  NotifyCompletion(true);
  return true;
}