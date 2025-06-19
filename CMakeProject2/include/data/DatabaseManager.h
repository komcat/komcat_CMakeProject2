// DatabaseManager.h
#pragma once

#include "external/sqlite/sqlite3.h"
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <map>

class DatabaseManager {
public:
  DatabaseManager();
  ~DatabaseManager();

  // Database lifecycle
  bool Initialize(const std::string& dbPath = "db/machine_operations.db");
  bool Close();
  bool IsConnected() const;

  // Schema management
  bool CreateTables();
  bool CreateIndexes();
  bool DropTables();

  // Basic operations
  bool ExecuteQuery(const std::string& query);
  bool ExecuteQuery(const std::string& query, std::vector<std::vector<std::string>>& results);

  // Prepared statements
  bool ExecutePreparedStatement(const std::string& query,
    const std::vector<std::string>& parameters);

  // Insert operations
  bool InsertRecord(const std::string& table,
    const std::map<std::string, std::string>& data);

  // Query operations
  std::vector<std::map<std::string, std::string>> SelectRecords(
    const std::string& table,
    const std::string& whereClause = "",
    const std::string& orderBy = "",
    int limit = -1);

  // Utility
  std::string GetLastError() const;
  void EnableWALMode();
  void EnableForeignKeys();
  bool BeginTransaction();
  bool CommitTransaction();
  bool RollbackTransaction();

  // Maintenance
  bool Vacuum();
  bool Analyze();
  int64_t GetDatabaseSize();

private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> m_db;
  mutable std::mutex m_dbMutex;
  std::string m_lastError;
  bool m_isConnected;

  // Helper methods
  bool ExecuteInternal(const std::string& query);
  std::string EscapeString(const std::string& input);
};