#pragma once
#include "ICameraHardware.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Manages camera exposure settings for vision system from JSON configuration file
 *
 * This class handles loading, applying, and managing camera exposure settings
 * for different nodes in the vision system using the ICameraHardware interface.
 */
class VisionCameraExposureManager {
public:
  // Use the existing ExposureSettings from ICameraHardware
  using ExposureSettings = ICameraHardware::ExposureSettings;

  // Extended settings with metadata for management
  struct NodeExposureSettings : public ExposureSettings {
    std::string description;
    std::string nodeId;

    NodeExposureSettings() = default;
    NodeExposureSettings(const ExposureSettings& base) : ExposureSettings(base) {}
  };

public:
  VisionCameraExposureManager();
  ~VisionCameraExposureManager() = default;

  /**
   * @brief Initialize from JSON configuration file
   * @param configPath Path to camera_exposure_config.json
   * @return true if successful
   */
  bool Initialize(const std::string& configPath = "camera_exposure_config.json");

  /**
   * @brief Check if manager is initialized
   */
  bool IsInitialized() const { return m_initialized; }

  /**
   * @brief Apply camera settings for a specific node
   * @param camera Camera hardware interface
   * @param nodeId Node identifier (e.g., "SeeCaldot", "node_4083")
   * @return true if settings were applied successfully
   */
  bool ApplySettingsForNode(ICameraHardware& camera, const std::string& nodeId);

  /**
   * @brief Apply default camera settings
   * @param camera Camera hardware interface
   * @return true if settings were applied successfully
   */
  bool ApplyDefaultSettings(ICameraHardware& camera);

  /**
   * @brief Apply specific settings to camera
   * @param camera Camera hardware interface
   * @param settings Settings to apply
   * @return true if successful
   */
  bool ApplySettingsToCamera(ICameraHardware& camera, const ExposureSettings& settings);

  /**
   * @brief Read current settings from camera
   * @param camera Camera hardware interface
   * @param settings Output settings structure
   * @return true if successful
   */
  bool ReadCurrentCameraSettings(ICameraHardware& camera, NodeExposureSettings& settings);

  /**
   * @brief Get settings for a specific node
   * @param nodeId Node identifier
   * @param settings Output settings structure
   * @return true if node settings exist
   */
  bool GetNodeSettings(const std::string& nodeId, NodeExposureSettings& settings);

  /**
   * @brief Get all node IDs that have settings
   * @return Vector of node IDs
   */
  std::vector<std::string> GetConfiguredNodes() const;

  /**
   * @brief Get default settings
   * @return Default camera exposure settings
   */
  NodeExposureSettings GetDefaultSettings() const { return m_defaultSettings; }

  /**
   * @brief Save current configuration back to JSON file
   * @return true if successful
   */
  bool SaveConfiguration();

  /**
   * @brief Update settings for a node
   * @param nodeId Node identifier
   * @param settings New settings
   * @return true if successful
   */
  bool UpdateNodeSettings(const std::string& nodeId, const NodeExposureSettings& settings);

  /**
   * @brief Remove settings for a node
   * @param nodeId Node identifier
   * @return true if successful
   */
  bool RemoveNodeSettings(const std::string& nodeId);

  /**
   * @brief Reload configuration from file
   * @return true if successful
   */
  bool ReloadConfiguration();

  // Logging
  void SetLogger(Logger* logger) { m_logger = logger; }
  std::string GetLastError() const { return m_lastError; }
  void ClearError() { m_lastError.clear(); }

  // Test/Debug
  void PrintCurrentConfiguration() const;
  void TestCameraSettings(ICameraHardware& camera, const std::string& nodeId = "");

private:
  bool m_initialized = false;
  std::string m_configPath;
  nlohmann::json m_config;
  Logger* m_logger = nullptr;
  std::string m_lastError;

  // Cached settings for quick access
  NodeExposureSettings m_defaultSettings;
  std::map<std::string, NodeExposureSettings> m_nodeSettings;

  // Helper methods
  void SetError(const std::string& error);
  bool LoadConfiguration();
  bool ParseSettings(const nlohmann::json& json, NodeExposureSettings& settings);
  nlohmann::json SettingsToJson(const NodeExposureSettings& settings) const;
  bool ValidateSettings(const ExposureSettings& settings) const;
};