#pragma once

#include "UAA3DatabaseManager.h"
#include "include/logger.h"
#include <string>
#include <vector>
#include <filesystem>

/**
 * @brief Utility functions for managing configuration files with database integration
 *
 * This utility provides seamless integration between JSON configuration files
 * and the UAA3DatabaseManager, allowing configurations to be stored in database
 * while maintaining backward compatibility with existing JSON-based systems.
 */
namespace ConfigDatabaseUtils {

    /**
     * @brief Configuration loading result
     */
    struct ConfigLoadResult {
        bool success = false;
        bool loadedFromDatabase = false;
        bool savedToDatabase = false;
        std::string source;  // "database", "file", or "none"
        std::string message;

        ConfigLoadResult() = default;
        ConfigLoadResult(bool s, bool fromDb = false, bool toDb = false,
            const std::string& src = "", const std::string& msg = "")
            : success(s), loadedFromDatabase(fromDb), savedToDatabase(toDb), source(src), message(msg) {
        }
    };

    /**
     * @brief Initialize the database system
     * @param databasePath Optional custom database path
     * @param logger Logger instance for reporting
     * @return true if database is ready, false if initialization failed
     */
    bool InitializeDatabase(const std::string& databasePath = "", Logger* logger = nullptr);

    /**
     * @brief Load configuration with database integration
     * @param configFileName Name of the JSON config file (e.g., "DataServerConfig.json")
     * @param logger Logger instance for reporting (optional)
     * @return ConfigLoadResult with detailed information about the loading process
     */
    ConfigLoadResult LoadConfigWithDatabase(const std::string& configFileName, Logger* logger = nullptr);

    /**
     * @brief Save current configuration file to database
     * @param configFileName Name of the JSON config file
     * @param overwrite Whether to overwrite existing table (default: true)
     * @param logger Logger instance (optional)
     * @return true if successfully saved to database
     */
    bool SaveConfigToDatabase(const std::string& configFileName, bool overwrite = true, Logger* logger = nullptr);

    /**
     * @brief Load multiple configuration files with database integration
     * @param configFileNames Vector of config file names to load
     * @param logger Logger instance (optional)
     * @return Vector of ConfigLoadResult for each file
     */
    std::vector<ConfigLoadResult> LoadMultipleConfigs(const std::vector<std::string>& configFileNames, Logger* logger = nullptr);

    /**
     * @brief Export configuration from database back to JSON file
     * @param configFileName Name of the config file to export
     * @param outputDirectory Directory to save the file (default: current directory)
     * @param logger Logger instance (optional)
     * @return true if successfully exported
     */
    bool ExportConfigFromDatabase(const std::string& configFileName, const std::string& outputDirectory = ".", Logger* logger = nullptr);

    /**
     * @brief Check if a configuration exists in database
     * @param configFileName Name of the config file to check
     * @return true if table exists in database
     */
    bool ConfigExistsInDatabase(const std::string& configFileName);

    /**
     * @brief Get configuration information from database
     * @param configFileName Name of the config file
     * @return ConfigTableInfo with metadata about the configuration
     */
    UAA3::ConfigTableInfo GetConfigInfo(const std::string& configFileName);

    /**
     * @brief Delete configuration from database
     * @param configFileName Name of the config file to delete
     * @param logger Logger instance (optional)
     * @return true if successfully deleted
     */
    bool DeleteConfigFromDatabase(const std::string& configFileName, Logger* logger = nullptr);

    /**
     * @brief Create backup of all configurations
     * @param backupPath Optional custom backup path
     * @param logger Logger instance (optional)
     * @return true if backup created successfully
     */
    bool CreateConfigBackup(const std::string& backupPath = "", Logger* logger = nullptr);

    /**
     * @brief Validate database integrity
     * @param logger Logger instance (optional)
     * @return true if database is valid and consistent
     */
    bool ValidateConfigDatabase(Logger* logger = nullptr);

    /**
     * @brief Get database statistics and log them
     * @param logger Logger instance (optional)
     */
    void LogDatabaseStats(Logger* logger = nullptr);

    /**
     * @brief Transfer all common UAA3 config files to database
     * @param configDirectory Directory containing config files (default: current directory)
     * @param overwriteExisting Whether to overwrite existing tables
     * @param logger Logger instance (optional)
     * @return Number of files successfully transferred
     */
    int TransferAllUAA3Configs(const std::string& configDirectory = "", bool overwriteExisting = false, Logger* logger = nullptr);

    /**
     * @brief Export all configurations from database to files
     * @param outputDirectory Directory to save files (default: current directory)
     * @param logger Logger instance (optional)
     * @return Number of files successfully exported
     */
    int ExportAllConfigs(const std::string& outputDirectory = "", Logger* logger = nullptr);

    /**
     * @brief Get list of all configuration tables in database
     * @return Vector of table names (without file extensions)
     */
    std::vector<std::string> GetAllConfigNames();

    /**
     * @brief Get the database file path
     * @return Full path to the database file, or empty string if not initialized
     */
    std::string GetDatabasePath();

    /**
     * @brief Check if database system is initialized
     * @return true if database is ready for operations
     */
    bool IsDatabaseInitialized();

    /**
     * @brief Utility function to convert filename to table name
     * @param filename The original filename (e.g., "DataServerConfig.json")
     * @return Sanitized table name (e.g., "DataServerConfig")
     */
    std::string FilenameToTableName(const std::string& filename);

    /**
     * @brief Utility function to convert table name back to filename
     * @param tableName The table name
     * @param extension File extension to add (default: ".json")
     * @return Filename (e.g., "DataServerConfig.json")
     */
    std::string TableNameToFilename(const std::string& tableName, const std::string& extension = ".json");

} // namespace ConfigDatabaseUtils