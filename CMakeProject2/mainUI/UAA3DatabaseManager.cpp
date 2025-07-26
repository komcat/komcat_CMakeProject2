#include "UAA3DatabaseManager.h"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <regex>

namespace UAA3 {

    // Destructor
    UAA3DatabaseManager::~UAA3DatabaseManager() {
        CloseDatabase();
    }

    // Initialize database
    DatabaseResult UAA3DatabaseManager::InitializeDatabase(const std::string& databasePath) {
        // Close existing connection if any
        CloseDatabase();

        // Determine database path
        if (databasePath.empty()) {
            m_databasePath = m_databaseName + ".db";
        }
        else {
            // Ensure directory exists
            std::filesystem::path dbPath(databasePath);
            if (dbPath.is_relative()) {
                dbPath = std::filesystem::current_path() / dbPath;
            }

            // If path is a directory, append database filename
            if (std::filesystem::is_directory(dbPath) || dbPath.filename().extension().empty()) {
                dbPath /= (m_databaseName + ".db");
            }

            // Create directory if it doesn't exist
            std::filesystem::create_directories(dbPath.parent_path());
            m_databasePath = dbPath.string();
        }

        // Open/create database
        int result = sqlite3_open(m_databasePath.c_str(), &m_database);
        if (result != SQLITE_OK) {
            std::string error = "Failed to open database: " + std::string(sqlite3_errmsg(m_database));
            sqlite3_close(m_database);
            m_database = nullptr;
            return DatabaseResult(false, error, "SQLite error code: " + std::to_string(result));
        }

        // Enable foreign keys
        char* errMsg = nullptr;
        result = sqlite3_exec(m_database, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::string error = "Failed to enable foreign keys: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        // Create metadata table
        if (!CreateMetadataTable()) {
            return DatabaseResult(false, "Failed to create metadata table");
        }

        m_isInitialized = true;
        return DatabaseResult(true, "", "Database initialized at: " + m_databasePath);
    }

    // Create metadata table
    bool UAA3DatabaseManager::CreateMetadataTable() {
        const char* sql = R"(
        CREATE TABLE IF NOT EXISTS config_metadata (
            table_name TEXT PRIMARY KEY,
            original_filename TEXT NOT NULL,
            created_timestamp TEXT NOT NULL,
            last_modified TEXT NOT NULL,
            record_count INTEGER DEFAULT 0,
            json_structure TEXT,
            checksum TEXT
        );
    )";

        char* errMsg = nullptr;
        int result = sqlite3_exec(m_database, sql, nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::cerr << "Failed to create metadata table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    // Check if table exists
    bool UAA3DatabaseManager::TableExists(const std::string& tableName) {
        if (!m_database) {
            return false;
        }

        const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
        sqlite3_stmt* stmt;

        int result = sqlite3_prepare_v2(m_database, sql, -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
        result = sqlite3_step(stmt);

        bool exists = (result == SQLITE_ROW);
        sqlite3_finalize(stmt);

        return exists;
    }

    // Sanitize table name
    std::string UAA3DatabaseManager::SanitizeTableName(const std::string& filename) {
        std::filesystem::path path(filename);
        std::string baseName = path.stem().string();

        // Replace invalid characters with underscores
        std::regex invalidChars("[^a-zA-Z0-9_]");
        std::string sanitized = std::regex_replace(baseName, invalidChars, "_");

        // Ensure it starts with a letter or underscore
        if (!sanitized.empty() && std::isdigit(sanitized[0])) {
            sanitized = "config_" + sanitized;
        }

        // Ensure it's not empty
        if (sanitized.empty()) {
            sanitized = "config_table";
        }

        return sanitized;
    }

    // Close database
    void UAA3DatabaseManager::CloseDatabase() {
        if (m_database) {
            sqlite3_close(m_database);
            m_database = nullptr;
        }
        m_isInitialized = false;
    }

    // Get all config tables
    std::vector<ConfigTableInfo> UAA3DatabaseManager::GetAllConfigTables() {
        std::vector<ConfigTableInfo> tables;

        if (!m_isInitialized) {
            return tables;
        }

        const char* sql = "SELECT table_name, original_filename, created_timestamp, last_modified, record_count FROM config_metadata;";
        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_database, sql, -1, &stmt, nullptr);

        if (result != SQLITE_OK) {
            return tables;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConfigTableInfo info;
            info.tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            info.originalFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info.createdTimestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            info.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.recordCount = sqlite3_column_int(stmt, 4);
            info.isValid = true;

            tables.push_back(info);
        }

        sqlite3_finalize(stmt);
        return tables;
    }

    // Check if table exists (public method)
    bool UAA3DatabaseManager::DoesTableExist(const std::string& tableName) {
        return TableExists(tableName);
    }

    // Get table info
    ConfigTableInfo UAA3DatabaseManager::GetTableInfo(const std::string& tableName) {
        ConfigTableInfo info;

        if (!m_isInitialized) {
            return info;
        }

        const char* sql = "SELECT table_name, original_filename, created_timestamp, last_modified, record_count FROM config_metadata WHERE table_name = ?;";
        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_database, sql, -1, &stmt, nullptr);

        if (result != SQLITE_OK) {
            return info;
        }

        sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            info.originalFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info.createdTimestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            info.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.recordCount = sqlite3_column_int(stmt, 4);
            info.isValid = true;
        }

        sqlite3_finalize(stmt);
        return info;
    }

    // Delete table
    DatabaseResult UAA3DatabaseManager::DeleteTable(const std::string& tableName) {
        if (!m_isInitialized) {
            return DatabaseResult(false, "Database not initialized");
        }

        if (!TableExists(tableName)) {
            return DatabaseResult(false, "Table does not exist: " + tableName);
        }

        // Begin transaction
        char* errMsg = nullptr;
        int result = sqlite3_exec(m_database, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::string error = "Failed to begin transaction: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        // Delete from metadata table
        std::string deleteMetadataSql = "DELETE FROM config_metadata WHERE table_name = ?;";
        sqlite3_stmt* stmt;
        result = sqlite3_prepare_v2(m_database, deleteMetadataSql.c_str(), -1, &stmt, nullptr);
        if (result == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Drop the table
        std::string dropTableSql = "DROP TABLE " + tableName + ";";
        result = sqlite3_exec(m_database, dropTableSql.c_str(), nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            sqlite3_exec(m_database, "ROLLBACK;", nullptr, nullptr, nullptr);
            std::string error = "Failed to drop table: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        // Commit transaction
        result = sqlite3_exec(m_database, "COMMIT;", nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::string error = "Failed to commit transaction: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        return DatabaseResult(true, "", "Successfully deleted table: " + tableName);
    }

} // namespace UAA3