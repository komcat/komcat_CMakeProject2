// AppSettings.cpp - Implementation with category support
#include "AppSettings.h"
#include "sqlite3.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>

AppSettings& AppSettings::getInstance() {
  static AppSettings instance;
  return instance;
}

AppSettings::AppSettings() : db(nullptr) {
  dbPath = "db/appsettings.db";
  initDatabase();
}

AppSettings::~AppSettings() {
  if (db) {
    sqlite3_close(db);
    db = nullptr;
  }
}

bool AppSettings::initDatabase() {
  // Create directory if it doesn't exist
  try {
    std::filesystem::create_directories("db");
  }
  catch (const std::exception& e) {
    std::cerr << "Failed to create db directory: " << e.what() << std::endl;
    return false;
  }

  // Open database
  int rc = sqlite3_open(dbPath.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::cerr << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  // Create table with category column
  const char* createTable = R"(
        CREATE TABLE IF NOT EXISTS app_settings (
            category TEXT NOT NULL DEFAULT 'default',
            var_name TEXT NOT NULL,
            var_type TEXT NOT NULL,
            var_value TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (category, var_name)
        );
        
        CREATE INDEX IF NOT EXISTS idx_category ON app_settings(category);
        
        CREATE TRIGGER IF NOT EXISTS update_timestamp 
        AFTER UPDATE ON app_settings
        FOR EACH ROW
        BEGIN
            UPDATE app_settings SET updated_at = CURRENT_TIMESTAMP 
            WHERE category = NEW.category AND var_name = NEW.var_name;
        END;
    )";

  return executeSQL(createTable);
}

bool AppSettings::executeSQL(const std::string& sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << (errMsg ? errMsg : "unknown") << std::endl;
    if (errMsg) sqlite3_free(errMsg);
    return false;
  }

  return true;
}

std::string AppSettings::varTypeToString(VarType type) const {
  switch (type) {
  case VarType::STRING: return "string";
  case VarType::INT: return "int";
  case VarType::FLOAT: return "float";
  case VarType::BOOL: return "bool";
  default: return "string";
  }
}

AppSettings::VarType AppSettings::stringToVarType(const std::string& typeStr) const {
  if (typeStr == "string") return VarType::STRING;
  if (typeStr == "int") return VarType::INT;
  if (typeStr == "float") return VarType::FLOAT;
  if (typeStr == "bool") return VarType::BOOL;
  return VarType::STRING;
}

// ======= Category-based Methods =======

bool AppSettings::createVariable(const std::string& category, const std::string& varName,
  VarType type, const std::string& initialValue) {
  if (!db) {
    std::cerr << "createVariable: Database not initialized" << std::endl;
    return false;
  }

  // Check if already exists
  if (exists(category, varName)) {
    std::cerr << "createVariable: Variable already exists: " << category << "." << varName << std::endl;
    return false;
  }

  std::string typeString = varTypeToString(type);
  std::cerr << "createVariable: Creating " << category << "." << varName
    << " as type " << typeString << " with value " << initialValue << std::endl;

  const char* sql = "INSERT INTO app_settings (category, var_name, var_type, var_value) VALUES (?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "createVariable: Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, varName.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, typeString.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, initialValue.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  bool success = (rc == SQLITE_DONE);
  std::cerr << "createVariable: Result = " << (success ? "SUCCESS" : "FAILED") << std::endl;

  return success;
}


bool AppSettings::createString(const std::string& category, const std::string& varName, const std::string& initialValue) {
  return createVariable(category, varName, VarType::STRING, initialValue);
}

bool AppSettings::createInt(const std::string& category, const std::string& varName, int initialValue) {
  return createVariable(category, varName, VarType::INT, std::to_string(initialValue));
}

bool AppSettings::createFloat(const std::string& category, const std::string& varName, float initialValue) {
  return createVariable(category, varName, VarType::FLOAT, std::to_string(initialValue));
}

bool AppSettings::createBool(const std::string& category, const std::string& varName, bool initialValue) {
  return createVariable(category, varName, VarType::BOOL, initialValue ? "1" : "0");
}

bool AppSettings::exists(const std::string& category, const std::string& varName) {
  if (!db) return false;

  const char* sql = "SELECT COUNT(*) FROM app_settings WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, varName.c_str(), -1, SQLITE_STATIC);

  bool result = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = (sqlite3_column_int(stmt, 0) > 0);
  }

  sqlite3_finalize(stmt);
  return result;
}

bool AppSettings::checkTypeMatch(const std::string& category, const std::string& varName, VarType expectedType) {
  if (!db) return false;

  const char* sql = "SELECT var_type FROM app_settings WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, varName.c_str(), -1, SQLITE_STATIC);

  bool match = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* typeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (typeStr) {
      match = (typeStr == varTypeToString(expectedType));
    }
  }

  sqlite3_finalize(stmt);
  return match;
}

std::optional<std::string> AppSettings::getRawValue(const std::string& category, const std::string& varName) {
  if (!db) return std::nullopt;

  const char* sql = "SELECT var_value FROM app_settings WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, varName.c_str(), -1, SQLITE_STATIC);

  std::optional<std::string> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (value) {
      result = std::string(value);
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

// Get methods with category
std::optional<std::string> AppSettings::getString(const std::string& category, const std::string& varName) {
  if (!checkTypeMatch(category, varName, VarType::STRING)) {
    return std::nullopt;
  }
  return getRawValue(category, varName);
}

std::optional<int> AppSettings::getInt(const std::string& category, const std::string& varName) {
  if (!checkTypeMatch(category, varName, VarType::INT)) {
    return std::nullopt;
  }

  auto value = getRawValue(category, varName);
  if (value) {
    try {
      return std::stoi(*value);
    }
    catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<float> AppSettings::getFloat(const std::string& category, const std::string& varName) {
  if (!checkTypeMatch(category, varName, VarType::FLOAT)) {
    return std::nullopt;
  }

  auto value = getRawValue(category, varName);
  if (value) {
    try {
      return std::stof(*value);
    }
    catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<bool> AppSettings::getBool(const std::string& category, const std::string& varName) {
  if (!checkTypeMatch(category, varName, VarType::BOOL)) {
    return std::nullopt;
  }

  auto value = getRawValue(category, varName);
  if (value) {
    return (*value == "1" || *value == "true" || *value == "True");
  }
  return std::nullopt;
}

// Set methods with category - FIXED VERSIONS
bool AppSettings::setString(const std::string& category, const std::string& varName, const std::string& value) {
  if (!db || !checkTypeMatch(category, varName, VarType::STRING)) {
    return false;
  }

  const char* sql = "UPDATE app_settings SET var_value = ? WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}

bool AppSettings::setInt(const std::string& category, const std::string& varName, int value) {
  if (!db || !checkTypeMatch(category, varName, VarType::INT)) {
    return false;
  }

  const char* sql = "UPDATE app_settings SET var_value = ? WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  std::string valueStr = std::to_string(value);
  sqlite3_bind_text(stmt, 1, valueStr.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}

bool AppSettings::setFloat(const std::string& category, const std::string& varName, float value) {
  if (!db || !checkTypeMatch(category, varName, VarType::FLOAT)) {
    return false;
  }

  const char* sql = "UPDATE app_settings SET var_value = ? WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  std::string valueStr = std::to_string(value);
  sqlite3_bind_text(stmt, 1, valueStr.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}

bool AppSettings::setBool(const std::string& category, const std::string& varName, bool value) {
  if (!db || !checkTypeMatch(category, varName, VarType::BOOL)) {
    return false;
  }

  const char* sql = "UPDATE app_settings SET var_value = ? WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  std::string valueStr = value ? "1" : "0";
  sqlite3_bind_text(stmt, 1, valueStr.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, varName.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}

// ======= Legacy Methods (use "default" category) =======

bool AppSettings::createString(const std::string& varName, const std::string& initialValue) {
  return createString("default", varName, initialValue);
}

bool AppSettings::createInt(const std::string& varName, int initialValue) {
  return createInt("default", varName, initialValue);
}

bool AppSettings::createFloat(const std::string& varName, float initialValue) {
  return createFloat("default", varName, initialValue);
}

bool AppSettings::createBool(const std::string& varName, bool initialValue) {
  return createBool("default", varName, initialValue);
}

std::optional<std::string> AppSettings::getString(const std::string& varName) {
  return getString("default", varName);
}

std::optional<int> AppSettings::getInt(const std::string& varName) {
  return getInt("default", varName);
}

std::optional<float> AppSettings::getFloat(const std::string& varName) {
  return getFloat("default", varName);
}

std::optional<bool> AppSettings::getBool(const std::string& varName) {
  return getBool("default", varName);
}

bool AppSettings::setString(const std::string& varName, const std::string& value) {
  return setString("default", varName, value);
}

bool AppSettings::setInt(const std::string& varName, int value) {
  return setInt("default", varName, value);
}

bool AppSettings::setFloat(const std::string& varName, float value) {
  return setFloat("default", varName, value);
}

bool AppSettings::setBool(const std::string& varName, bool value) {
  return setBool("default", varName, value);
}

bool AppSettings::exists(const std::string& varName) {
  return exists("default", varName);
}

// ======= Utility Methods =======

bool AppSettings::deleteVariable(const std::string& category, const std::string& varName) {
  if (!db) return false;

  const char* sql = "DELETE FROM app_settings WHERE category = ? AND var_name = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, varName.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}

bool AppSettings::deleteVariable(const std::string& varName) {
  return deleteVariable("default", varName);
}

bool AppSettings::deleteCategory(const std::string& category) {
  if (!db) return false;

  const char* sql = "DELETE FROM app_settings WHERE category = ?;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE;
}

std::vector<std::string> AppSettings::listVariables(const std::string& category) {
  std::vector<std::string> variables;
  if (!db) return variables;

  const char* sql = "SELECT var_name FROM app_settings WHERE category = ? ORDER BY var_name;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return variables;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (name) {
      variables.push_back(std::string(name));
    }
  }

  sqlite3_finalize(stmt);
  return variables;
}

std::vector<std::string> AppSettings::listCategories() {
  std::vector<std::string> categories;
  if (!db) return categories;

  const char* sql = "SELECT DISTINCT category FROM app_settings ORDER BY category;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return categories;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* cat = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (cat) {
      categories.push_back(std::string(cat));
    }
  }

  sqlite3_finalize(stmt);
  return categories;
}

std::vector<std::pair<std::string, std::string>> AppSettings::getCategorySettings(const std::string& category) {
  std::vector<std::pair<std::string, std::string>> settings;
  if (!db) return settings;

  const char* sql = "SELECT var_name, var_value FROM app_settings WHERE category = ? ORDER BY var_name;";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return settings;
  }

  sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (name && value) {
      settings.emplace_back(std::string(name), std::string(value));
    }
  }

  sqlite3_finalize(stmt);
  return settings;
}

bool AppSettings::copyCategory(const std::string& sourceCategory, const std::string& destCategory) {
  if (!db) return false;

  // Delete destination category first
  deleteCategory(destCategory);

  const char* sql = R"(
        INSERT INTO app_settings (category, var_name, var_type, var_value)
        SELECT ?, var_name, var_type, var_value
        FROM app_settings
        WHERE category = ?;
    )";

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, destCategory.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, sourceCategory.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE;
}