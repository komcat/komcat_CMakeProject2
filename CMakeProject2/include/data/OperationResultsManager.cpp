// OperationResultsManager.cpp
#include "OperationResultsManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

OperationResultsManager::OperationResultsManager(std::shared_ptr<DatabaseManager> dbManager)
  : m_dbManager(dbManager) {

  if (!m_dbManager || !m_dbManager->IsConnected()) {
    throw std::runtime_error("OperationResultsManager: Invalid or disconnected database manager");
  }
}

OperationResultsManager::~OperationResultsManager() {
  // End any remaining active operations
  std::lock_guard<std::mutex> lock(m_operationsMutex);
  for (const auto& [opId, startTime] : m_activeOperations) {
    EndOperation(opId, "interrupted", "Application shutdown");
  }
}

std::string OperationResultsManager::StartOperation(const std::string& methodName,
  const std::string& deviceName,
  const std::string& callerContext,  // NEW
  const std::string& sequenceName,   // NEW
  const std::map<std::string, std::string>& parameters) {
  std::string operationId = GenerateOperationId();
  std::string timestamp = GetCurrentTimestamp();

  // Store start time for elapsed calculation
  {
    std::lock_guard<std::mutex> lock(m_operationsMutex);
    m_activeOperations[operationId] = std::chrono::high_resolution_clock::now();
  }

  // Insert operation record with caller context
  std::map<std::string, std::string> operationData = {
      {"operation_id", operationId},
      {"method_name", methodName},
      {"device_name", deviceName},
      {"caller_context", callerContext},  // NEW
      {"sequence_name", sequenceName},    // NEW
      {"status", "running"},
      {"start_time", timestamp}
  };

  if (!m_dbManager->InsertRecord("operations", operationData)) {
    // Remove from active operations if database insert failed
    std::lock_guard<std::mutex> lock(m_operationsMutex);
    m_activeOperations.erase(operationId);
    return "";
  }

  // Store parameters as operation results
  for (const auto& [key, value] : parameters) {
    StoreResult(operationId, "param_" + key, value);
  }

  return operationId;
}

bool OperationResultsManager::EndOperation(const std::string& operationId,
  const std::string& status,
  const std::string& errorMessage) {
  int64_t elapsedMs = CalculateElapsedTime(operationId);
  std::string timestamp = GetCurrentTimestamp();

  // Remove from active operations
  {
    std::lock_guard<std::mutex> lock(m_operationsMutex);
    m_activeOperations.erase(operationId);
  }

  // Update operation record
  std::stringstream updateQuery;
  updateQuery << "UPDATE operations SET "
    << "status = ?, "
    << "end_time = ?, "
    << "elapsed_time_ms = ?";

  std::vector<std::string> parameters = { status, timestamp, std::to_string(elapsedMs) };

  if (!errorMessage.empty()) {
    updateQuery << ", error_message = ?";
    parameters.push_back(errorMessage);
  }

  updateQuery << " WHERE operation_id = ?";
  parameters.push_back(operationId);

  return m_dbManager->ExecutePreparedStatement(updateQuery.str(), parameters);
}

bool OperationResultsManager::StoreResult(const std::string& operationId,
  const std::string& key,
  const std::string& value) {
  std::map<std::string, std::string> resultData = {
      {"operation_id", operationId},
      {"key", key},
      {"value", value},
      {"timestamp", GetCurrentTimestamp()}
  };

  return m_dbManager->InsertRecord("operation_results", resultData);
}

bool OperationResultsManager::StoreResults(const std::string& operationId,
  const std::map<std::string, std::string>& results) {
  // Use transaction for multiple inserts
  if (!m_dbManager->BeginTransaction()) {
    return false;
  }

  bool allSuccess = true;
  for (const auto& [key, value] : results) {
    if (!StoreResult(operationId, key, value)) {
      allSuccess = false;
      break;
    }
  }

  if (allSuccess) {
    return m_dbManager->CommitTransaction();
  }
  else {
    m_dbManager->RollbackTransaction();
    return false;
  }
}

std::vector<OperationResult> OperationResultsManager::GetOperationHistory(int limit) {
  std::vector<OperationResult> results;

  auto records = m_dbManager->SelectRecords("operations", "", "start_time DESC", limit);

  for (const auto& record : records) {
    OperationResult result;
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";  // NEW
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";     // NEW
    result.status = record.at("status");

    // Parse elapsed time
    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    // Get all result data for this operation
    result.data = GetAllResults(result.operationId);

    results.push_back(result);
  }

  return results;
}

std::vector<OperationResult> OperationResultsManager::GetOperationsByMethod(const std::string& methodName, int limit) {
  std::string whereClause = "method_name = '" + methodName + "'";
  auto records = m_dbManager->SelectRecords("operations", whereClause, "start_time DESC", limit);

  std::vector<OperationResult> results;
  for (const auto& record : records) {
    OperationResult result;
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";
    result.status = record.at("status");

    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    result.data = GetAllResults(result.operationId);
    results.push_back(result);
  }

  return results;
}

std::vector<OperationResult> OperationResultsManager::GetOperationsByDevice(const std::string& deviceName, int limit) {
  std::string whereClause = "device_name = '" + deviceName + "'";
  auto records = m_dbManager->SelectRecords("operations", whereClause, "start_time DESC", limit);

  std::vector<OperationResult> results;
  for (const auto& record : records) {
    OperationResult result;
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";
    result.status = record.at("status");

    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    result.data = GetAllResults(result.operationId);
    results.push_back(result);
  }

  return results;
}

// NEW: Get operations by caller
std::vector<OperationResult> OperationResultsManager::GetOperationsByCaller(const std::string& callerContext, int limit) {
  std::string whereClause = "caller_context = '" + callerContext + "'";
  auto records = m_dbManager->SelectRecords("operations", whereClause, "start_time DESC", limit);

  std::vector<OperationResult> results;
  for (const auto& record : records) {
    OperationResult result;
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";
    result.status = record.at("status");

    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    result.data = GetAllResults(result.operationId);
    results.push_back(result);
  }

  return results;
}

// NEW: Get operations by sequence
std::vector<OperationResult> OperationResultsManager::GetOperationsBySequence(const std::string& sequenceName, int limit) {
  std::string whereClause = "sequence_name = '" + sequenceName + "'";
  auto records = m_dbManager->SelectRecords("operations", whereClause, "start_time DESC", limit);

  std::vector<OperationResult> results;
  for (const auto& record : records) {
    OperationResult result;
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";
    result.status = record.at("status");

    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    result.data = GetAllResults(result.operationId);
    results.push_back(result);
  }

  return results;
}

OperationResult OperationResultsManager::GetLatestOperation(const std::string& methodName) {
  OperationResult result;

  std::string whereClause = "";
  if (!methodName.empty()) {
    whereClause = "method_name = '" + methodName + "'";
  }

  auto records = m_dbManager->SelectRecords("operations", whereClause, "start_time DESC", 1);

  if (!records.empty()) {
    const auto& record = records[0];
    result.operationId = record.at("operation_id");
    result.methodName = record.at("method_name");
    result.deviceName = record.count("device_name") ? record.at("device_name") : "";
    result.callerContext = record.count("caller_context") ? record.at("caller_context") : "";
    result.sequenceName = record.count("sequence_name") ? record.at("sequence_name") : "";
    result.status = record.at("status");

    if (record.count("elapsed_time_ms") && !record.at("elapsed_time_ms").empty()) {
      try {
        result.elapsedTimeMs = std::stoll(record.at("elapsed_time_ms"));
      }
      catch (const std::exception&) {
        result.elapsedTimeMs = 0;
      }
    }

    result.data = GetAllResults(result.operationId);
  }

  return result;
}

std::map<std::string, std::string> OperationResultsManager::GetLatestResults(const std::string& methodName) {
  auto latestOp = GetLatestOperation(methodName);
  return latestOp.data;
}

std::string OperationResultsManager::GetResult(const std::string& operationId, const std::string& key) {
  std::string whereClause = "operation_id = '" + operationId + "' AND key = '" + key + "'";
  auto records = m_dbManager->SelectRecords("operation_results", whereClause, "timestamp DESC", 1);

  if (!records.empty() && records[0].count("value")) {
    return records[0].at("value");
  }

  return "";
}

std::map<std::string, std::string> OperationResultsManager::GetAllResults(const std::string& operationId) {
  std::map<std::string, std::string> results;

  std::string whereClause = "operation_id = '" + operationId + "'";
  auto records = m_dbManager->SelectRecords("operation_results", whereClause, "timestamp ASC");

  for (const auto& record : records) {
    if (record.count("key") && record.count("value")) {
      results[record.at("key")] = record.at("value");
    }
  }

  return results;
}

int OperationResultsManager::GetOperationCount(const std::string& methodName) {
  std::string whereClause = "";
  if (!methodName.empty()) {
    whereClause = "method_name = '" + methodName + "'";
  }

  std::string query = "SELECT COUNT(*) FROM operations";
  if (!whereClause.empty()) {
    query += " WHERE " + whereClause;
  }

  std::vector<std::vector<std::string>> results;
  if (m_dbManager->ExecuteQuery(query, results) && !results.empty() && !results[0].empty()) {
    try {
      return std::stoi(results[0][0]);
    }
    catch (const std::exception&) {
      return 0;
    }
  }

  return 0;
}

double OperationResultsManager::GetAverageElapsedTime(const std::string& methodName) {
  std::string whereClause = "elapsed_time_ms IS NOT NULL";
  if (!methodName.empty()) {
    whereClause += " AND method_name = '" + methodName + "'";
  }

  std::string query = "SELECT AVG(elapsed_time_ms) FROM operations WHERE " + whereClause;

  std::vector<std::vector<std::string>> results;
  if (m_dbManager->ExecuteQuery(query, results) && !results.empty() && !results[0].empty()) {
    try {
      return std::stod(results[0][0]);
    }
    catch (const std::exception&) {
      return 0.0;
    }
  }

  return 0.0;
}

double OperationResultsManager::GetSuccessRate(const std::string& methodName) {
  std::string baseWhere = "status != 'running'";
  if (!methodName.empty()) {
    baseWhere += " AND method_name = '" + methodName + "'";
  }

  // Get total completed operations
  std::string totalQuery = "SELECT COUNT(*) FROM operations WHERE " + baseWhere;
  std::vector<std::vector<std::string>> totalResults;

  if (!m_dbManager->ExecuteQuery(totalQuery, totalResults) || totalResults.empty()) {
    return 0.0;
  }

  int total;
  try {
    total = std::stoi(totalResults[0][0]);
  }
  catch (const std::exception&) {
    return 0.0;
  }

  if (total == 0) return 0.0;

  // Get successful operations
  std::string successQuery = "SELECT COUNT(*) FROM operations WHERE " + baseWhere + " AND status = 'success'";
  std::vector<std::vector<std::string>> successResults;

  if (!m_dbManager->ExecuteQuery(successQuery, successResults) || successResults.empty()) {
    return 0.0;
  }

  int successful;
  try {
    successful = std::stoi(successResults[0][0]);
  }
  catch (const std::exception&) {
    return 0.0;
  }

  return (static_cast<double>(successful) / static_cast<double>(total)) * 100.0;
}

// NEW: Get sequence success rate
double OperationResultsManager::GetSequenceSuccessRate(const std::string& sequenceName) {
  std::string baseWhere = "status != 'running'";
  if (!sequenceName.empty()) {
    baseWhere += " AND sequence_name = '" + sequenceName + "'";
  }

  std::string totalQuery = "SELECT COUNT(*) FROM operations WHERE " + baseWhere;
  std::vector<std::vector<std::string>> totalResults;

  if (!m_dbManager->ExecuteQuery(totalQuery, totalResults) || totalResults.empty()) {
    return 0.0;
  }

  int total;
  try {
    total = std::stoi(totalResults[0][0]);
  }
  catch (const std::exception&) {
    return 0.0;
  }

  if (total == 0) return 0.0;

  std::string successQuery = "SELECT COUNT(*) FROM operations WHERE " + baseWhere + " AND status = 'success'";
  std::vector<std::vector<std::string>> successResults;

  if (!m_dbManager->ExecuteQuery(successQuery, successResults) || successResults.empty()) {
    return 0.0;
  }

  int successful;
  try {
    successful = std::stoi(successResults[0][0]);
  }
  catch (const std::exception&) {
    return 0.0;
  }

  return (static_cast<double>(successful) / static_cast<double>(total)) * 100.0;
}

// NEW: Get operation count by sequence
std::map<std::string, int> OperationResultsManager::GetOperationCountBySequence() {
  std::map<std::string, int> counts;

  std::string query = "SELECT sequence_name, COUNT(*) FROM operations WHERE sequence_name != '' GROUP BY sequence_name";
  std::vector<std::vector<std::string>> results;

  if (m_dbManager->ExecuteQuery(query, results)) {
    for (const auto& row : results) {
      if (row.size() >= 2) {
        try {
          counts[row[0]] = std::stoi(row[1]);
        }
        catch (const std::exception&) {
          counts[row[0]] = 0;
        }
      }
    }
  }

  return counts;
}

bool OperationResultsManager::CleanupOldOperations(int maxAgeHours) {
  auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(maxAgeHours);

  std::time_t cutoff = std::chrono::system_clock::to_time_t(cutoffTime);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&cutoff), "%Y-%m-%d %H:%M:%S");

  std::string query = "DELETE FROM operations WHERE start_time < ?";
  return m_dbManager->ExecutePreparedStatement(query, { ss.str() });
}

bool OperationResultsManager::CleanupByCount(int maxOperations) {
  std::string query = R"(
        DELETE FROM operations 
        WHERE operation_id NOT IN (
            SELECT operation_id FROM operations 
            ORDER BY start_time DESC 
            LIMIT ?
        )
    )";

  return m_dbManager->ExecutePreparedStatement(query, { std::to_string(maxOperations) });
}

std::string OperationResultsManager::GenerateOperationId() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << "op_"
    << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S")
    << "_" << std::setfill('0') << std::setw(3) << ms.count()
    << "_" << std::setfill('0') << std::setw(3) << (m_operationCounter++);

  return ss.str();
}

std::string OperationResultsManager::GetCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

int64_t OperationResultsManager::CalculateElapsedTime(const std::string& operationId) {
  std::lock_guard<std::mutex> lock(m_operationsMutex);

  auto it = m_activeOperations.find(operationId);
  if (it != m_activeOperations.end()) {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
    return elapsed.count();
  }

  return 0;
}