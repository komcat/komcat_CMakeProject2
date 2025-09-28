// AppSettings.h - Updated with category support
#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <string>
#include <memory>
#include <optional>
#include <vector>

// Forward declaration
struct sqlite3;
struct sqlite3_stmt;

class AppSettings {
public:
  // Singleton instance
  static AppSettings& getInstance();

  // Delete copy constructor and assignment operator
  AppSettings(const AppSettings&) = delete;
  AppSettings& operator=(const AppSettings&) = delete;

  // Variable types enum
  enum class VarType {
    STRING,
    INT,
    FLOAT,
    BOOL
  };

  // ======= Category-based Methods =======

  // Create with category
  bool createVariable(const std::string& category, const std::string& varName,
    VarType type, const std::string& initialValue = "");
  bool createString(const std::string& category, const std::string& varName,
    const std::string& initialValue = "");
  bool createInt(const std::string& category, const std::string& varName,
    int initialValue = 0);
  bool createFloat(const std::string& category, const std::string& varName,
    float initialValue = 0.0f);
  bool createBool(const std::string& category, const std::string& varName,
    bool initialValue = false);

  // Get with category
  std::optional<std::string> getString(const std::string& category, const std::string& varName);
  std::optional<int> getInt(const std::string& category, const std::string& varName);
  std::optional<float> getFloat(const std::string& category, const std::string& varName);
  std::optional<bool> getBool(const std::string& category, const std::string& varName);

  // Set with category
  bool setString(const std::string& category, const std::string& varName, const std::string& value);
  bool setInt(const std::string& category, const std::string& varName, int value);
  bool setFloat(const std::string& category, const std::string& varName, float value);
  bool setBool(const std::string& category, const std::string& varName, bool value);

  // ======= Legacy Methods (backward compatibility) =======

  // These use "default" as category
  bool createString(const std::string& varName, const std::string& initialValue = "");
  bool createInt(const std::string& varName, int initialValue = 0);
  bool createFloat(const std::string& varName, float initialValue = 0.0f);
  bool createBool(const std::string& varName, bool initialValue = false);

  std::optional<std::string> getString(const std::string& varName);
  std::optional<int> getInt(const std::string& varName);
  std::optional<float> getFloat(const std::string& varName);
  std::optional<bool> getBool(const std::string& varName);

  bool setString(const std::string& varName, const std::string& value);
  bool setInt(const std::string& varName, int value);
  bool setFloat(const std::string& varName, float value);
  bool setBool(const std::string& varName, bool value);

  // ======= Utility Methods =======

  // Check if variable exists in category
  bool exists(const std::string& category, const std::string& varName);
  bool exists(const std::string& varName); // Uses "default" category

  // Delete variable
  bool deleteVariable(const std::string& category, const std::string& varName);
  bool deleteVariable(const std::string& varName);

  // Delete entire category
  bool deleteCategory(const std::string& category);

  // List all variables in a category
  std::vector<std::string> listVariables(const std::string& category);

  // List all categories
  std::vector<std::string> listCategories();

  // Get all settings in a category as pairs
  std::vector<std::pair<std::string, std::string>> getCategorySettings(const std::string& category);

  // Export/Import category
  bool exportCategory(const std::string& category, const std::string& filepath);
  bool importCategory(const std::string& category, const std::string& filepath);

  // Copy category
  bool copyCategory(const std::string& sourceCategory, const std::string& destCategory);

private:
  AppSettings();
  ~AppSettings();

  sqlite3* db;
  std::string dbPath;

  // Initialize database and create table if needed
  bool initDatabase();

  // Helper methods
  std::string varTypeToString(VarType type) const;
  VarType stringToVarType(const std::string& typeStr) const;
  bool checkTypeMatch(const std::string& category, const std::string& varName, VarType expectedType);
  std::optional<std::string> getRawValue(const std::string& category, const std::string& varName);

  // Execute simple SQL statements
  bool executeSQL(const std::string& sql);
};

// ======= Config Helper Class Using Categories =======

class CategoryConfig {
protected:
  AppSettings& settings;
  std::string category;

public:
  CategoryConfig(const std::string& categoryName)
    : settings(AppSettings::getInstance()), category(categoryName) {
  }

  // Initialize with defaults
  void initDefault(const std::string& varName, const std::string& defaultValue) {
    if (!settings.exists(category, varName)) {
      settings.createString(category, varName, defaultValue);
    }
  }

  void initDefault(const std::string& varName, int defaultValue) {
    if (!settings.exists(category, varName)) {
      settings.createInt(category, varName, defaultValue);
    }
  }

  void initDefault(const std::string& varName, float defaultValue) {
    if (!settings.exists(category, varName)) {
      settings.createFloat(category, varName, defaultValue);
    }
  }

  void initDefault(const std::string& varName, bool defaultValue) {
    if (!settings.exists(category, varName)) {
      settings.createBool(category, varName, defaultValue);
    }
  }

  // Getters with fallback
  std::string getString(const std::string& varName, const std::string& fallback = "") {
    auto val = settings.getString(category, varName);
    return val ? *val : fallback;
  }

  int getInt(const std::string& varName, int fallback = 0) {
    auto val = settings.getInt(category, varName);
    return val ? *val : fallback;
  }

  float getFloat(const std::string& varName, float fallback = 0.0f) {
    auto val = settings.getFloat(category, varName);
    return val ? *val : fallback;
  }

  bool getBool(const std::string& varName, bool fallback = false) {
    auto val = settings.getBool(category, varName);
    return val ? *val : fallback;
  }

  // Setters
  void setString(const std::string& varName, const std::string& value) {
    settings.setString(category, varName, value);
  }

  void setInt(const std::string& varName, int value) {
    settings.setInt(category, varName, value);
  }

  void setFloat(const std::string& varName, float value) {
    settings.setFloat(category, varName, value);
  }

  void setBool(const std::string& varName, bool value) {
    settings.setBool(category, varName, value);
  }

  // Utility
  std::string getCategory() const { return category; }

  void deleteAll() {
    settings.deleteCategory(category);
  }

  std::vector<std::string> listAll() {
    return settings.listVariables(category);
  }
};

#endif // APPSETTINGS_H