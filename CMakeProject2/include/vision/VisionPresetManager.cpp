#include "VisionPresetManager.h"
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

VisionPresetManager::VisionPresetManager() {
  // Constructor - database will be initialized when Initialize() is called
}

VisionPresetManager::~VisionPresetManager() {
  if (m_db) {
    sqlite3_close(static_cast<sqlite3*>(m_db));
    m_db = nullptr;
  }
}

bool VisionPresetManager::Initialize(const std::string& dbPath) {
  if (m_initialized) {
    return true;
  }

  m_dbPath = dbPath;

  // Open database
  int rc = sqlite3_open(m_dbPath.c_str(), reinterpret_cast<sqlite3**>(&m_db));
  if (rc != SQLITE_OK) {
    SetError("Cannot open database: " + std::string(sqlite3_errmsg(static_cast<sqlite3*>(m_db))));
    return false;
  }

  // Create tables if they don't exist
  if (!CreateTables()) {
    return false;
  }

  // Create default presets if database is empty
  auto presets = GetAllPresets();
  if (presets.empty()) {
    if (!CreateDefaultPresets()) {
      std::cerr << "Warning: Failed to create default presets" << std::endl;
    }
  }

  m_initialized = true;
  std::cout << "[VisionPresetManager] Initialized with database: " << m_dbPath << std::endl;
  return true;
}

bool VisionPresetManager::CreateTables() {
  const std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS vision_presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            parameters TEXT NOT NULL,
            description TEXT DEFAULT '',
            created_at TEXT NOT NULL,
            last_modified TEXT NOT NULL,
            is_default INTEGER DEFAULT 0
        );
        
        CREATE INDEX IF NOT EXISTS idx_name ON vision_presets(name);
        CREATE INDEX IF NOT EXISTS idx_is_default ON vision_presets(is_default);
    )";

  return ExecuteSQL(createTableSQL);
}

bool VisionPresetManager::ExecuteSQL(const std::string& sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(static_cast<sqlite3*>(m_db), sql.c_str(), nullptr, nullptr, &errMsg);

  if (rc != SQLITE_OK) {
    std::string error = errMsg ? errMsg : "Unknown SQL error";
    SetError("SQL Error: " + error);
    if (errMsg) sqlite3_free(errMsg);
    return false;
  }

  return true;
}

std::string VisionPresetManager::GetCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

void VisionPresetManager::SetError(const std::string& error) {
  m_lastError = error;
  std::cerr << "[VisionPresetManager] Error: " << error << std::endl;
}

int VisionPresetManager::SavePreset(const std::string& name, const nlohmann::json& parameters,
  const std::string& description) {
  if (!m_initialized) {
    SetError("Preset manager not initialized");
    return -1;
  }

  if (name.empty()) {
    SetError("Preset name cannot be empty");
    return -1;
  }

  std::string timestamp = GetCurrentTimestamp();
  std::string paramJson = parameters.dump();

  const std::string sql = R"(
        INSERT INTO vision_presets (name, parameters, description, created_at, last_modified, is_default)
        VALUES (?, ?, ?, ?, ?, 0)
    )";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    SetError("Failed to prepare statement: " + std::string(sqlite3_errmsg(static_cast<sqlite3*>(m_db))));
    return -1;
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, paramJson.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, timestamp.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  int presetId = -1;

  if (rc == SQLITE_DONE) {
    presetId = static_cast<int>(sqlite3_last_insert_rowid(static_cast<sqlite3*>(m_db)));
    std::cout << "[VisionPresetManager] Saved preset '" << name << "' with ID: " << presetId << std::endl;
  }
  else {
    SetError("Failed to insert preset: " + std::string(sqlite3_errmsg(static_cast<sqlite3*>(m_db))));
  }

  sqlite3_finalize(stmt);
  return presetId;
}

bool VisionPresetManager::LoadPreset(int presetId, nlohmann::json& parameters) {
  if (!m_initialized) {
    SetError("Preset manager not initialized");
    return false;
  }

  const std::string sql = "SELECT parameters FROM vision_presets WHERE id = ?";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    SetError("Failed to prepare statement");
    return false;
  }

  sqlite3_bind_int(stmt, 1, presetId);
  rc = sqlite3_step(stmt);

  bool success = false;
  if (rc == SQLITE_ROW) {
    const char* paramText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    try {
      parameters = nlohmann::json::parse(paramText);
      success = true;
      std::cout << "[VisionPresetManager] Loaded preset ID: " << presetId << std::endl;
    }
    catch (const std::exception& e) {
      SetError("Failed to parse JSON parameters: " + std::string(e.what()));
    }
  }
  else {
    SetError("Preset with ID " + std::to_string(presetId) + " not found");
  }

  sqlite3_finalize(stmt);
  return success;
}

std::vector<VisionPresetManager::PresetInfo> VisionPresetManager::GetAllPresets() {
  std::vector<PresetInfo> presets;

  if (!m_initialized) {
    SetError("Preset manager not initialized");
    return presets;
  }

  const std::string sql = "SELECT id, name, description, is_default, last_modified FROM vision_presets ORDER BY is_default DESC, name ASC";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    SetError("Failed to prepare statement");
    return presets;
  }

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    PresetInfo info;
    info.id = sqlite3_column_int(stmt, 0);
    info.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    info.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    info.isDefault = sqlite3_column_int(stmt, 3) != 0;
    info.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    presets.push_back(info);
  }

  sqlite3_finalize(stmt);
  return presets;
}

bool VisionPresetManager::DeletePreset(int presetId) {
  if (!m_initialized) {
    SetError("Preset manager not initialized");
    return false;
  }

  const std::string sql = "DELETE FROM vision_presets WHERE id = ? AND is_default = 0";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    SetError("Failed to prepare statement");
    return false;
  }

  sqlite3_bind_int(stmt, 1, presetId);
  rc = sqlite3_step(stmt);

  bool success = (rc == SQLITE_DONE && sqlite3_changes(static_cast<sqlite3*>(m_db)) > 0);

  if (success) {
    std::cout << "[VisionPresetManager] Deleted preset ID: " << presetId << std::endl;
  }
  else {
    SetError("Failed to delete preset (may be default preset or not found)");
  }

  sqlite3_finalize(stmt);
  return success;
}

bool VisionPresetManager::CreateDefaultPresets() {
  auto defaultData = GetDefaultPresetData();

  for (const auto& [name, params] : defaultData) {
    int id = SavePreset(name, params, "Default preset for " + name);
    if (id > 0) {
      SetAsDefault(id, true);
    }
  }

  std::cout << "[VisionPresetManager] Created " << defaultData.size() << " default presets" << std::endl;
  return true;
}

bool VisionPresetManager::SetAsDefault(int presetId, bool isDefault) {
  if (!m_initialized) {
    SetError("Preset manager not initialized");
    return false;
  }

  const std::string sql = "UPDATE vision_presets SET is_default = ? WHERE id = ?";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    SetError("Failed to prepare statement");
    return false;
  }

  sqlite3_bind_int(stmt, 1, isDefault ? 1 : 0);
  sqlite3_bind_int(stmt, 2, presetId);

  rc = sqlite3_step(stmt);
  bool success = (rc == SQLITE_DONE);

  sqlite3_finalize(stmt);
  return success;
}

std::vector<std::pair<std::string, nlohmann::json>> VisionPresetManager::GetDefaultPresetData() {
  std::vector<std::pair<std::string, nlohmann::json>> defaults;

  // Small Circle Preset
  nlohmann::json smallCircle = {
      {"roi", {{"size", 200}, {"offsetX", 0}, {"offsetY", 0}}},
      {"threshold", {{"low", 180}, {"high", 255}, {"invertImage", true}}},
      {"filter", {
          {"minArea", 300}, {"maxArea", 2000},
          {"minCircularity", 0.7}, {"maxCircularity", 1.0},
          {"minRadius", 10.0}, {"maxRadius", 30.0}, {"targetRadius", 20.0}
      }},
      {"advanced", {
          {"useCompactnessFilter", false}, {"minCompactness", 1.0}, {"maxCompactness", 10.0},
          {"useNoiseReduction", true}, {"medianKernelSize", 3},
          {"enableFallback", true}, {"fallbackMinRadius", 8.0}, {"fallbackMaxRadius", 35.0}
      }},
      {"metadata", {{"version", "1.0"}, {"description", "Small circle detection parameters"}}}
  };

  // Medium Circle Preset
  nlohmann::json mediumCircle = {
      {"roi", {{"size", 300}, {"offsetX", 0}, {"offsetY", 0}}},
      {"threshold", {{"low", 170}, {"high", 255}, {"invertImage", true}}},
      {"filter", {
          {"minArea", 1000}, {"maxArea", 8000},
          {"minCircularity", 0.75}, {"maxCircularity", 1.0},
          {"minRadius", 40.0}, {"maxRadius", 80.0}, {"targetRadius", 60.0}
      }},
      {"advanced", {
          {"useCompactnessFilter", false}, {"minCompactness", 1.0}, {"maxCompactness", 10.0},
          {"useNoiseReduction", false}, {"medianKernelSize", 3},
          {"enableFallback", true}, {"fallbackMinRadius", 30.0}, {"fallbackMaxRadius", 90.0}
      }},
      {"metadata", {{"version", "1.0"}, {"description", "Medium circle detection parameters"}}}
  };

  // Large Circle Preset
  nlohmann::json largeCircle = {
      {"roi", {{"size", 400}, {"offsetX", 0}, {"offsetY", 0}}},
      {"threshold", {{"low", 160}, {"high", 255}, {"invertImage", true}}},
      {"filter", {
          {"minArea", 5000}, {"maxArea", 50000},
          {"minCircularity", 0.8}, {"maxCircularity", 1.0},
          {"minRadius", 80.0}, {"maxRadius", 150.0}, {"targetRadius", 115.0}
      }},
      {"advanced", {
          {"useCompactnessFilter", true}, {"minCompactness", 1.0}, {"maxCompactness", 5.0},
          {"useNoiseReduction", true}, {"medianKernelSize", 5},
          {"enableFallback", true}, {"fallbackMinRadius", 70.0}, {"fallbackMaxRadius", 160.0}
      }},
      {"metadata", {{"version", "1.0"}, {"description", "Large circle detection parameters"}}}
  };

  // High Precision Preset
  nlohmann::json highPrecision = {
      {"roi", {{"size", 250}, {"offsetX", 0}, {"offsetY", 0}}},
      {"threshold", {{"low", 190}, {"high", 255}, {"invertImage", true}}},
      {"filter", {
          {"minArea", 800}, {"maxArea", 5000},
          {"minCircularity", 0.85}, {"maxCircularity", 1.0},
          {"minRadius", 30.0}, {"maxRadius", 70.0}, {"targetRadius", 50.0}
      }},
      {"advanced", {
          {"useCompactnessFilter", true}, {"minCompactness", 1.0}, {"maxCompactness", 3.0},
          {"useNoiseReduction", true}, {"medianKernelSize", 3},
          {"enableFallback", false}, {"fallbackMinRadius", 25.0}, {"fallbackMaxRadius", 75.0}
      }},
      {"metadata", {{"version", "1.0"}, {"description", "High precision circle detection"}}}
  };

  defaults.push_back({ "Small Circle", smallCircle });
  defaults.push_back({ "Medium Circle", mediumCircle });
  defaults.push_back({ "Large Circle", largeCircle });
  defaults.push_back({ "High Precision", highPrecision });

  return defaults;
}

int VisionPresetManager::GetNextAvailableId() {
  if (!m_initialized) return -1;

  const std::string sql = "SELECT MAX(id) FROM vision_presets";
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) return -1;

  int maxId = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    maxId = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return maxId + 1;
}