// DatabaseManager.cpp
#include "DatabaseManager.h"
#include <filesystem>
#include <iostream>
#include <sstream>

DatabaseManager::DatabaseManager()
  : m_db(nullptr, sqlite3_close), m_isConnected(false) {
}

DatabaseManager::~DatabaseManager() {
  Close();
}

bool DatabaseManager::Initialize(const std::string& dbPath) {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  try {
    // Create db/ directory if it doesn't exist
    std::filesystem::path path(dbPath);
    std::filesystem::create_directories(path.parent_path());

    // Open database
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);

    if (rc != SQLITE_OK) {
      m_lastError = "Cannot open database: " + std::string(sqlite3_errmsg(db));
      if (db) sqlite3_close(db);
      return false;
    }

    m_db.reset(db);
    m_isConnected = true;

    // Configure database
    EnableWALMode();
    EnableForeignKeys();

    // Create tables and indexes
    if (!CreateTables() || !CreateIndexes()) {
      m_lastError = "Failed to create database schema";
      return false;
    }

    return true;

  }
  catch (const std::exception& e) {
    m_lastError = "Exception during database initialization: " + std::string(e.what());
    return false;
  }
}

bool DatabaseManager::Close() {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (m_db) {
    m_db.reset();
    m_isConnected = false;
  }
  return true;
}

bool DatabaseManager::IsConnected() const {
  return m_isConnected && m_db != nullptr;
}

bool DatabaseManager::CreateTables() {
  const std::vector<std::string> createQueries = {
    // Operations table - WITH caller context
    R"(
        CREATE TABLE IF NOT EXISTS operations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            operation_id TEXT UNIQUE NOT NULL,
            method_name TEXT NOT NULL,
            device_name TEXT,
            caller_context TEXT,        -- NEW: Which operation class called this
            sequence_name TEXT,         -- NEW: Which sequence this belongs to
            status TEXT NOT NULL DEFAULT 'running',
            start_time TEXT NOT NULL,
            end_time TEXT,
            elapsed_time_ms INTEGER,
            error_message TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
        )",

    // Operation results table (key-value pairs)
    R"(
        CREATE TABLE IF NOT EXISTS operation_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            operation_id TEXT NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(operation_id) REFERENCES operations(operation_id) ON DELETE CASCADE
        )
        )"
  };

  for (const auto& query : createQueries) {
    if (!ExecuteInternal(query)) {
      return false;
    }
  }

  return true;
}

bool DatabaseManager::CreateIndexes() {
  const std::vector<std::string> indexQueries = {
      "CREATE INDEX IF NOT EXISTS idx_operations_method ON operations(method_name)",
      "CREATE INDEX IF NOT EXISTS idx_operations_device ON operations(device_name)",
      "CREATE INDEX IF NOT EXISTS idx_operations_caller ON operations(caller_context)",      // NEW
      "CREATE INDEX IF NOT EXISTS idx_operations_sequence ON operations(sequence_name)",     // NEW
      "CREATE INDEX IF NOT EXISTS idx_operations_status ON operations(status)",
      "CREATE INDEX IF NOT EXISTS idx_operations_start_time ON operations(start_time)",
      "CREATE INDEX IF NOT EXISTS idx_operation_results_operation_id ON operation_results(operation_id)",
      "CREATE INDEX IF NOT EXISTS idx_operation_results_key ON operation_results(key)",
      "CREATE INDEX IF NOT EXISTS idx_operation_results_timestamp ON operation_results(timestamp)"
  };

  for (const auto& query : indexQueries) {
    if (!ExecuteInternal(query)) {
      return false;
    }
  }

  return true;
}

bool DatabaseManager::DropTables() {
  const std::vector<std::string> dropQueries = {
      "DROP TABLE IF EXISTS operation_results",
      "DROP TABLE IF EXISTS operations"
  };

  for (const auto& query : dropQueries) {
    if (!ExecuteInternal(query)) {
      return false;
    }
  }

  return true;
}

bool DatabaseManager::ExecuteQuery(const std::string& query) {
  std::lock_guard<std::mutex> lock(m_dbMutex);
  return ExecuteInternal(query);
}

bool DatabaseManager::ExecuteQuery(const std::string& query,
  std::vector<std::vector<std::string>>& results) {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (!IsConnected()) {
    m_lastError = "Database not connected";
    return false;
  }

  results.clear();

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_db.get(), query.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    m_lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db.get()));
    return false;
  }

  // Get column count
  int columnCount = sqlite3_column_count(stmt);

  // Execute and fetch results
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    std::vector<std::string> row;
    for (int i = 0; i < columnCount; i++) {
      const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
      row.push_back(value ? value : "");
    }
    results.push_back(row);
  }

  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    m_lastError = "Error executing query: " + std::string(sqlite3_errmsg(m_db.get()));
    return false;
  }

  return true;
}

bool DatabaseManager::ExecutePreparedStatement(const std::string& query,
  const std::vector<std::string>& parameters) {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (!IsConnected()) {
    m_lastError = "Database not connected";
    return false;
  }

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_db.get(), query.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    m_lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db.get()));
    return false;
  }

  // Bind parameters
  for (size_t i = 0; i < parameters.size(); i++) {
    rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), parameters[i].c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
      m_lastError = "Failed to bind parameter: " + std::string(sqlite3_errmsg(m_db.get()));
      sqlite3_finalize(stmt);
      return false;
    }
  }

  // Execute
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    m_lastError = "Error executing prepared statement: " + std::string(sqlite3_errmsg(m_db.get()));
    return false;
  }

  return true;
}

bool DatabaseManager::InsertRecord(const std::string& table,
  const std::map<std::string, std::string>& data) {
  if (data.empty()) {
    m_lastError = "No data provided for insert";
    return false;
  }

  std::stringstream query;
  query << "INSERT INTO " << table << " (";

  std::vector<std::string> columns;
  std::vector<std::string> values;

  for (const auto& pair : data) {
    columns.push_back(pair.first);
    values.push_back(pair.second);
  }

  // Build column list
  for (size_t i = 0; i < columns.size(); i++) {
    if (i > 0) query << ", ";
    query << columns[i];
  }

  query << ") VALUES (";

  // Build placeholders
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0) query << ", ";
    query << "?";
  }

  query << ")";

  return ExecutePreparedStatement(query.str(), values);
}

std::vector<std::map<std::string, std::string>> DatabaseManager::SelectRecords(
  const std::string& table,
  const std::string& whereClause,
  const std::string& orderBy,
  int limit) {

  std::vector<std::map<std::string, std::string>> records;

  std::stringstream query;
  query << "SELECT * FROM " << table;

  if (!whereClause.empty()) {
    query << " WHERE " << whereClause;
  }

  if (!orderBy.empty()) {
    query << " ORDER BY " << orderBy;
  }

  if (limit > 0) {
    query << " LIMIT " << limit;
  }

  std::vector<std::vector<std::string>> results;
  if (!ExecuteQuery(query.str(), results)) {
    return records;
  }

  // Get column names
  std::string columnQuery = "PRAGMA table_info(" + table + ")";
  std::vector<std::vector<std::string>> columnInfo;
  if (!ExecuteQuery(columnQuery, columnInfo)) {
    return records;
  }

  std::vector<std::string> columnNames;
  for (const auto& info : columnInfo) {
    if (info.size() >= 2) {
      columnNames.push_back(info[1]); // Column name is at index 1
    }
  }

  // Convert results to map format
  for (const auto& row : results) {
    std::map<std::string, std::string> record;
    for (size_t i = 0; i < row.size() && i < columnNames.size(); i++) {
      record[columnNames[i]] = row[i];
    }
    records.push_back(record);
  }

  return records;
}

std::string DatabaseManager::GetLastError() const {
  return m_lastError;
}

void DatabaseManager::EnableWALMode() {
  ExecuteInternal("PRAGMA journal_mode = WAL");
}

void DatabaseManager::EnableForeignKeys() {
  ExecuteInternal("PRAGMA foreign_keys = ON");
}

bool DatabaseManager::BeginTransaction() {
  return ExecuteInternal("BEGIN TRANSACTION");
}

bool DatabaseManager::CommitTransaction() {
  return ExecuteInternal("COMMIT");
}

bool DatabaseManager::RollbackTransaction() {
  return ExecuteInternal("ROLLBACK");
}

bool DatabaseManager::Vacuum() {
  return ExecuteInternal("VACUUM");
}

bool DatabaseManager::Analyze() {
  return ExecuteInternal("ANALYZE");
}

int64_t DatabaseManager::GetDatabaseSize() {
  std::vector<std::vector<std::string>> results;
  if (ExecuteQuery("PRAGMA page_count", results) && !results.empty() && !results[0].empty()) {
    try {
      int64_t pageCount = std::stoll(results[0][0]);

      if (ExecuteQuery("PRAGMA page_size", results) && !results.empty() && !results[0].empty()) {
        int64_t pageSize = std::stoll(results[0][0]);
        return pageCount * pageSize;
      }
    }
    catch (const std::exception&) {
      // Fall through to return -1
    }
  }
  return -1;
}

bool DatabaseManager::ExecuteInternal(const std::string& query) {
  if (!IsConnected()) {
    m_lastError = "Database not connected";
    return false;
  }

  char* errorMsg = nullptr;
  int rc = sqlite3_exec(m_db.get(), query.c_str(), nullptr, nullptr, &errorMsg);

  if (rc != SQLITE_OK) {
    m_lastError = "SQL error: " + std::string(errorMsg ? errorMsg : "Unknown error");
    if (errorMsg) sqlite3_free(errorMsg);
    return false;
  }

  return true;
}

std::string DatabaseManager::EscapeString(const std::string& input) {
  std::string escaped;
  escaped.reserve(input.length() * 2);

  for (char c : input) {
    if (c == '\'') {
      escaped += "''";
    }
    else {
      escaped += c;
    }
  }

  return escaped;
}