// AppContext.cpp
#include "AppContext.h"
#include "include/machine_operations.h"  // Now we can include the full header

std::shared_ptr<DatabaseManager> AppContext::GetDatabaseManagerShared() const {
  auto* machineOps = GetMachineOperations();
  if (machineOps) {
    // Try to get from MachineOperations first (which returns shared_ptr)
    return machineOps->GetDatabaseManager();
  }
  // Fallback: wrap raw pointer in shared_ptr (use with caution)
  auto* db = GetDatabaseManager();
  return db ? std::shared_ptr<DatabaseManager>(db, [](DatabaseManager*) {}) : nullptr;
}

std::shared_ptr<OperationResultsManager> AppContext::GetResultsManagerShared() const {
  auto* machineOps = GetMachineOperations();
  if (machineOps) {
    // Try to get from MachineOperations first (which returns shared_ptr)
    return machineOps->GetResultsManager();
  }
  // Fallback: wrap raw pointer in shared_ptr (use with caution)
  auto* results = GetResultsManager();
  return results ? std::shared_ptr<OperationResultsManager>(results, [](OperationResultsManager*) {}) : nullptr;
}