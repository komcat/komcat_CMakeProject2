#include "SequenceStep.h"

SequenceStep::SequenceStep(const std::string& name, MachineOperations& machineOps)
  : ProcessStep(name, machineOps)
{
}

void SequenceStep::AddOperation(std::shared_ptr<SequenceOperation> operation) {
  m_operations.push_back(operation);
  LogInfo("Added operation: " + operation->GetDescription());
}

bool SequenceStep::Execute() {
  LogInfo("Starting sequence execution with " + std::to_string(m_operations.size()) + " operations");

  // Clean up any lingering scanners before starting a new sequence
  m_machineOps.CleanupAllScanners();

  // Print the entire sequence plan before execution
  PrintSequencePlan();

  LogInfo("Starting sequence execution with " + std::to_string(m_operations.size()) + " operations");

  bool success = true;
  for (size_t i = 0; i < m_operations.size(); ++i) {
    auto& operation = m_operations[i];

    LogInfo("EXECUTING " + std::to_string(i + 1) + "/" +
      std::to_string(m_operations.size()) + ": " + operation->GetDescription());

    if (!operation->Execute(m_machineOps)) {
      LogError("Operation FAILED: " + operation->GetDescription());
      success = false;
      break;
    }

    LogInfo("Operation COMPLETED SUCCESSFULLY: " + operation->GetDescription());
  }

  if (success) {
    LogInfo("Sequence completed successfully");
  }
  else {
    LogError("Sequence failed");
  }

  // Notify completion
  NotifyCompletion(success);
  return success;
}
void SequenceStep::PrintSequencePlan() const {
  Logger* logger = Logger::GetInstance();

  logger->LogInfo("=== SEQUENCE PLAN: " + GetName() + " ===");
  logger->LogInfo("Total operations: " + std::to_string(m_operations.size()));

  for (size_t i = 0; i < m_operations.size(); ++i) {
    auto& operation = m_operations[i];
    logger->LogInfo(std::to_string(i + 1) + ". " + operation->GetDescription());
  }

  logger->LogInfo("=== END SEQUENCE PLAN ===");
}