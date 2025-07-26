#include "ConfigDatabaseUtils.h"
#include <iostream>
#include <filesystem>

namespace ConfigDatabaseUtils {

    // Static flag to track database initialization
    static bool g_databaseInitialized = false;

    bool InitializeDatabase(const std::string& databasePath, Logger* logger) {
        if (g_databaseInitialized) {
            if (logger) logger->LogInfo("Database already initialized");
            return true;
        }

        UAA3::DatabaseResult result = UAA3::InitializeGlobalDatabase(databasePath);
        if (result.success) {
            g_databaseInitialized = true;
            if (logger) {
                logger->LogInfo("UAA3DatabaseManager initialized successfully");
                if (!result.details.empty()) {
                    logger->LogInfo(result.details);
                }
            }
            return true;
        }
        else {
            if (logger) {
                logger->LogError("Failed to initialize UAA3DatabaseManager: " + result.errorMessage);
                if (!result.details.empty()) {
                    logger->LogError("Details: " + result.details);
                }
            }
            return false;
        }
    }

    ConfigLoadResult LoadConfigWithDatabase(const std::string& configFileName, Logger* logger) {
        // Initialize database if not already done
        if (!InitializeDatabase("", logger)) {
            // Database failed, check if file exists
            if (std::filesystem::exists(configFileName)) {
                if (logger) logger->LogInfo("Database unavailable, using file: " + configFileName);
                return ConfigLoadResult(true, false, false, "file", "Loaded from file (database unavailable)");
            }
            else {
                if (logger) logger->LogError("Neither database nor file available for: " + configFileName);
                return ConfigLoadResult(false, false, false, "none", "Configuration not found");
            }
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::string tableName = FilenameToTableName(configFileName);

        // Check if table exists in database
        if (dbManager.DoesTableExist(tableName)) {
            if (logger) logger->LogInfo("Found " + tableName + " table in database - loading from database");

            // Export from database to JSON file
            UAA3::DatabaseResult exportResult = dbManager.ExportTableToOriginalFile(tableName, ".");
            if (exportResult.success) {
                if (logger) logger->LogInfo("Successfully loaded " + configFileName + " from database");
                return ConfigLoadResult(true, true, false, "database", "Loaded from database");
            }
            else {
                if (logger) logger->LogWarning("Failed to export " + tableName + " from database: " + exportResult.errorMessage);
                // Fall through to check JSON file
            }
        }

        // Table doesn't exist or export failed - check for JSON file
        if (std::filesystem::exists(configFileName)) {
            if (logger) logger->LogInfo("Loading " + configFileName + " from file system");

            // Try to save to database for next time
            UAA3::DatabaseResult transferResult = dbManager.TransferJsonToDatabase(configFileName, false);
            if (transferResult.success) {
                if (logger) logger->LogInfo("Transferred " + configFileName + " to database for future use");
                return ConfigLoadResult(true, false, true, "file", "Loaded from file and saved to database");
            }
            else {
                if (logger) logger->LogWarning("Could not transfer " + configFileName + " to database: " + transferResult.errorMessage);
                return ConfigLoadResult(true, false, false, "file", "Loaded from file only");
            }
        }

        if (logger) logger->LogError("Neither database table nor JSON file found for " + configFileName);
        return ConfigLoadResult(false, false, false, "none", "Configuration not found");
    }

    bool SaveConfigToDatabase(const std::string& configFileName, bool overwrite, Logger* logger) {
        if (!std::filesystem::exists(configFileName)) {
            if (logger) logger->LogError("Cannot save to database - JSON file does not exist: " + configFileName);
            return false;
        }

        if (!InitializeDatabase("", logger)) {
            if (logger) logger->LogError("Database not available - cannot save config");
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        UAA3::DatabaseResult result = dbManager.TransferJsonToDatabase(configFileName, overwrite);

        if (result.success) {
            if (logger) logger->LogInfo("Successfully saved " + configFileName + " to database");
            return true;
        }
        else {
            if (logger) logger->LogError("Failed to save " + configFileName + " to database: " + result.errorMessage);
            return false;
        }
    }

    std::vector<ConfigLoadResult> LoadMultipleConfigs(const std::vector<std::string>& configFileNames, Logger* logger) {
        std::vector<ConfigLoadResult> results;

        if (logger) logger->LogInfo("Loading " + std::to_string(configFileNames.size()) + " configuration files with database integration");

        for (const std::string& configFile : configFileNames) {
            ConfigLoadResult result = LoadConfigWithDatabase(configFile, logger);
            results.push_back(result);
        }

        // Summary logging
        if (logger) {
            int successful = 0, fromDatabase = 0, fromFile = 0, savedToDb = 0;
            for (const auto& result : results) {
                if (result.success) successful++;
                if (result.loadedFromDatabase) fromDatabase++;
                if (result.source == "file") fromFile++;
                if (result.savedToDatabase) savedToDb++;
            }

            logger->LogInfo("Configuration loading summary:");
            logger->LogInfo("  Total files: " + std::to_string(configFileNames.size()));
            logger->LogInfo("  Successful: " + std::to_string(successful));
            logger->LogInfo("  From database: " + std::to_string(fromDatabase));
            logger->LogInfo("  From files: " + std::to_string(fromFile));
            logger->LogInfo("  Saved to database: " + std::to_string(savedToDb));
        }

        return results;
    }

    bool ExportConfigFromDatabase(const std::string& configFileName, const std::string& outputDirectory, Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::string tableName = FilenameToTableName(configFileName);

        if (!dbManager.DoesTableExist(tableName)) {
            if (logger) logger->LogError("Configuration table does not exist: " + tableName);
            return false;
        }

        UAA3::DatabaseResult result = dbManager.ExportTableToOriginalFile(tableName, outputDirectory);

        if (result.success) {
            if (logger) logger->LogInfo("Successfully exported " + configFileName + " from database");
            return true;
        }
        else {
            if (logger) logger->LogError("Failed to export " + configFileName + ": " + result.errorMessage);
            return false;
        }
    }

    bool ConfigExistsInDatabase(const std::string& configFileName) {
        if (!g_databaseInitialized) {
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::string tableName = FilenameToTableName(configFileName);
        return dbManager.DoesTableExist(tableName);
    }

    UAA3::ConfigTableInfo GetConfigInfo(const std::string& configFileName) {
        UAA3::ConfigTableInfo info;

        if (!g_databaseInitialized) {
            return info;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::string tableName = FilenameToTableName(configFileName);
        return dbManager.GetTableInfo(tableName);
    }

    bool DeleteConfigFromDatabase(const std::string& configFileName, Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::string tableName = FilenameToTableName(configFileName);

        UAA3::DatabaseResult result = dbManager.DeleteTable(tableName);

        if (result.success) {
            if (logger) logger->LogInfo("Successfully deleted " + configFileName + " from database");
            return true;
        }
        else {
            if (logger) logger->LogError("Failed to delete " + configFileName + ": " + result.errorMessage);
            return false;
        }
    }

    bool CreateConfigBackup(const std::string& backupPath, Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        UAA3::DatabaseResult result = dbManager.CreateBackup(backupPath);

        if (result.success) {
            if (logger) logger->LogInfo("Configuration backup created successfully");
            if (!result.details.empty() && logger) {
                logger->LogInfo(result.details);
            }
            return true;
        }
        else {
            if (logger) logger->LogError("Failed to create backup: " + result.errorMessage);
            return false;
        }
    }

    bool ValidateConfigDatabase(Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return false;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        UAA3::DatabaseResult result = dbManager.ValidateDatabase();

        if (logger) {
            if (result.success) {
                logger->LogInfo("Database validation passed");
            }
            else {
                logger->LogError("Database validation failed: " + result.errorMessage);
            }

            if (!result.details.empty()) {
                logger->LogInfo("Validation details:\n" + result.details);
            }
        }

        return result.success;
    }

    void LogDatabaseStats(Logger* logger) {
        if (!g_databaseInitialized) {
            if (logger) logger->LogInfo("Database not initialized - no stats available");
            return;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        nlohmann::json stats = dbManager.GetDatabaseStats();

        if (!logger) {
            std::cout << "=== Database Configuration Stats ===" << std::endl;
            std::cout << stats.dump(2) << std::endl;
            return;
        }

        logger->LogInfo("=== Database Configuration Stats ===");

        if (stats.contains("database_path")) {
            logger->LogInfo("Database path: " + stats["database_path"].get<std::string>());
        }

        if (stats.contains("file_size_bytes")) {
            size_t size = stats["file_size_bytes"].get<size_t>();
            std::string sizeStr = (size > 1024 * 1024) ?
                std::to_string(size / (1024 * 1024)) + " MB" :
                std::to_string(size / 1024) + " KB";
            logger->LogInfo("File size: " + sizeStr);
        }

        if (stats.contains("table_count")) {
            logger->LogInfo("Total tables: " + std::to_string(stats["table_count"].get<size_t>()));
        }

        if (stats.contains("total_records")) {
            logger->LogInfo("Total records: " + std::to_string(stats["total_records"].get<int>()));
        }

        if (stats.contains("tables") && stats["tables"].is_array()) {
            logger->LogInfo("Configuration tables:");
            for (const auto& table : stats["tables"]) {
                std::string tableInfo = "  - " + table["name"].get<std::string>() +
                    " (" + std::to_string(table["records"].get<int>()) + " records";

                if (table.contains("original_filename")) {
                    tableInfo += ", file: " + table["original_filename"].get<std::string>();
                }

                if (table.contains("modified")) {
                    tableInfo += ", modified: " + table["modified"].get<std::string>();
                }

                tableInfo += ")";
                logger->LogInfo(tableInfo);
            }
        }

        logger->LogInfo("=== End Database Stats ===");
    }

    int TransferAllUAA3Configs(const std::string& configDirectory, bool overwriteExisting, Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return 0;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::vector<UAA3::DatabaseResult> results = dbManager.TransferAllUAA3Configs(configDirectory, overwriteExisting);

        int successCount = 0;
        for (const auto& result : results) {
            if (result.success) {
                successCount++;
                if (logger) logger->LogInfo(result.details);
            }
            else {
                if (logger) logger->LogWarning(result.errorMessage + " - " + result.details);
            }
        }

        if (logger) {
            logger->LogInfo("Transferred " + std::to_string(successCount) + " out of " +
                std::to_string(results.size()) + " configuration files to database");
        }

        return successCount;
    }

    int ExportAllConfigs(const std::string& outputDirectory, Logger* logger) {
        if (!InitializeDatabase("", logger)) {
            return 0;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::vector<UAA3::DatabaseResult> results = dbManager.ExportAllTablesToFiles(outputDirectory);

        int successCount = 0;
        for (const auto& result : results) {
            if (result.success) {
                successCount++;
                if (logger) logger->LogInfo(result.details);
            }
            else {
                if (logger) logger->LogError(result.errorMessage);
            }
        }

        if (logger) {
            logger->LogInfo("Exported " + std::to_string(successCount) + " out of " +
                std::to_string(results.size()) + " configurations from database");
        }

        return successCount;
    }

    std::vector<std::string> GetAllConfigNames() {
        std::vector<std::string> configNames;

        if (!g_databaseInitialized) {
            return configNames;
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        std::vector<UAA3::ConfigTableInfo> tables = dbManager.GetAllConfigTables();

        for (const auto& table : tables) {
            configNames.push_back(table.tableName);
        }

        return configNames;
    }

    std::string GetDatabasePath() {
        if (!g_databaseInitialized) {
            return "";
        }

        UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();
        return dbManager.GetDatabasePath();
    }

    bool IsDatabaseInitialized() {
        return g_databaseInitialized;
    }

    std::string FilenameToTableName(const std::string& filename) {
        std::filesystem::path path(filename);
        return path.stem().string();  // Remove extension
    }

    std::string TableNameToFilename(const std::string& tableName, const std::string& extension) {
        return tableName + extension;
    }

} // namespace ConfigDatabaseUtils