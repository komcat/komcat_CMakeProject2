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

  bool InsertDataPoint(
    const std::string& serialNumber,
    const std::string& key,
    double value,
    const std::chrono::system_clock::time_point& timestamp
  ) override;

  std::string GetLastError() const override;

private:
  sqlite3* m_db;
  sqlite3_stmt* m_insertStmt;
  std::string m_lastError;

  bool CreateTableIfNotExists();
  bool PrepareStatements();
};