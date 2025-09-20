#include "UAA3DatabaseManager.h"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace UAA3 {

	// Get database statistics
	nlohmann::json UAA3DatabaseManager::GetDatabaseStats() {
		nlohmann::json stats;

		if (!m_isInitialized) {
			stats["error"] = "Database not initialized";
			return stats;
		}

		stats["database_path"] = m_databasePath;
		stats["database_name"] = m_databaseName;
		stats["initialized"] = m_isInitialized;

		// Get file size
		if (std::filesystem::exists(m_databasePath)) {
			stats["file_size_bytes"] = std::filesystem::file_size(m_databasePath);
		}

		// Get table count
		auto tables = GetAllConfigTables();
		stats["table_count"] = tables.size();

		// Get total record count
		int totalRecords = 0;
		for (const auto& table : tables) {
			totalRecords += static_cast<int>(table.recordCount);
		}
		stats["total_records"] = totalRecords;

		// Get table details
		nlohmann::json tableDetails = nlohmann::json::array();
		for (const auto& table : tables) {
			nlohmann::json tableInfo;
			tableInfo["name"] = table.tableName;
			tableInfo["original_filename"] = table.originalFilename;
			tableInfo["created"] = table.createdTimestamp;
			tableInfo["modified"] = table.lastModified;
			tableInfo["records"] = table.recordCount;
			tableDetails.push_back(tableInfo);
		}
		stats["tables"] = tableDetails;

		return stats;
	}

	// Transfer all UAA3 configs
	std::vector<DatabaseResult> UAA3DatabaseManager::TransferAllUAA3Configs(const std::string& configDirectory, bool overwriteExisting) {
		std::vector<DatabaseResult> results;

		std::string searchDir = configDirectory.empty() ? "." : configDirectory;

		// Common UAA3 config files
		std::vector<std::string> configFiles = {
				"motion_config.json",
				"IOConfig.json",
				"DataServerConfig.json",
				"camera_config.json",
				"smu_config.json",
				"default_config.json"
		};

		for (const std::string& configFile : configFiles) {
			std::filesystem::path fullPath = std::filesystem::path(searchDir) / configFile;

			if (std::filesystem::exists(fullPath)) {
				DatabaseResult result = TransferJsonToDatabase(fullPath.string(), overwriteExisting);
				result.details = "File: " + configFile + " - " + result.details;
				results.push_back(result);
			}
			else {
				results.push_back(DatabaseResult(false, "File not found: " + configFile, fullPath.string()));
			}
		}

		return results;
	}

	// Export all tables to files
	std::vector<DatabaseResult> UAA3DatabaseManager::ExportAllTablesToFiles(const std::string& outputDirectory) {
		std::vector<DatabaseResult> results;

		auto tables = GetAllConfigTables();

		for (const auto& table : tables) {
			DatabaseResult result = ExportTableToOriginalFile(table.tableName, outputDirectory);
			results.push_back(result);
		}

		return results;
	}

	// Create backup
	DatabaseResult UAA3DatabaseManager::CreateBackup(const std::string& backupPath) {
		if (!m_isInitialized) {
			return DatabaseResult(false, "Database not initialized");
		}

		std::string targetPath = backupPath;
		if (targetPath.empty()) {
			auto now = std::chrono::system_clock::now();
			auto time_t = std::chrono::system_clock::to_time_t(now);
			std::stringstream ss;
			std::tm timeinfo;
			localtime_s(&timeinfo, &time_t);
			ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
			targetPath = m_databaseName + "_backup_" + ss.str() + ".db";
		}

		try {
			std::filesystem::copy_file(m_databasePath, targetPath, std::filesystem::copy_options::overwrite_existing);
			return DatabaseResult(true, "", "Backup created: " + targetPath);
		}
		catch (const std::exception& e) {
			return DatabaseResult(false, "Failed to create backup: " + std::string(e.what()));
		}
	}

	// Validate database
	DatabaseResult UAA3DatabaseManager::ValidateDatabase() {
		if (!m_isInitialized) {
			return DatabaseResult(false, "Database not initialized");
		}

		std::ostringstream details;
		bool isValid = true;

		// Check database integrity
		const char* integritySql = "PRAGMA integrity_check;";
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(m_database, integritySql, -1, &stmt, nullptr);

		if (result == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				const char* integrityResult = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
				if (std::string(integrityResult) != "ok") {
					isValid = false;
					details << "Integrity check failed: " << integrityResult << "\n";
				}
				else {
					details << "Database integrity: OK\n";
				}
			}
			sqlite3_finalize(stmt);
		}

		// Check metadata table exists
		if (!TableExists("config_metadata")) {
			isValid = false;
			details << "Metadata table missing\n";
		}
		else {
			details << "Metadata table: OK\n";
		}

		// Check each config table
		auto tables = GetAllConfigTables();
		details << "Config tables: " << tables.size() << "\n";

		for (const auto& table : tables) {
			if (!TableExists(table.tableName)) {
				isValid = false;
				details << "Missing table: " << table.tableName << "\n";
			}
		}

		return DatabaseResult(isValid, isValid ? "" : "Database validation failed", details.str());
	}


	// Add these methods to UAA3DatabaseManager class (in UAA3DatabaseManager_Utils.cpp)

/**
 * @brief Check if table uses old complex format or new simple format
 */
	bool UAA3DatabaseManager::IsOldFormatTable(const std::string& tableName) {
		if (!m_database || !TableExists(tableName)) {
			return false;
		}

		// Check if table has the old complex structure (json_key, json_value, data_type columns)
		const char* sql = "PRAGMA table_info(?);";
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(m_database, sql, -1, &stmt, nullptr);
		if (result != SQLITE_OK) {
			return false;
		}

		sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);

		bool hasJsonKey = false;
		bool hasJsonValue = false;
		bool hasJsonContent = false;

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const char* columnName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			if (columnName) {
				std::string colName(columnName);
				if (colName == "json_key") hasJsonKey = true;
				if (colName == "json_value") hasJsonValue = true;
				if (colName == "json_content") hasJsonContent = true;
			}
		}

		sqlite3_finalize(stmt);

		// Old format has json_key and json_value, new format has json_content
		return (hasJsonKey && hasJsonValue && !hasJsonContent);
	}

	/**
	 * @brief Migrate old format table to new simple format
	 */
	DatabaseResult UAA3DatabaseManager::MigrateTableToNewFormat(const std::string& tableName) {
		if (!IsOldFormatTable(tableName)) {
			return DatabaseResult(true, "", "Table is already in new format or doesn't exist");
		}

		// Get the current JSON data using the old method
		nlohmann::json jsonData = GetTableAsJsonOldFormat(tableName);
		if (jsonData.empty()) {
			return DatabaseResult(false, "Failed to read existing data from old format table");
		}

		// Get metadata
		ConfigTableInfo info = GetTableInfo(tableName);

		// Drop the old table
		std::string dropSql = "DROP TABLE " + tableName + ";";
		char* errMsg = nullptr;
		int result = sqlite3_exec(m_database, dropSql.c_str(), nullptr, nullptr, &errMsg);
		if (result != SQLITE_OK) {
			std::string error = "Failed to drop old table: " + std::string(errMsg);
			sqlite3_free(errMsg);
			return DatabaseResult(false, error);
		}

		// Create new table with simple format
		std::string createSql = GenerateCreateTableSQL(jsonData, tableName);
		result = sqlite3_exec(m_database, createSql.c_str(), nullptr, nullptr, &errMsg);
		if (result != SQLITE_OK) {
			std::string error = "Failed to create new table: " + std::string(errMsg);
			sqlite3_free(errMsg);
			return DatabaseResult(false, error);
		}

		// Insert data in new format
		DatabaseResult insertResult = InsertJsonData(tableName, jsonData);
		if (!insertResult.success) {
			return insertResult;
		}

		// Restore metadata
		DatabaseResult metadataResult = UpdateMetadata(tableName, info.originalFilename);
		if (!metadataResult.success) {
			return metadataResult;
		}

		return DatabaseResult(true, "", "Successfully migrated table to new format");
	}

	/**
	 * @brief Old method to read JSON from complex format (for migration only)
	 */
	nlohmann::json UAA3DatabaseManager::GetTableAsJsonOldFormat(const std::string& tableName) {
		if (!m_isInitialized || !TableExists(tableName)) {
			return nlohmann::json{};
		}

		std::string sql = "SELECT json_key, json_value, data_type, parent_key, array_index FROM " + tableName + " ORDER BY id;";
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(m_database, sql.c_str(), -1, &stmt, nullptr);

		if (result != SQLITE_OK) {
			return nlohmann::json{};
		}

		nlohmann::json reconstructed;

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
			const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			const char* dataType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

			if (key && value && dataType) {
				// Convert value based on data type
				if (std::string(dataType) == "string") {
					reconstructed[key] = std::string(value);
				}
				else if (std::string(dataType) == "integer") {
					reconstructed[key] = std::stoll(value);
				}
				else if (std::string(dataType) == "float") {
					reconstructed[key] = std::stod(value);
				}
				else if (std::string(dataType) == "boolean") {
					reconstructed[key] = (std::string(value) == "true");
				}
				else if (std::string(dataType) == "null") {
					reconstructed[key] = nullptr;
				}
				else {
					// Try to parse as JSON
					try {
						reconstructed[key] = nlohmann::json::parse(value);
					}
					catch (...) {
						reconstructed[key] = std::string(value);
					}
				}
			}
		}

		sqlite3_finalize(stmt);
		return reconstructed;
	}

	/**
	 * @brief Migrate all tables to new format
	 */
	std::vector<DatabaseResult> UAA3DatabaseManager::MigrateAllTablesToNewFormat() {
		std::vector<DatabaseResult> results;

		auto tables = GetAllConfigTables();
		for (const auto& table : tables) {
			if (IsOldFormatTable(table.tableName)) {
				DatabaseResult result = MigrateTableToNewFormat(table.tableName);
				result.details = "Table: " + table.tableName + " - " + result.details;
				results.push_back(result);
			}
			else {
				results.push_back(DatabaseResult(true, "", "Table " + table.tableName + " already in new format"));
			}
		}

		return results;
	}

} // namespace UAA3