// SQLiteConnection.h
#pragma once

#include "DUTDataRecorder.h"
#include <sqlite3.h>
#include <string>
#include <chrono>

class SQLiteConnection : public DatabaseConnection {
public:
  SQLiteConnection();
  ~SQLiteConnection() override;

  bool Connect(const std::string& connectionString) override;
  void Disconnect() override;
  bool IsConnected() const override;

  bool BeginTransaction() override;
  bool CommitTransaction() override;
  bool RollbackTransaction() override;
  // NEW: Add method to get latest serial number
  std::string GetLatestSerialNumber();

  // NEW: Getter for raw database handle (if needed for advanced queries)
  sqlite3* GetDatabase() const { return m_db; }

  // Updated method signature to include label parameter
  bool InsertDataPoint(
    const std::string& serialNumber,
    const std::string& key,
    double value,
    const std::chrono::system_clock::time_point& timestamp,
    const std::string& label = ""  // NEW: label parameter with default
  ) override;

  std::string GetLastError() const override;

private:
  sqlite3* m_db;
  sqlite3_stmt* m_insertStmt;
  std::string m_lastError;

  bool CreateTableIfNotExists();
  bool PrepareStatements();
  bool UpgradeDatabase();  // NEW: method to handle database upgrades
};