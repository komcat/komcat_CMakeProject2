#include "UAA3DatabaseManager.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <functional>

namespace UAA3 {


    // Generate CREATE TABLE SQL - SIMPLIFIED VERSION
    std::string UAA3DatabaseManager::GenerateCreateTableSQL(const nlohmann::json& jsonData, const std::string& tableName) {
        std::ostringstream sql;
        sql << "CREATE TABLE IF NOT EXISTS " << tableName << " (\n";
        sql << "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n";
        sql << "    json_content TEXT NOT NULL,\n";  // Store entire JSON as text
        sql << "    created_timestamp TEXT DEFAULT CURRENT_TIMESTAMP,\n";
        sql << "    modified_timestamp TEXT DEFAULT CURRENT_TIMESTAMP\n";
        sql << ");";

        return sql.str();
    }

    // Insert JSON data - SIMPLIFIED VERSION  
    DatabaseResult UAA3DatabaseManager::InsertJsonData(const std::string& tableName, const nlohmann::json& jsonData) {
        if (!m_database) {
            return DatabaseResult(false, "Database not initialized");
        }

        // Clear existing data
        std::string deleteSql = "DELETE FROM " + tableName + ";";
        char* errMsg = nullptr;
        int result = sqlite3_exec(m_database, deleteSql.c_str(), nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::string error = "Failed to clear table: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        // Convert JSON to formatted string
        std::string jsonString = jsonData.dump(2); // Pretty-printed with 2 spaces

        // Prepare insert statement
        std::string insertSql = "INSERT INTO " + tableName + " (json_content) VALUES (?);";
        sqlite3_stmt* stmt;
        result = sqlite3_prepare_v2(m_database, insertSql.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            return DatabaseResult(false, "Failed to prepare insert statement: " + std::string(sqlite3_errmsg(m_database)));
        }

        // Bind the JSON string
        sqlite3_bind_text(stmt, 1, jsonString.c_str(), -1, SQLITE_STATIC);

        // Execute the insert
        result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result != SQLITE_DONE) {
            return DatabaseResult(false, "Failed to insert JSON data: " + std::string(sqlite3_errmsg(m_database)));
        }

        return DatabaseResult(true, "", "Successfully stored JSON configuration");
    }



    // Update metadata - SIMPLIFIED VERSION
    DatabaseResult UAA3DatabaseManager::UpdateMetadata(const std::string& tableName, const std::string& filename) {
        if (!m_database) {
            return DatabaseResult(false, "Database not initialized");
        }

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        std::string timestamp = ss.str();

        // Count records (should be 1 with new approach)
        std::string countSql = "SELECT COUNT(*) FROM " + tableName + ";";
        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_database, countSql.c_str(), -1, &stmt, nullptr);
        int recordCount = 0;
        if (result == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                recordCount = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        // Insert or update metadata
        const char* sql = R"(
        INSERT OR REPLACE INTO config_metadata 
        (table_name, original_filename, created_timestamp, last_modified, record_count)
        VALUES (?, ?, 
                COALESCE((SELECT created_timestamp FROM config_metadata WHERE table_name = ?), ?),
                ?, ?);
    )";

        result = sqlite3_prepare_v2(m_database, sql, -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            return DatabaseResult(false, "Failed to prepare metadata update: " + std::string(sqlite3_errmsg(m_database)));
        }

        sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, tableName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, recordCount);

        result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result != SQLITE_DONE) {
            return DatabaseResult(false, "Failed to update metadata: " + std::string(sqlite3_errmsg(m_database)));
        }

        return DatabaseResult(true);
    }

    // Transfer JSON file to database
    DatabaseResult UAA3DatabaseManager::TransferJsonToDatabase(const std::string& jsonFilePath, bool overwriteExisting) {
        if (!m_isInitialized) {
            return DatabaseResult(false, "Database not initialized");
        }

        // Check if file exists
        if (!std::filesystem::exists(jsonFilePath)) {
            return DatabaseResult(false, "JSON file does not exist: " + jsonFilePath);
        }

        // Generate table name
        std::string tableName = SanitizeTableName(jsonFilePath);

        // Check if table already exists
        if (!overwriteExisting && TableExists(tableName)) {
            return DatabaseResult(false, "Table already exists: " + tableName, "Use overwriteExisting=true to replace");
        }

        // Read JSON file
        std::ifstream file(jsonFilePath);
        if (!file.is_open()) {
            return DatabaseResult(false, "Failed to open JSON file: " + jsonFilePath);
        }

        nlohmann::json jsonData;
        try {
            file >> jsonData;
        }
        catch (const std::exception& e) {
            return DatabaseResult(false, "Failed to parse JSON: " + std::string(e.what()));
        }

        // Create table
        std::string createTableSql = GenerateCreateTableSQL(jsonData, tableName);
        char* errMsg = nullptr;
        int result = sqlite3_exec(m_database, createTableSql.c_str(), nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::string error = "Failed to create table: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return DatabaseResult(false, error);
        }

        // Insert data
        DatabaseResult insertResult = InsertJsonData(tableName, jsonData);
        if (!insertResult.success) {
            return insertResult;
        }

        // Update metadata
        DatabaseResult metadataResult = UpdateMetadata(tableName, std::filesystem::path(jsonFilePath).filename().string());
        if (!metadataResult.success) {
            return metadataResult;
        }

        return DatabaseResult(true, "", "Successfully transferred " + jsonFilePath + " to table " + tableName);
    }


    // Get table as JSON - SIMPLIFIED VERSION
    nlohmann::json UAA3DatabaseManager::GetTableAsJson(const std::string& tableName) {
        if (!m_isInitialized || !TableExists(tableName)) {
            return nlohmann::json{};
        }

        std::string sql = "SELECT json_content FROM " + tableName + " ORDER BY id DESC LIMIT 1;"; // Get most recent
        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_database, sql.c_str(), -1, &stmt, nullptr);

        if (result != SQLITE_OK) {
            return nlohmann::json{};
        }

        nlohmann::json reconstructed;

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* jsonContent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

            if (jsonContent) {
                try {
                    reconstructed = nlohmann::json::parse(jsonContent);
                }
                catch (const std::exception& e) {
                    std::cerr << "Failed to parse JSON from database: " << e.what() << std::endl;
                    reconstructed = nlohmann::json{};
                }
            }
        }

        sqlite3_finalize(stmt);
        return reconstructed;
    }
    // Export table to JSON file
    DatabaseResult UAA3DatabaseManager::ExportTableToJson(const std::string& tableName, const std::string& outputJsonPath, bool includeMetadata) {
        if (!m_isInitialized) {
            return DatabaseResult(false, "Database not initialized");
        }

        if (!TableExists(tableName)) {
            return DatabaseResult(false, "Table does not exist: " + tableName);
        }

        nlohmann::json jsonData = GetTableAsJson(tableName);

        if (includeMetadata) {
            ConfigTableInfo info = GetTableInfo(tableName);
            jsonData["_metadata"] = {
                {"table_name", info.tableName},
                {"original_filename", info.originalFilename},
                {"created_timestamp", info.createdTimestamp},
                {"last_modified", info.lastModified},
                {"record_count", info.recordCount}
            };
        }

        // Create output directory if needed
        std::filesystem::path outputPath(outputJsonPath);
        std::filesystem::create_directories(outputPath.parent_path());

        // Write JSON file
        std::ofstream file(outputJsonPath);
        if (!file.is_open()) {
            return DatabaseResult(false, "Failed to create output file: " + outputJsonPath);
        }

        file << std::setw(2) << jsonData << std::endl;
        file.close();

        return DatabaseResult(true, "", "Exported table " + tableName + " to " + outputJsonPath);
    }

    // Export table to original file
    DatabaseResult UAA3DatabaseManager::ExportTableToOriginalFile(const std::string& tableName, const std::string& outputDirectory) {
        ConfigTableInfo info = GetTableInfo(tableName);
        if (!info.isValid) {
            return DatabaseResult(false, "Table not found or invalid: " + tableName);
        }

        std::string outputPath;
        if (outputDirectory.empty()) {
            outputPath = info.originalFilename;
        }
        else {
            outputPath = (std::filesystem::path(outputDirectory) / info.originalFilename).string();
        }

        return ExportTableToJson(tableName, outputPath, false);
    }

    // Update table data
    DatabaseResult UAA3DatabaseManager::UpdateTableData(const std::string& tableName, const nlohmann::json& jsonData) {
        if (!m_isInitialized) {
            return DatabaseResult(false, "Database not initialized");
        }

        if (!TableExists(tableName)) {
            return DatabaseResult(false, "Table does not exist: " + tableName);
        }

        // Insert new data (this will clear existing data first)
        DatabaseResult insertResult = InsertJsonData(tableName, jsonData);
        if (!insertResult.success) {
            return insertResult;
        }

        // Update metadata timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        std::string timestamp = ss.str();

        std::string updateSql = "UPDATE config_metadata SET last_modified = ?, record_count = ? WHERE table_name = ?;";
        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_database, updateSql.c_str(), -1, &stmt, nullptr);
        if (result == SQLITE_OK) {
            // Count records
            std::string countSql = "SELECT COUNT(*) FROM " + tableName + ";";
            sqlite3_stmt* countStmt;
            int recordCount = 0;
            if (sqlite3_prepare_v2(m_database, countSql.c_str(), -1, &countStmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(countStmt) == SQLITE_ROW) {
                    recordCount = sqlite3_column_int(countStmt, 0);
                }
                sqlite3_finalize(countStmt);
            }

            sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, recordCount);
            sqlite3_bind_text(stmt, 3, tableName.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        return DatabaseResult(true, "", "Successfully updated table: " + tableName);
    }

} // namespace UAA3