#include "ModulePreferencesDB.h"
#include "include/logger.h"
#include <iostream>

ModulePreferencesDB::ModulePreferencesDB()
    : m_db(nullptr), m_initialized(false) {
}

ModulePreferencesDB::~ModulePreferencesDB() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool ModulePreferencesDB::Initialize(const std::string& dbPath) {
    Logger* logger = Logger::GetInstance();

    // Close existing connection if any
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }

    // Open database
    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        logger->LogError("Failed to open SQLite database: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // Create tables if needed
    if (!CreateTablesIfNeeded()) {
        logger->LogError("Failed to create database tables");
        return false;
    }

    m_initialized = true;
    logger->LogInfo("Module preferences database initialized: " + dbPath);
    return true;
}

bool ModulePreferencesDB::CreateTablesIfNeeded() {
    const std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS module_preferences (
            module_name TEXT PRIMARY KEY,
            enabled INTEGER NOT NULL DEFAULT 0,
            last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    return ExecuteSQL(createTableSQL);
}

bool ModulePreferencesDB::ExecuteSQL(const std::string& sql) {
    if (!m_db) {
        return false;
    }

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);

    if (result != SQLITE_OK) {
        Logger* logger = Logger::GetInstance();
        logger->LogError("SQL execution failed: " + std::string(errorMsg ? errorMsg : "Unknown error"));
        if (errorMsg) {
            sqlite3_free(errorMsg);
        }
        return false;
    }

    return true;
}

bool ModulePreferencesDB::SaveModulePreference(const std::string& moduleName, bool enabled) {
    if (!m_initialized || !m_db) {
        return false;
    }

    const std::string sql = R"(
        INSERT OR REPLACE INTO module_preferences (module_name, enabled, last_updated)
        VALUES (?, ?, CURRENT_TIMESTAMP);
    )";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        Logger::GetInstance()->LogError("Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, moduleName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, enabled ? 1 : 0);

    // Execute
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        Logger::GetInstance()->LogError("Failed to save module preference: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    Logger::GetInstance()->LogInfo("Saved preference: " + moduleName + " = " + (enabled ? "enabled" : "disabled"));
    return true;
}

bool ModulePreferencesDB::LoadModulePreference(const std::string& moduleName, bool defaultValue) {
    if (!m_initialized || !m_db) {
        return defaultValue;
    }

    const std::string sql = "SELECT enabled FROM module_preferences WHERE module_name = ?;";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        Logger::GetInstance()->LogError("Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(m_db)));
        return defaultValue;
    }

    // Bind parameter
    sqlite3_bind_text(stmt, 1, moduleName.c_str(), -1, SQLITE_STATIC);

    // Execute and get result
    bool preference = defaultValue;
    result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
        int enabled = sqlite3_column_int(stmt, 0);
        preference = (enabled == 1);
    }
    else if (result != SQLITE_DONE) {
        Logger::GetInstance()->LogError("Failed to load module preference: " + std::string(sqlite3_errmsg(m_db)));
    }

    sqlite3_finalize(stmt);
    return preference;
}

bool ModulePreferencesDB::SaveAllPreferences(const std::map<std::string, bool>& preferences) {
    if (!m_initialized || !m_db) {
        return false;
    }

    // Use transaction for better performance
    if (!ExecuteSQL("BEGIN TRANSACTION;")) {
        return false;
    }

    bool success = true;
    for (const auto& [moduleName, enabled] : preferences) {
        if (!SaveModulePreference(moduleName, enabled)) {
            success = false;
            break;
        }
    }

    if (success) {
        success = ExecuteSQL("COMMIT;");
    }
    else {
        ExecuteSQL("ROLLBACK;");
    }

    return success;
}

std::map<std::string, bool> ModulePreferencesDB::LoadAllPreferences() {
    std::map<std::string, bool> preferences;

    if (!m_initialized || !m_db) {
        return preferences;
    }

    const std::string sql = "SELECT module_name, enabled FROM module_preferences;";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        Logger::GetInstance()->LogError("Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(m_db)));
        return preferences;
    }

    // Execute and collect all results
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* moduleName = (const char*)sqlite3_column_text(stmt, 0);
        int enabled = sqlite3_column_int(stmt, 1);

        if (moduleName) {
            preferences[std::string(moduleName)] = (enabled == 1);
        }
    }

    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        Logger::GetInstance()->LogError("Failed to load all preferences: " + std::string(sqlite3_errmsg(m_db)));
        preferences.clear();
    }

    Logger::GetInstance()->LogInfo("Loaded " + std::to_string(preferences.size()) + " module preferences from database");
    return preferences;
}

bool ModulePreferencesDB::ResetAllPreferences() {
    if (!m_initialized || !m_db) {
        return false;
    }

    const std::string sql = "DELETE FROM module_preferences;";
    bool success = ExecuteSQL(sql);

    if (success) {
        Logger::GetInstance()->LogInfo("All module preferences have been reset");
    }

    return success;
}