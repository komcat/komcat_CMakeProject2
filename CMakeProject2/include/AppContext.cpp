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

void AppContext::PrintModuleStatusReport() const {
  auto* log = GetLogger();
  if (!log) return;

  struct Category {
    std::string name;
    std::vector<std::pair<std::string, bool>> modules;
  };

  std::vector<Category> categories = {
      {"Core Services", {
          {"Logger", GetLogger() != nullptr},
          {"MotionConfig", GetMotionConfig() != nullptr},
          {"MotionControlLayer", GetMotionControlLayer() != nullptr},
          {"ConfigWatchdog", GetConfigWatchdog() != nullptr}
      }},
      {"Motion Hardware", {
          {"PIController", GetPIController() != nullptr},
          {"ACSController", GetACSController() != nullptr}
      }},
      {"IO Systems", {
          {"IOManager", GetIOManager() != nullptr},
          {"IOConfig", GetIOConfig() != nullptr},
          {"Pneumatic", GetPneumaticManager() != nullptr}
      }},
      {"Vision", {
          {"CameraManager", GetCameraManager() != nullptr},
          {"CameraConfig", GetCameraConfig() != nullptr},
          {"VisionExposureManager", GetVisionExposureManager() != nullptr}
      }},
      {"Instruments", {
          {"CLD101x Laser", GetCLD101x() != nullptr},
          {"Keithley 2400", GetKeithley() != nullptr},
          {"Keithley 6482", GetKeithley6482() != nullptr},
          {"SPD Power Supply", GetSPDPowerSupply() != nullptr}
      }},
      {"Data Services", {
          {"DataClient", GetDataClient() != nullptr},
          {"Database", GetDatabaseManager() != nullptr},
          {"ResultsManager", GetResultsManager() != nullptr},
          {"DUT Recorder", GetDUTDataRecorder() != nullptr}
      }},
      {"Operations", {
          {"MachineOps", GetMachineOperations() != nullptr},
          {"MotionOps", GetMotionOps() != nullptr},
          {"IOOps", GetIOOps() != nullptr},
          {"VisionOps", GetVisionOps() != nullptr}
      }}
  };

  // Count totals
  int totalModules = 0;
  int successCount = 0;

  // Count required vs optional
  std::map<std::string, bool> requiredModules = {
      {"Logger", true},
      {"MotionConfig", true},
      {"MotionControlLayer", true},
      {"PIController", true},
      {"ACSController", true},
      {"IOManager", true},
      {"IOConfig", true},
      {"Pneumatic", true},
      {"MachineOps", true},
      {"MotionOps", true},
      {"IOOps", true}
  };

  int requiredSuccess = 0;
  int requiredTotal = 0;
  int optionalSuccess = 0;
  int optionalTotal = 0;

  log->LogInfo("════════════════════════════════════════════════════════");
  log->LogInfo("           MODULE INITIALIZATION REPORT                  ");
  log->LogInfo("════════════════════════════════════════════════════════");

  for (const auto& category : categories) {
    log->LogInfo("");
    log->LogInfo("【" + category.name + "】");
    log->LogInfo("────────────────────────────────────────");

    for (const auto& [name, initialized] : category.modules) {
      totalModules++;

      bool isRequired = requiredModules.find(name) != requiredModules.end() &&
        requiredModules[name];

      if (isRequired) {
        requiredTotal++;
        if (initialized) requiredSuccess++;
      }
      else {
        optionalTotal++;
        if (initialized) optionalSuccess++;
      }

      if (initialized) successCount++;

      // Format status
      std::string status = initialized ? "✓" : "✗";
      std::string statusText = initialized ? "SUCCESS" : "NOT INITIALIZED";
      std::string reqText = isRequired ? "[REQUIRED]" : "[OPTIONAL]";

      // Create formatted line with padding
      std::string line = "  " + status + " " + name;
      // Pad to 30 characters
      while (line.length() < 30) {
        line += " ";
      }
      line += statusText + " " + reqText;

      if (initialized) {
        log->LogInfo(line);
      }
      else if (isRequired) {
        log->LogError(line);
      }
      else {
        log->LogWarning(line);
      }
    }
  }

  log->LogInfo("");
  log->LogInfo("════════════════════════════════════════════════════════");
  log->LogInfo("                      SUMMARY                            ");
  log->LogInfo("────────────────────────────────────────────────────────");

  // Calculate percentages
  float totalSuccessRate = (totalModules > 0) ?
    (float)successCount / totalModules * 100.0f : 0.0f;
  float requiredSuccessRate = (requiredTotal > 0) ?
    (float)requiredSuccess / requiredTotal * 100.0f : 0.0f;
  float optionalSuccessRate = (optionalTotal > 0) ?
    (float)optionalSuccess / optionalTotal * 100.0f : 0.0f;

  log->LogInfo("  Total Modules:    " + std::to_string(successCount) + "/" +
    std::to_string(totalModules) + " (" +
    std::to_string((int)totalSuccessRate) + "%)");

  log->LogInfo("  Required Modules: " + std::to_string(requiredSuccess) + "/" +
    std::to_string(requiredTotal) + " (" +
    std::to_string((int)requiredSuccessRate) + "%)");

  log->LogInfo("  Optional Modules: " + std::to_string(optionalSuccess) + "/" +
    std::to_string(optionalTotal) + " (" +
    std::to_string((int)optionalSuccessRate) + "%)");

  log->LogInfo("");

  // System status
  std::string systemStatus;
  if (requiredSuccessRate == 100.0f) {
    systemStatus = "✓ SYSTEM FULLY OPERATIONAL";
    log->LogInfo(systemStatus);
  }
  else if (requiredSuccessRate >= 80.0f) {
    systemStatus = "⚠ SYSTEM PARTIALLY OPERATIONAL";
    log->LogWarning(systemStatus);
  }
  else {
    systemStatus = "✗ SYSTEM NOT READY - CRITICAL MODULES MISSING";
    log->LogError(systemStatus);
  }

  log->LogInfo("════════════════════════════════════════════════════════");
}