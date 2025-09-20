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

    // NEW: Notify callback that operation is starting
    if (m_operationCallback) {
      m_operationCallback(i, operation->GetDescription(), true);
    }

    LogInfo("EXECUTING " + std::to_string(i + 1) + "/" +
      std::to_string(m_operations.size()) + ": " + operation->GetDescription());

    if (!operation->Execute(m_machineOps)) {
      LogError("Operation FAILED: " + operation->GetDescription());

      // CHECK FOR FALLBACK
      auto it = m_fallbacks.find(operation);
      if (it != m_fallbacks.end()) {
        LogInfo("Attempting FALLBACK operation: " + it->second->GetDescription());

        // NEW: Notify callback about fallback
        if (m_operationCallback) {
          m_operationCallback(i, "FALLBACK: " + it->second->GetDescription(), true);
        }

        if (it->second->Execute(m_machineOps)) {
          LogInfo("FALLBACK SUCCEEDED: " + it->second->GetDescription());
          continue;  // Continue to next operation
        }
        else {
          LogError("FALLBACK also FAILED: " + it->second->GetDescription());
          success = false;
          break;
        }
      }
      else {
        // No fallback available
        success = false;
        break;
      }
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


// In SequenceStep.cpp
void SequenceStep::AddOperationWithFallback(
  std::shared_ptr<SequenceOperation> primary,
  std::shared_ptr<SequenceOperation> fallback) {

  m_operations.push_back(primary);
  m_fallbacks[primary] = fallback;

  // Add debug log to verify it's being stored
  LogInfo("Added operation with fallback: " + primary->GetDescription() +
    " -> " + fallback->GetDescription());
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