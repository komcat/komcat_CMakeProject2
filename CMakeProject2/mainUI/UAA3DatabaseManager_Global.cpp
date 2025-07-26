#include "UAA3DatabaseManager.h"
#include <memory>

namespace UAA3 {

    // Static global instance for singleton pattern
    static std::unique_ptr<UAA3DatabaseManager> g_databaseManager = nullptr;

    // Global convenience functions implementation

    UAA3DatabaseManager& GetDatabaseManager() {
        if (!g_databaseManager) {
            g_databaseManager = std::make_unique<UAA3DatabaseManager>();
        }
        return *g_databaseManager;
    }

    DatabaseResult InitializeGlobalDatabase(const std::string& databasePath) {
        return GetDatabaseManager().InitializeDatabase(databasePath);
    }

    DatabaseResult QuickTransferConfig(const std::string& jsonFilePath, bool overwrite) {
        return GetDatabaseManager().TransferJsonToDatabase(jsonFilePath, overwrite);
    }

    DatabaseResult QuickExportConfig(const std::string& tableName, const std::string& outputPath) {
        return GetDatabaseManager().ExportTableToJson(tableName, outputPath);
    }

} // namespace UAA3