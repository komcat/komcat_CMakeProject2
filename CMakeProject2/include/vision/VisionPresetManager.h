#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @brief Manages vision circle detection parameter presets
 *
 * Handles database storage and retrieval of parameter presets with automatic
 * numbering and custom naming capabilities.
 */
class VisionPresetManager {
public:
  struct Preset {
    int id = -1;                    // Auto-generated preset ID
    std::string name = "Custom";    // User-defined name
    nlohmann::json parameters;      // Complete parameter set
    std::string description = "";   // Optional description
    std::string createdAt = "";     // Creation timestamp
    std::string lastModified = "";  // Last modification timestamp
    bool isDefault = false;         // Whether this is a default preset
  };

  struct PresetInfo {
    int id;
    std::string name;
    std::string description;
    bool isDefault;
    std::string lastModified;
  };

public:
  VisionPresetManager();
  ~VisionPresetManager();

  // Database initialization
  bool Initialize(const std::string& dbPath = "vision_presets.db");
  bool IsInitialized() const { return m_initialized; }

  // Preset management
  int SavePreset(const std::string& name, const nlohmann::json& parameters,
    const std::string& description = "");
  bool LoadPreset(int presetId, nlohmann::json& parameters);
  bool DeletePreset(int presetId);
  bool RenamePreset(int presetId, const std::string& newName);
  bool UpdatePresetDescription(int presetId, const std::string& description);

  // Preset queries
  std::vector<PresetInfo> GetAllPresets();
  std::vector<PresetInfo> GetDefaultPresets();
  std::vector<PresetInfo> GetCustomPresets();
  Preset GetPresetDetails(int presetId);
  bool PresetExists(int presetId);
  bool PresetNameExists(const std::string& name);

  // Default preset management
  bool CreateDefaultPresets();
  bool SetAsDefault(int presetId, bool isDefault = true);

  // Utility functions
  std::string GetLastError() const { return m_lastError; }
  void ClearError() { m_lastError.clear(); }
  int GetNextAvailableId();

  // Export/Import
  bool ExportPreset(int presetId, const std::string& filePath);
  bool ImportPreset(const std::string& filePath, const std::string& name = "");
  bool ExportAllPresets(const std::string& filePath);
  bool ImportPresetsFromFile(const std::string& filePath);

private:
  bool m_initialized = false;
  std::string m_dbPath;
  std::string m_lastError;
  void* m_db = nullptr;  // SQLite database handle

  // Internal database operations
  bool CreateTables();
  bool ExecuteSQL(const std::string& sql);
  std::string GetCurrentTimestamp();
  void SetError(const std::string& error);

  // Default preset data
  std::vector<std::pair<std::string, nlohmann::json>> GetDefaultPresetData();
};