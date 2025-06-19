// OperationResultsManager.h
#pragma once

#include "DatabaseManager.h"
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <atomic>
#include <memory>

struct OperationResult {
  std::string operationId;
  std::string methodName;
  std::string deviceName;
  std::string callerContext;  // Who called this operation
  std::string sequenceName;   // Which sequence this belongs to
  std::string status; // "success", "failed", "timeout"
  int64_t elapsedTimeMs;
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> data; // Key-value pairs for method-specific data
};

class OperationResultsManager {
public:
  OperationResultsManager(std::shared_ptr<DatabaseManager> dbManager);
  ~OperationResultsManager();

  // Operation lifecycle - WITH caller context
  std::string StartOperation(const std::string& methodName,
    const std::string& deviceName = "",
    const std::string& callerContext = "",  // NEW
    const std::string& sequenceName = "",   // NEW  
    const std::map<std::string, std::string>& parameters = {});

  bool EndOperation(const std::string& operationId,
    const std::string& status,
    const std::string& errorMessage = "");

  // Result storage
  bool StoreResult(const std::string& operationId,
    const std::string& key,
    const std::string& value);

  bool StoreResults(const std::string& operationId,
    const std::map<std::string, std::string>& results);

  // Query operations
  std::vector<OperationResult> GetOperationHistory(int limit = 100);
  std::vector<OperationResult> GetOperationsByMethod(const std::string& methodName, int limit = 50);
  std::vector<OperationResult> GetOperationsByDevice(const std::string& deviceName, int limit = 50);
  std::vector<OperationResult> GetOperationsByCaller(const std::string& callerContext, int limit = 50);  // NEW
  std::vector<OperationResult> GetOperationsBySequence(const std::string& sequenceName, int limit = 50); // NEW

  // Get latest operation results
  OperationResult GetLatestOperation(const std::string& methodName = "");
  std::map<std::string, std::string> GetLatestResults(const std::string& methodName = "");

  // Specific result queries
  std::string GetResult(const std::string& operationId, const std::string& key);
  std::map<std::string, std::string> GetAllResults(const std::string& operationId);

  // Statistics
  int GetOperationCount(const std::string& methodName = "");
  double GetAverageElapsedTime(const std::string& methodName = "");
  double GetSuccessRate(const std::string& methodName = "");

  // NEW: Sequence-level statistics
  double GetSequenceSuccessRate(const std::string& sequenceName = "");
  std::map<std::string, int> GetOperationCountBySequence();

  // Maintenance
  bool CleanupOldOperations(int maxAgeHours = 24);
  bool CleanupByCount(int maxOperations = 10000);

private:
  std::shared_ptr<DatabaseManager> m_dbManager;
  std::atomic<uint64_t> m_operationCounter{ 1 };
  std::map<std::string, std::chrono::high_resolution_clock::time_point> m_activeOperations;
  mutable std::mutex m_operationsMutex;

  // Helper methods
  std::string GenerateOperationId();
  std::string GetCurrentTimestamp();
  int64_t CalculateElapsedTime(const std::string& operationId);
  bool CreateOperationTables();
};