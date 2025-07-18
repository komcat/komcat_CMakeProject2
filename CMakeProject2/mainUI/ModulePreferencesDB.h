#pragma once

#include <string>
#include <map>
#include <sqlite3.h>

class ModulePreferencesDB {
public:
    ModulePreferencesDB();
    ~ModulePreferencesDB();

    // Initialize database (creates tables if needed)
    bool Initialize(const std::string& dbPath = "module_preferences.db");

    // Save module preference (enabled/disabled)
    bool SaveModulePreference(const std::string& moduleName, bool enabled);

    // Load module preference (returns false if not found, default disabled)
    bool LoadModulePreference(const std::string& moduleName, bool defaultValue = false);

    // Save all preferences at once
    bool SaveAllPreferences(const std::map<std::string, bool>& preferences);

    // Load all preferences
    std::map<std::string, bool> LoadAllPreferences();

    // Reset all preferences to default
    bool ResetAllPreferences();

    // Check if database is properly initialized
    bool IsInitialized() const { return m_initialized; }

private:
    sqlite3* m_db;
    bool m_initialized;

    // Helper methods
    bool CreateTablesIfNeeded();
    bool ExecuteSQL(const std::string& sql);
};