#include "CameraConfigManager.h"
#include "include/logger.h"
#include "include/camera/CameraManager.h"  // This will include the CameraInfo struct
#include "include/camera/pylon_camera_test.h"
#include <fstream>
#include <algorithm>

CameraConfigManager::CameraConfigManager(const std::string& configFilePath)
  : configFilePath_(configFilePath), logger_(nullptr), configLoaded_(false) {
}

bool CameraConfigManager::LoadConfig() {
  return LoadConfig(configFilePath_);
}

bool CameraConfigManager::LoadConfig(const std::string& configFilePath) {
  configFilePath_ = configFilePath;

  try {
    std::ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
      LogWarning("Camera config file not found: " + configFilePath + ", creating default configuration");
      CreateDefaultConfig();
      return SaveConfig(); // Save the default config
    }

    nlohmann::json jsonConfig;
    configFile >> jsonConfig;
    configFile.close();

    LogInfo("Loading camera configuration from: " + configFilePath);

    bool success = LoadFromJson(jsonConfig);
    if (success) {
      configLoaded_ = true;
      LogInfo("Camera configuration loaded successfully");

      // Validate the loaded configuration
      if (!ValidateConfig()) {
        LogWarning("Camera configuration has validation issues");
        auto errors = GetValidationErrors();
        for (const auto& error : errors) {
          LogWarning("Validation error: " + error);
        }
      }
    }

    return success;
  }
  catch (const std::exception& e) {
    LogError("Failed to load camera configuration: " + std::string(e.what()));
    return false;
  }
}

bool CameraConfigManager::SaveConfig() const {
  return SaveConfig(configFilePath_);
}

bool CameraConfigManager::SaveConfig(const std::string& configFilePath) const {
  try {
    nlohmann::json jsonConfig = SaveToJson();

    std::ofstream configFile(configFilePath);
    if (!configFile.is_open()) {
      LogError("Failed to open config file for writing: " + configFilePath);
      return false;
    }

    configFile << jsonConfig.dump(2); // Pretty print with 2-space indentation
    configFile.close();

    LogInfo("Camera configuration saved to: " + configFilePath);
    return true;
  }
  catch (const std::exception& e) {
    LogError("Failed to save camera configuration: " + std::string(e.what()));
    return false;
  }
}

bool CameraConfigManager::InitializeCameraManager(CameraManager& cameraManager) const {
  if (!configLoaded_) {
    LogError("Cannot initialize CameraManager: configuration not loaded");
    return false;
  }

  LogInfo("Initializing CameraManager with configuration");

  try {
    // Apply manager settings
    LogInfo("Applying camera manager settings:");
    LogInfo("  - Auto connect: " + std::string(managerSettings_.autoConnect ? "enabled" : "disabled"));
    LogInfo("  - Broadcasting: " + std::string(managerSettings_.broadcastEnabled ? "enabled" : "disabled"));
    LogInfo("  - Max subscribers: " + std::to_string(managerSettings_.maxSubscribers));
    LogInfo("  - Default exposure: " + std::to_string(managerSettings_.defaultExposureTime) + "μs");
    LogInfo("  - Default gain: " + std::to_string(managerSettings_.defaultGain));

    // Add cameras to manager
    int addedCameras = 0;
    int enabledCameras = 0;

    for (const auto& camConfig : cameraConfigs_) {
      if (!camConfig.enabled) {
        LogInfo("Skipping disabled camera: " + camConfig.id);
        continue;
      }

      enabledCameras++;

      try {
        // Convert our CameraConfigData to the existing CameraInfo struct that CameraManager expects
        CameraInfo camera = ConvertToCameraInfo(camConfig);

        LogInfo("Adding " + camConfig.connectionType + " camera: " + camConfig.id +
          " (" + (camConfig.ipAddress.empty() ? "auto-detect" : camConfig.ipAddress) + ") - " +
          camConfig.displayName);

        // Add camera to manager
        cameraManager.AddCamera(camera);
        addedCameras++;

        LogInfo("Successfully added camera: " + camConfig.id);

      }
      catch (const std::exception& e) {
        LogError("Failed to add camera " + camConfig.id + ": " + std::string(e.what()));
      }
    }

    LogInfo("Camera configuration summary:");
    LogInfo("  - Total cameras in config: " + std::to_string(cameraConfigs_.size()));
    LogInfo("  - Enabled cameras: " + std::to_string(enabledCameras));
    LogInfo("  - Successfully added: " + std::to_string(addedCameras));

    // Initialize all cameras if auto_connect is enabled
    if (managerSettings_.autoConnect && addedCameras > 0) {
      LogInfo("Auto-initializing cameras...");
      cameraManager.InitializeAllCameras();

      // Log connection results
      for (const auto& camConfig : cameraConfigs_) {
        if (camConfig.enabled) {
          auto status = cameraManager.GetCameraStatus(camConfig.id);
          if (status.connected) {
            LogInfo("✓ Camera " + camConfig.id + " connected successfully");
          }
          else {
            LogWarning("✗ Camera " + camConfig.id + " failed to connect");
          }
        }
      }
    }

    return addedCameras > 0;
  }
  catch (const std::exception& e) {
    LogError("Exception during CameraManager initialization: " + std::string(e.what()));
    return false;
  }
}

bool CameraConfigManager::GetCameraConfig(const std::string& cameraId, CameraConfigData& config) const {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](const CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    config = *it;
    return true;
  }
  return false;
}

bool CameraConfigManager::UpdateCameraConfig(const std::string& cameraId, const CameraConfigData& config) {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](const CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    *it = config;
    LogInfo("Updated camera configuration: " + cameraId);
    return true;
  }
  return false;
}

bool CameraConfigManager::AddCameraConfig(const CameraConfigData& config) {
  // Check if camera ID already exists
  if (std::any_of(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&config](const CameraConfigData& cfg) { return cfg.id == config.id; })) {
    LogWarning("Camera ID already exists: " + config.id);
    return false;
  }

  cameraConfigs_.push_back(config);
  LogInfo("Added camera configuration: " + config.id);
  return true;
}

bool CameraConfigManager::RemoveCameraConfig(const std::string& cameraId) {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](const CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    cameraConfigs_.erase(it);
    LogInfo("Removed camera configuration: " + cameraId);
    return true;
  }
  return false;
}

std::vector<std::string> CameraConfigManager::GetEnabledCameraIds() const {
  std::vector<std::string> enabledIds;
  for (const auto& config : cameraConfigs_) {
    if (config.enabled) {
      enabledIds.push_back(config.id);
    }
  }
  return enabledIds;
}

std::vector<std::string> CameraConfigManager::GetAutoConnectCameraIds() const {
  std::vector<std::string> autoConnectIds;
  for (const auto& config : cameraConfigs_) {
    if (config.enabled && config.autoConnect) {
      autoConnectIds.push_back(config.id);
    }
  }
  return autoConnectIds;
}

int CameraConfigManager::GetEnabledCameraCount() const {
  return static_cast<int>(std::count_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [](const CameraConfigData& cfg) { return cfg.enabled; }));
}

bool CameraConfigManager::IsCameraEnabled(const std::string& cameraId) const {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](const CameraConfigData& cfg) { return cfg.id == cameraId; });

  return it != cameraConfigs_.end() && it->enabled;
}

bool CameraConfigManager::ValidateConfig() const {
  validationErrors_.clear();

  // Validate manager settings
  ValidateManagerSettings(managerSettings_, validationErrors_);

  // Validate each camera config
  for (const auto& config : cameraConfigs_) {
    ValidateCameraConfig(config, validationErrors_);
  }

  // Check for duplicate camera IDs
  std::vector<std::string> ids;
  for (const auto& config : cameraConfigs_) {
    if (std::find(ids.begin(), ids.end(), config.id) != ids.end()) {
      validationErrors_.push_back("Duplicate camera ID: " + config.id);
    }
    ids.push_back(config.id);
  }

  return validationErrors_.empty();
}

std::vector<std::string> CameraConfigManager::GetValidationErrors() const {
  return validationErrors_;
}

void CameraConfigManager::CreateDefaultConfig() {
  LogInfo("Creating default camera configuration");

  // Set default manager settings
  managerSettings_ = CameraManagerSettings();

  // Clear existing configs
  cameraConfigs_.clear();

  // Add default cameras
  CameraConfigData mainCamera;
  mainCamera.id = "main_camera";
  mainCamera.displayName = "Top view camera";
  mainCamera.connectionType = "ip";
  mainCamera.ipAddress = "192.168.0.68";
  mainCamera.port = 0;
  mainCamera.enabled = true;
  mainCamera.autoConnect = true;
  mainCamera.description = "Primary overhead camera for top-down imaging";
  cameraConfigs_.push_back(mainCamera);

  CameraConfigData auxCamera;
  auxCamera.id = "aux_camera";
  auxCamera.displayName = "Auxiliary Camera";
  auxCamera.connectionType = "ip";
  auxCamera.ipAddress = "192.168.0.69";
  auxCamera.port = 0;
  auxCamera.enabled = true;
  auxCamera.autoConnect = true;
  auxCamera.description = "Secondary camera for auxiliary imaging";
  cameraConfigs_.push_back(auxCamera);

  configLoaded_ = true;
}

// Private helper methods
bool CameraConfigManager::LoadFromJson(const nlohmann::json& jsonConfig) {
  try {
    // Load manager settings
    if (jsonConfig.contains("manager_settings")) {
      managerSettings_ = ParseManagerSettings(jsonConfig["manager_settings"]);
    }

    // Load cameras
    cameraConfigs_.clear();
    if (jsonConfig.contains("cameras")) {
      for (const auto& camJson : jsonConfig["cameras"]) {
        CameraConfigData config = ParseCameraConfig(camJson);
        cameraConfigs_.push_back(config);
      }
    }

    return true;
  }
  catch (const std::exception& e) {
    LogError("Error parsing JSON configuration: " + std::string(e.what()));
    return false;
  }
}

nlohmann::json CameraConfigManager::SaveToJson() const {
  nlohmann::json jsonConfig;

  // Save manager settings
  jsonConfig["manager_settings"] = ManagerSettingsToJson(managerSettings_);

  // Save cameras
  jsonConfig["cameras"] = nlohmann::json::array();
  for (const auto& config : cameraConfigs_) {
    jsonConfig["cameras"].push_back(CameraConfigToJson(config));
  }

  return jsonConfig;
}

CameraConfigData CameraConfigManager::ParseCameraConfig(const nlohmann::json& camJson) const {
  CameraConfigData config;

  config.id = camJson.value("id", "");
  config.displayName = camJson.value("display_name", "Unknown Camera");
  config.connectionType = camJson.value("connection_type", "auto");
  config.ipAddress = camJson.value("ip_address", "");
  config.port = camJson.value("port", 0);
  config.enabled = camJson.value("enabled", true);
  config.autoConnect = camJson.value("auto_connect", true);
  config.description = camJson.value("description", "");

  // Optional camera settings
  config.exposureTime = camJson.value("exposure_time", managerSettings_.defaultExposureTime);
  config.gain = camJson.value("gain", managerSettings_.defaultGain);
  config.width = camJson.value("width", managerSettings_.defaultWidth);
  config.height = camJson.value("height", managerSettings_.defaultHeight);

  return config;
}

CameraManagerSettings CameraConfigManager::ParseManagerSettings(const nlohmann::json& settingsJson) const {
  CameraManagerSettings settings;

  settings.autoConnect = settingsJson.value("auto_connect", true);
  settings.defaultExposureTime = settingsJson.value("default_exposure_time", 1000);
  settings.defaultGain = settingsJson.value("default_gain", 1.0);
  settings.broadcastEnabled = settingsJson.value("broadcast_enabled", true);
  settings.maxSubscribers = settingsJson.value("max_subscribers", 10);
  settings.defaultWidth = settingsJson.value("default_width", 0);
  settings.defaultHeight = settingsJson.value("default_height", 0);
  settings.logLevel = settingsJson.value("log_level", "INFO");

  return settings;
}

nlohmann::json CameraConfigManager::CameraConfigToJson(const CameraConfigData& config) const {
  nlohmann::json camJson;

  camJson["id"] = config.id;
  camJson["display_name"] = config.displayName;
  camJson["connection_type"] = config.connectionType;
  camJson["ip_address"] = config.ipAddress;
  camJson["port"] = config.port;
  camJson["enabled"] = config.enabled;
  camJson["auto_connect"] = config.autoConnect;
  camJson["description"] = config.description;

  // Only save non-default camera settings
  if (config.exposureTime != managerSettings_.defaultExposureTime) {
    camJson["exposure_time"] = config.exposureTime;
  }
  if (config.gain != managerSettings_.defaultGain) {
    camJson["gain"] = config.gain;
  }
  if (config.width != managerSettings_.defaultWidth) {
    camJson["width"] = config.width;
  }
  if (config.height != managerSettings_.defaultHeight) {
    camJson["height"] = config.height;
  }

  return camJson;
}

nlohmann::json CameraConfigManager::ManagerSettingsToJson(const CameraManagerSettings& settings) const {
  nlohmann::json settingsJson;

  settingsJson["auto_connect"] = settings.autoConnect;
  settingsJson["default_exposure_time"] = settings.defaultExposureTime;
  settingsJson["default_gain"] = settings.defaultGain;
  settingsJson["broadcast_enabled"] = settings.broadcastEnabled;
  settingsJson["max_subscribers"] = settings.maxSubscribers;
  settingsJson["default_width"] = settings.defaultWidth;
  settingsJson["default_height"] = settings.defaultHeight;
  settingsJson["log_level"] = settings.logLevel;

  return settingsJson;
}

bool CameraConfigManager::ValidateCameraConfig(const CameraConfigData& config, std::vector<std::string>& errors) const {
  bool isValid = true;

  if (config.id.empty()) {
    errors.push_back("Camera ID cannot be empty");
    isValid = false;
  }

  if (config.connectionType == "ip" && config.ipAddress.empty()) {
    errors.push_back("IP camera " + config.id + " missing IP address");
    isValid = false;
  }

  if (config.exposureTime < 0) {
    errors.push_back("Camera " + config.id + " has invalid exposure time");
    isValid = false;
  }

  if (config.gain < 0) {
    errors.push_back("Camera " + config.id + " has invalid gain");
    isValid = false;
  }

  return isValid;
}

bool CameraConfigManager::ValidateManagerSettings(const CameraManagerSettings& settings, std::vector<std::string>& errors) const {
  bool isValid = true;

  if (settings.defaultExposureTime < 0) {
    errors.push_back("Default exposure time cannot be negative");
    isValid = false;
  }

  if (settings.defaultGain < 0) {
    errors.push_back("Default gain cannot be negative");
    isValid = false;
  }

  if (settings.maxSubscribers < 1) {
    errors.push_back("Max subscribers must be at least 1");
    isValid = false;
  }

  return isValid;
}

void CameraConfigManager::LogInfo(const std::string& message) const {
  if (logger_) {
    logger_->LogInfo("[CameraConfig] " + message);
  }
}

void CameraConfigManager::LogWarning(const std::string& message) const {
  if (logger_) {
    logger_->LogWarning("[CameraConfig] " + message);
  }
}

void CameraConfigManager::LogError(const std::string& message) const {
  if (logger_) {
    logger_->LogError("[CameraConfig] " + message);
  }
}

// Helper function to convert CameraConfigData to CameraInfo (for CameraManager compatibility)
// This takes our CameraConfigData (loaded from JSON) and converts it to the existing 
// CameraInfo struct that CameraManager expects
CameraInfo CameraConfigManager::ConvertToCameraInfo(const CameraConfigData& configData) const {
  if (configData.connectionType == "ip") {
    return CameraInfo::CreateByIP(configData.id, configData.ipAddress, configData.displayName, configData.autoConnect);
  }
  else if (configData.connectionType == "serial") {
    // For serial connection, you might need to implement CreateBySerial if not available
    return CameraInfo(configData.id, configData.displayName, configData.autoConnect);
  }
  else if (configData.connectionType == "index") {
    // For device index connection
    return CameraInfo::CreateByIndex(configData.id, configData.port, configData.displayName, configData.autoConnect);
  }
  else {
    // Default to auto-detection
    return CameraInfo(configData.id, configData.displayName, configData.autoConnect);
  }
}

// Convenience methods for common operations
bool CameraConfigManager::SetCameraIPAddress(const std::string& cameraId, const std::string& ipAddress) {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    it->ipAddress = ipAddress;
    it->connectionType = "ip";  // Ensure connection type is set to IP
    LogInfo("Updated IP address for camera " + cameraId + " to: " + ipAddress);
    return true;
  }

  LogWarning("Camera " + cameraId + " not found for IP address update");
  return false;
}

bool CameraConfigManager::EnableCamera(const std::string& cameraId, bool enabled) {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    it->enabled = enabled;
    LogInfo(std::string(enabled ? "Enabled" : "Disabled") + " camera: " + cameraId);
    return true;
  }

  LogWarning("Camera " + cameraId + " not found for enable/disable operation");
  return false;
}

bool CameraConfigManager::SetCameraAutoConnect(const std::string& cameraId, bool autoConnect) {
  auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
    [&cameraId](CameraConfigData& cfg) { return cfg.id == cameraId; });

  if (it != cameraConfigs_.end()) {
    it->autoConnect = autoConnect;
    LogInfo("Set auto-connect for camera " + cameraId + " to: " + std::string(autoConnect ? "true" : "false"));
    return true;
  }

  LogWarning("Camera " + cameraId + " not found for auto-connect setting");
  return false;
}