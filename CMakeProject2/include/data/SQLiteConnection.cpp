
// SQLiteConnection.cpp
#include "SQLiteConnection.h"
#include <sstream>
#include <iomanip>
#include <iostream>

SQLiteConnection::SQLiteConnection()
  : m_db(nullptr), m_insertStmt(nullptr) {
}

SQLiteConnection::~SQLiteConnection() {
  Disconnect();
}

bool SQLiteConnection::Connect(const std::string& connectionString) {
  // SQLite automatically creates the database file if it doesn't exist
  // connectionString is the database file path
  int rc = sqlite3_open(connectionString.c_str(), &m_db);
  if (rc != SQLITE_OK) {
    m_lastError = sqlite3_errmsg(m_db);
    sqlite3_close(m_db);
    m_db = nullptr;
    return false;
  }

  // Log if we created a new database
  bool isNewDatabase = !std::filesystem::exists(connectionString) ||
    std::filesystem::file_size(connectionString) == 0;

  if (isNewDatabase) {
    std::cout << "Creating new database: " << connectionString << std::endl;
  }

  // Enable WAL mode for better concurrent access
  char* errMsg = nullptr;
  rc = sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
  }

  // Create table if not exists (this handles both new and existing databases)
  if (!CreateTableIfNotExists()) {
    Disconnect();
    return false;
  }

  // Prepare statements
  if (!PrepareStatements()) {
    Disconnect();
    return false;
  }

  return true;
}

void SQLiteConnection::Disconnect() {
  if (m_insertStmt) {
    sqlite3_finalize(m_insertStmt);
    m_insertStmt = nullptr;
  }

  if (m_db) {
    sqlite3_close(m_db);
    m_db = nullptr;
  }
}

bool SQLiteConnection::IsConnected() const {
  return m_db != nullptr;
}

bool SQLiteConnection::CreateTableIfNotExists() {
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS dut_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      serial_number TEXT NOT NULL,
      key TEXT NOT NULL,
      value REAL NOT NULL,
      timestamp TEXT NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );
    
    CREATE INDEX IF NOT EXISTS idx_serial_number ON dut_data(serial_number);
    CREATE INDEX IF NOT EXISTS idx_timestamp ON dut_data(timestamp);
    CREATE INDEX IF NOT EXISTS idx_key ON dut_data(key);
  )";

  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
    return false;
  }

  return true;
}

bool SQLiteConnection::PrepareStatements() {
  const char* sql = "INSERT INTO dut_data (serial_number, key, value, timestamp) VALUES (?, ?, ?, ?);";

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &m_insertStmt, nullptr);
  if (rc != SQLITE_OK) {
    m_lastError = sqlite3_errmsg(m_db);
    return false;
  }

  return true;
}

bool SQLiteConnection::BeginTransaction() {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool SQLiteConnection::CommitTransaction() {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool SQLiteConnection::RollbackTransaction() {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool SQLiteConnection::InsertDataPoint(
  const std::string& serialNumber,
  const std::string& key,
  double value,
  const std::chrono::system_clock::time_point& timestamp) {

  // Convert timestamp to string
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    timestamp.time_since_epoch()) % 1000;

	std::tm timeinfo;
	localtime_s(&timeinfo, &time_t);

  std::stringstream ss;
  ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
    << "." << std::setfill('0') << std::setw(3) << ms.count();
  std::string timestampStr = ss.str();

  // Bind parameters
  sqlite3_reset(m_insertStmt);
  sqlite3_bind_text(m_insertStmt, 1, serialNumber.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(m_insertStmt, 2, key.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_double(m_insertStmt, 3, value);
  sqlite3_bind_text(m_insertStmt, 4, timestampStr.c_str(), -1, SQLITE_STATIC);

  // Execute
  int rc = sqlite3_step(m_insertStmt);
  if (rc != SQLITE_DONE) {
    m_lastError = sqlite3_errmsg(m_db);
    return false;
  }

  return true;
}

std::string SQLiteConnection::GetLastError() const {
  return m_lastError;
}