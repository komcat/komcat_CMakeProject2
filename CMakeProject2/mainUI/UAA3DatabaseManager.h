#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

// Forward declaration for SQLite
struct sqlite3;

namespace UAA3 {

    /**
     * @brief Result structure for database operations
     */
    struct DatabaseResult {
        bool success = false;
        std::string errorMessage;
        std::string details;

        DatabaseResult() = default;
        DatabaseResult(bool s, const std::string& error = "", const std::string& det = "")
            : success(s), errorMessage(error), details(det) {
        }
    };

    /**
     * @brief Configuration table information
     */
    struct ConfigTableInfo {
        std::string tableName;
        std::string originalFilename;
        std::string createdTimestamp;
        std::string lastModified;
        size_t recordCount = 0;
        bool isValid = false;
    };

    /**
     * @brief UAA3 Application Database Manager
     *
     * Manages SQLite database operations for JSON configuration files.
     * Each JSON configuration file becomes a table in the "allicareisyou" database.
     */
    class UAA3DatabaseManager {
    private:
        sqlite3* m_database = nullptr;
        std::string m_databasePath;
        std::string m_databaseName = "allicareisyou";
        bool m_isInitialized = false;

        // Internal helper methods (implemented in UAA3DatabaseManager.cpp)
        bool CreateMetadataTable();
        bool TableExists(const std::string& tableName);
        std::string SanitizeTableName(const std::string& filename);



    public:
        UAA3DatabaseManager() = default;
        ~UAA3DatabaseManager();

        // Core database operations (implemented in UAA3DatabaseManager.cpp)
        DatabaseResult InitializeDatabase(const std::string& databasePath = "");
        void CloseDatabase();
        bool IsInitialized() const { return m_isInitialized; }
        std::string GetDatabasePath() const { return m_databasePath; }

        // JSON operations (implemented in UAA3DatabaseManager_JsonOps.cpp)
        DatabaseResult TransferJsonToDatabase(const std::string& jsonFilePath, bool overwriteExisting = false);
        DatabaseResult ExportTableToJson(const std::string& tableName, const std::string& outputJsonPath, bool includeMetadata = false);
        DatabaseResult ExportTableToOriginalFile(const std::string& tableName, const std::string& outputDirectory = "");
        DatabaseResult UpdateTableData(const std::string& tableName, const nlohmann::json& jsonData);
        nlohmann::json GetTableAsJson(const std::string& tableName);

        // Query and management operations (implemented in UAA3DatabaseManager.cpp)
        std::vector<ConfigTableInfo> GetAllConfigTables();
        bool DoesTableExist(const std::string& tableName);
        ConfigTableInfo GetTableInfo(const std::string& tableName);
        DatabaseResult DeleteTable(const std::string& tableName);

        // Utility functions (implemented in UAA3DatabaseManager_Utils.cpp)
        nlohmann::json GetDatabaseStats();
        DatabaseResult CreateBackup(const std::string& backupPath = "");
        DatabaseResult ValidateDatabase();
        std::vector<DatabaseResult> TransferAllUAA3Configs(const std::string& configDirectory = "", bool overwriteExisting = false);
        std::vector<DatabaseResult> ExportAllTablesToFiles(const std::string& outputDirectory = "");

        // JSON operation helpers (implemented in UAA3DatabaseManager_JsonOps.cpp)
        std::string GenerateCreateTableSQL(const nlohmann::json& jsonData, const std::string& tableName);
        DatabaseResult InsertJsonData(const std::string& tableName, const nlohmann::json& jsonData);
        DatabaseResult UpdateMetadata(const std::string& tableName, const std::string& filename);
        bool IsOldFormatTable(const std::string& tableName);
        DatabaseResult MigrateTableToNewFormat(const std::string& tableName);
        nlohmann::json GetTableAsJsonOldFormat(const std::string& tableName);
        std::vector<DatabaseResult> MigrateAllTablesToNewFormat();
    };

    // Global convenience functions (implemented in UAA3DatabaseManager_Global.cpp)
    UAA3DatabaseManager& GetDatabaseManager();
    DatabaseResult InitializeGlobalDatabase(const std::string& databasePath = "");
    DatabaseResult QuickTransferConfig(const std::string& jsonFilePath, bool overwrite = false);
    DatabaseResult QuickExportConfig(const std::string& tableName, const std::string& outputPath);

} // namespace UAA3