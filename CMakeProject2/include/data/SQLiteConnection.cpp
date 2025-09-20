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
  // Check if database file already exists
  bool isExistingDatabase = std::filesystem::exists(connectionString) &&
    std::filesystem::file_size(connectionString) > 0;

  // SQLite automatically creates the database file if it doesn't exist
  int rc = sqlite3_open(connectionString.c_str(), &m_db);
  if (rc != SQLITE_OK) {
    m_lastError = sqlite3_errmsg(m_db);
    sqlite3_close(m_db);
    m_db = nullptr;
    return false;
  }

  if (!isExistingDatabase) {
    std::cout << "Creating new database: " << connectionString << std::endl;
  }

  // Enable WAL mode for better concurrent access
  char* errMsg = nullptr;
  rc = sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
  }

  // Handle database creation or upgrade
  if (isExistingDatabase) {
    // Upgrade existing database if needed
    if (!UpgradeDatabase()) {
      Disconnect();
      return false;
    }
  }
  else {
    // Create new database with latest schema
    if (!CreateTableIfNotExists()) {
      Disconnect();
      return false;
    }
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
  // Updated schema with label column
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS dut_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      serial_number TEXT NOT NULL,
      key TEXT NOT NULL,
      value REAL NOT NULL,
      timestamp TEXT NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      label TEXT DEFAULT ''
    );
    
    CREATE INDEX IF NOT EXISTS idx_serial_number ON dut_data(serial_number);
    CREATE INDEX IF NOT EXISTS idx_timestamp ON dut_data(timestamp);
    CREATE INDEX IF NOT EXISTS idx_key ON dut_data(key);
    CREATE INDEX IF NOT EXISTS idx_label ON dut_data(label);
  )";

  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    m_lastError = errMsg;
    sqlite3_free(errMsg);
    return false;
  }

  std::cout << "Database schema created with label support" << std::endl;
  return true;
}

bool SQLiteConnection::UpgradeDatabase() {
  // Check if label column exists
  sqlite3_stmt* stmt;
  const char* checkColumnSQL = "PRAGMA table_info(dut_data);";

  int rc = sqlite3_prepare_v2(m_db, checkColumnSQL, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    m_lastError = sqlite3_errmsg(m_db);
    return false;
  }

  bool hasLabelColumn = false;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char* columnName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (columnName && std::string(columnName) == "label") {
      hasLabelColumn = true;
      break;
    }
  }
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    m_lastError = sqlite3_errmsg(m_db);
    return false;
  }

  // Add label column if it doesn't exist
  if (!hasLabelColumn) {
    std::cout << "Upgrading database: Adding label column..." << std::endl;

    const char* addColumnSQL = R"(
      ALTER TABLE dut_data ADD COLUMN label TEXT DEFAULT '';
      CREATE INDEX IF NOT EXISTS idx_label ON dut_data(label);
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(m_db, addColumnSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
      m_lastError = errMsg;
      sqlite3_free(errMsg);
      return false;
    }

    std::cout << "Database upgrade completed: label column added" << std::endl;
  }
  else {
    std::cout << "Database schema is up to date" << std::endl;
  }

  return true;
}

bool SQLiteConnection::PrepareStatements() {
  // Updated SQL to include label parameter
  const char* sql = "INSERT INTO dut_data (serial_number, key, value, timestamp, label) VALUES (?, ?, ?, ?, ?);";

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
  const std::chrono::system_clock::time_point& timestamp,
  const std::string& label) {

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

  // Bind parameters (now includes label as 5th parameter)
  sqlite3_reset(m_insertStmt);
  sqlite3_bind_text(m_insertStmt, 1, serialNumber.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(m_insertStmt, 2, key.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_double(m_insertStmt, 3, value);
  sqlite3_bind_text(m_insertStmt, 4, timestampStr.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(m_insertStmt, 5, label.c_str(), -1, SQLITE_STATIC);  // NEW: bind label

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

// SQLiteConnection.cpp - Add this method to the implementation

std::string SQLiteConnection::GetLatestSerialNumber() {
  if (!m_db) {
    return "";
  }

  const char* sql = "SELECT serial_number FROM dut_data ORDER BY created_at DESC LIMIT 1;";

  sqlite3_stmt* stmt;
  std::string latestSerial = "";

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    m_lastError = "Failed to prepare query for latest serial: " + std::string(sqlite3_errmsg(m_db));
    return "";
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (result) {
      latestSerial = std::string(result);
    }
  }
  else if (rc != SQLITE_DONE) {
    m_lastError = "Error executing latest serial query: " + std::string(sqlite3_errmsg(m_db));
  }

  sqlite3_finalize(stmt);
  return latestSerial;
}