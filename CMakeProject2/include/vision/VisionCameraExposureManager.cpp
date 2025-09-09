#include "VisionCameraExposureManager.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

VisionCameraExposureManager::VisionCameraExposureManager() {
  // Constructor
}

bool VisionCameraExposureManager::Initialize(const std::string& configPath) {
  if (m_initialized) {
    if (m_logger) {
      m_logger->LogWarning("VisionCameraExposureManager: Already initialized");
    }
    return true;
  }

  m_configPath = configPath;

  if (!LoadConfiguration()) {
    SetError("Failed to load configuration from: " + configPath);
    return false;
  }

  m_initialized = true;

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Initialized with " +
      std::to_string(m_nodeSettings.size()) + " node configurations");
  }

  return true;
}

bool VisionCameraExposureManager::LoadConfiguration() {
  try {
    // Read JSON file
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
      SetError("Cannot open configuration file: " + m_configPath);
      return false;
    }

    file >> m_config;
    file.close();

    // Clear existing settings
    m_nodeSettings.clear();

    // Parse camera_exposure_settings section
    if (m_config.contains("camera_exposure_settings")) {
      auto& settings = m_config["camera_exposure_settings"];

      // Load default settings
      if (settings.contains("default")) {
        if (!ParseSettings(settings["default"], m_defaultSettings)) {
          SetError("Failed to parse default settings");
          return false;
        }
        m_defaultSettings.nodeId = "default";

        if (m_logger) {
          m_logger->LogInfo("VisionCameraExposureManager: Loaded default settings");
        }
      }

      // Load node-specific settings
      if (settings.contains("nodes")) {
        for (auto& [nodeId, nodeConfig] : settings["nodes"].items()) {
          NodeExposureSettings nodeSettings;
          if (ParseSettings(nodeConfig, nodeSettings)) {
            nodeSettings.nodeId = nodeId;
            m_nodeSettings[nodeId] = nodeSettings;

            if (m_logger) {
              m_logger->LogDebug("VisionCameraExposureManager: Loaded settings for node: " +
                nodeId + " - " + nodeSettings.description);
            }
          }
          else {
            if (m_logger) {
              m_logger->LogWarning("VisionCameraExposureManager: Failed to parse settings for node: " + nodeId);
            }
          }
        }
      }
    }
    else {
      SetError("Configuration file missing 'camera_exposure_settings' section");
      return false;
    }

    return true;
  }
  catch (const nlohmann::json::exception& e) {
    SetError("JSON parsing error: " + std::string(e.what()));
    return false;
  }
  catch (const std::exception& e) {
    SetError("Exception loading configuration: " + std::string(e.what()));
    return false;
  }
}

bool VisionCameraExposureManager::ParseSettings(const nlohmann::json& json,
  NodeExposureSettings& settings) {
  try {
    // Parse description (node-specific metadata)
    if (json.contains("description")) {
      settings.description = json["description"].get<std::string>();
    }

    // Parse exposure settings (matches JSON structure)
    if (json.contains("exposure_auto")) {
      settings.auto_exposure = json["exposure_auto"].get<bool>();
    }
    if (json.contains("exposure_time")) {
      settings.exposure_time = json["exposure_time"].get<double>();
    }
    if (json.contains("gain")) {
      settings.gain = json["gain"].get<double>();
    }
    if (json.contains("gain_auto")) {
      settings.auto_gain = json["gain_auto"].get<bool>();
    }

    return ValidateSettings(settings);
  }
  catch (const std::exception& e) {
    SetError("Failed to parse settings: " + std::string(e.what()));
    return false;
  }
}

bool VisionCameraExposureManager::ValidateSettings(const ExposureSettings& settings) const {
  // Validate exposure time range (typical range: 10 us to 1 second)
  if (settings.exposure_time < 10.0 || settings.exposure_time > 1000000.0) {
    if (m_logger) {
      m_logger->LogWarning("VisionCameraExposureManager: Exposure time out of range: " +
        std::to_string(settings.exposure_time) + " us");
    }
    return false;
  }

  // Validate gain range (0-10 based on your config)
  if (settings.gain < 0.0 || settings.gain > 10.0) {
    if (m_logger) {
      m_logger->LogWarning("VisionCameraExposureManager: Gain out of range: " +
        std::to_string(settings.gain));
    }
    return false;
  }

  return true;
}

bool VisionCameraExposureManager::ApplySettingsForNode(ICameraHardware& camera, const std::string& nodeId) {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  if (!camera.IsConnected()) {
    SetError("Camera is not connected");
    return false;
  }

  // Check if node has specific settings
  auto it = m_nodeSettings.find(nodeId);
  if (it != m_nodeSettings.end()) {
    if (m_logger) {
      m_logger->LogInfo("VisionCameraExposureManager: Applying settings for node '" +
        nodeId + "': " + it->second.description);
      m_logger->LogInfo("  Exposure: " + std::to_string(it->second.exposure_time) +
        " us (auto: " + (it->second.auto_exposure ? "ON" : "OFF") +
        "), Gain: " + std::to_string(it->second.gain) +
        " (auto: " + (it->second.auto_gain ? "ON" : "OFF") + ")");
    }
    return ApplySettingsToCamera(camera, it->second);
  }

  // Node not found, use default
  if (m_logger) {
    m_logger->LogWarning("VisionCameraExposureManager: No specific settings for node '" +
      nodeId + "', using default settings");
  }
  return ApplyDefaultSettings(camera);
}

bool VisionCameraExposureManager::ApplyDefaultSettings(ICameraHardware& camera) {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  if (!camera.IsConnected()) {
    SetError("Camera is not connected");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Applying default camera settings");
    m_logger->LogInfo("  Exposure: " + std::to_string(m_defaultSettings.exposure_time) +
      " us (auto: " + (m_defaultSettings.auto_exposure ? "ON" : "OFF") +
      "), Gain: " + std::to_string(m_defaultSettings.gain) +
      " (auto: " + (m_defaultSettings.auto_gain ? "ON" : "OFF") + ")");
  }

  return ApplySettingsToCamera(camera, m_defaultSettings);
}

bool VisionCameraExposureManager::ApplySettingsToCamera(ICameraHardware& camera,
  const ExposureSettings& settings) {
  try {
    if (!camera.IsConnected()) {
      SetError("Camera is not connected");
      return false;
    }

    // Use the ICameraHardware interface to set exposure settings
    if (!camera.SetExposureSettings(settings)) {
      SetError("Failed to apply exposure settings: " + camera.GetLastError());
      return false;
    }

    // Small delay to let settings take effect
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (m_logger) {
      m_logger->LogDebug("VisionCameraExposureManager: Successfully applied camera settings");
    }

    return true;
  }
  catch (const std::exception& e) {
    SetError("Exception applying camera settings: " + std::string(e.what()));
    return false;
  }
}

bool VisionCameraExposureManager::ReadCurrentCameraSettings(ICameraHardware& camera,
  NodeExposureSettings& settings) {
  try {
    if (!camera.IsConnected()) {
      SetError("Camera is not connected");
      return false;
    }

    // Use the ICameraHardware interface to get current settings
    ExposureSettings currentSettings = camera.GetExposureSettings();

    // Copy to NodeExposureSettings
    settings.exposure_time = currentSettings.exposure_time;
    settings.gain = currentSettings.gain;
    settings.auto_exposure = currentSettings.auto_exposure;
    settings.auto_gain = currentSettings.auto_gain;
    settings.description = "Current camera settings";
    settings.nodeId = "current";

    if (m_logger) {
      m_logger->LogInfo("VisionCameraExposureManager: Read current camera settings:");
      m_logger->LogInfo("  Auto Exposure: " + std::string(settings.auto_exposure ? "ON" : "OFF"));
      m_logger->LogInfo("  Exposure Time: " + std::to_string(settings.exposure_time) + " us");
      m_logger->LogInfo("  Auto Gain: " + std::string(settings.auto_gain ? "ON" : "OFF"));
      m_logger->LogInfo("  Gain: " + std::to_string(settings.gain));
    }

    return true;
  }
  catch (const std::exception& e) {
    SetError("Exception reading camera settings: " + std::string(e.what()));
    return false;
  }
}

bool VisionCameraExposureManager::GetNodeSettings(const std::string& nodeId,
  NodeExposureSettings& settings) {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  auto it = m_nodeSettings.find(nodeId);
  if (it != m_nodeSettings.end()) {
    settings = it->second;
    return true;
  }

  SetError("No settings found for node: " + nodeId);
  return false;
}

std::vector<std::string> VisionCameraExposureManager::GetConfiguredNodes() const {
  std::vector<std::string> nodes;
  nodes.reserve(m_nodeSettings.size());

  for (const auto& [nodeId, settings] : m_nodeSettings) {
    nodes.push_back(nodeId);
  }

  return nodes;
}

bool VisionCameraExposureManager::UpdateNodeSettings(const std::string& nodeId,
  const NodeExposureSettings& settings) {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  if (!ValidateSettings(settings)) {
    SetError("Invalid settings for node: " + nodeId);
    return false;
  }

  // Update in-memory settings
  NodeExposureSettings updatedSettings = settings;
  updatedSettings.nodeId = nodeId;
  m_nodeSettings[nodeId] = updatedSettings;

  // Update JSON config
  m_config["camera_exposure_settings"]["nodes"][nodeId] = SettingsToJson(updatedSettings);

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Updated settings for node: " + nodeId);
  }

  return SaveConfiguration();
}

bool VisionCameraExposureManager::RemoveNodeSettings(const std::string& nodeId) {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  auto it = m_nodeSettings.find(nodeId);
  if (it == m_nodeSettings.end()) {
    SetError("Node not found: " + nodeId);
    return false;
  }

  // Remove from in-memory map
  m_nodeSettings.erase(it);

  // Remove from JSON config
  if (m_config["camera_exposure_settings"]["nodes"].contains(nodeId)) {
    m_config["camera_exposure_settings"]["nodes"].erase(nodeId);
  }

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Removed settings for node: " + nodeId);
  }

  return SaveConfiguration();
}

bool VisionCameraExposureManager::SaveConfiguration() {
  try {
    // Update JSON with current settings
    nlohmann::json output;
    output["camera_exposure_settings"]["default"] = SettingsToJson(m_defaultSettings);

    // Add all node settings
    for (const auto& [nodeId, settings] : m_nodeSettings) {
      output["camera_exposure_settings"]["nodes"][nodeId] = SettingsToJson(settings);
    }

    // Write to file with pretty formatting
    std::ofstream file(m_configPath);
    if (!file.is_open()) {
      SetError("Cannot open file for writing: " + m_configPath);
      return false;
    }

    file << std::setw(2) << output << std::endl;
    file.close();

    if (m_logger) {
      m_logger->LogInfo("VisionCameraExposureManager: Saved configuration to: " + m_configPath);
    }

    return true;
  }
  catch (const std::exception& e) {
    SetError("Failed to save configuration: " + std::string(e.what()));
    return false;
  }
}

bool VisionCameraExposureManager::ReloadConfiguration() {
  if (!m_initialized) {
    SetError("Manager not initialized");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Reloading configuration from: " + m_configPath);
  }

  // Clear current settings
  m_nodeSettings.clear();

  // Reload from file
  if (!LoadConfiguration()) {
    SetError("Failed to reload configuration");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("VisionCameraExposureManager: Configuration reloaded successfully");
  }

  return true;
}

nlohmann::json VisionCameraExposureManager::SettingsToJson(const NodeExposureSettings& settings) const {
  nlohmann::json json;

  // Add description if it exists
  if (!settings.description.empty()) {
    json["description"] = settings.description;
  }

  // Add exposure settings (matching the JSON structure in config file)
  json["exposure_auto"] = settings.auto_exposure;
  json["exposure_time"] = settings.exposure_time;
  json["gain"] = settings.gain;
  json["gain_auto"] = settings.auto_gain;

  return json;
}

void VisionCameraExposureManager::SetError(const std::string& error) {
  m_lastError = error;
  if (m_logger) {
    m_logger->LogError("VisionCameraExposureManager: " + error);
  }
  else {
    std::cerr << "VisionCameraExposureManager Error: " << error << std::endl;
  }
}

void VisionCameraExposureManager::PrintCurrentConfiguration() const {
  std::cout << "\n========================================" << std::endl;
  std::cout << "Vision Camera Exposure Configuration" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Config file: " << m_configPath << std::endl;
  std::cout << "Initialized: " << (m_initialized ? "Yes" : "No") << std::endl;

  std::cout << "\nDefault Settings:" << std::endl;
  std::cout << "  Description: " << m_defaultSettings.description << std::endl;
  std::cout << "  Exposure: " << m_defaultSettings.exposure_time << " us" << std::endl;
  std::cout << "  Auto Exposure: " << (m_defaultSettings.auto_exposure ? "ON" : "OFF") << std::endl;
  std::cout << "  Gain: " << m_defaultSettings.gain << std::endl;
  std::cout << "  Auto Gain: " << (m_defaultSettings.auto_gain ? "ON" : "OFF") << std::endl;

  std::cout << "\nNode-specific Settings (" << m_nodeSettings.size() << " nodes):" << std::endl;
  for (const auto& [nodeId, settings] : m_nodeSettings) {
    std::cout << "\n  Node: " << nodeId << std::endl;
    std::cout << "    Description: " << settings.description << std::endl;
    std::cout << "    Exposure: " << settings.exposure_time << " us (auto: "
      << (settings.auto_exposure ? "ON" : "OFF") << ")" << std::endl;
    std::cout << "    Gain: " << settings.gain << " (auto: "
      << (settings.auto_gain ? "ON" : "OFF") << ")" << std::endl;
  }
  std::cout << "========================================\n" << std::endl;
}

void VisionCameraExposureManager::TestCameraSettings(ICameraHardware& camera, const std::string& nodeId) {
  if (!m_initialized) {
    std::cout << "VisionCameraExposureManager: Not initialized" << std::endl;
    return;
  }

  if (!camera.IsConnected()) {
    std::cout << "VisionCameraExposureManager: Camera not connected" << std::endl;
    std::cout << "Camera Type: " <<
      (camera.GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" :
        camera.GetCameraType() == ICameraHardware::CameraType::IDS ? "IDS" : "Unknown")
      << std::endl;
    return;
  }

  std::cout << "\n=== CAMERA INFORMATION ===" << std::endl;
  std::cout << "Camera Type: " <<
    (camera.GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" :
      camera.GetCameraType() == ICameraHardware::CameraType::IDS ? "IDS" : "Unknown")
    << std::endl;
  std::cout << "Model: " << camera.GetModelName() << std::endl;
  std::cout << "Serial: " << camera.GetSerialNumber() << std::endl;
  std::cout << "Vendor: " << camera.GetVendorName() << std::endl;
  std::cout << "Camera ID: " << camera.GetCameraId() << std::endl;

  if (nodeId.empty()) {
    // Test reading current settings
    std::cout << "\n=== READING CURRENT CAMERA SETTINGS ===" << std::endl;
    NodeExposureSettings current;
    if (ReadCurrentCameraSettings(camera, current)) {
      std::cout << "Successfully read camera settings:" << std::endl;
      std::cout << "  Exposure Time: " << current.exposure_time << " us" << std::endl;
      std::cout << "  Auto Exposure: " << (current.auto_exposure ? "ON" : "OFF") << std::endl;
      std::cout << "  Gain: " << current.gain << std::endl;
      std::cout << "  Auto Gain: " << (current.auto_gain ? "ON" : "OFF") << std::endl;
    }
    else {
      std::cout << "Failed to read camera settings: " << m_lastError << std::endl;
    }
  }
  else {
    // Test applying settings for specific node
    std::cout << "\n=== TESTING CAMERA SETTINGS FOR NODE: " << nodeId << " ===" << std::endl;

    // Show what settings will be applied
    NodeExposureSettings nodeSettings;
    if (GetNodeSettings(nodeId, nodeSettings)) {
      std::cout << "Node Settings to Apply:" << std::endl;
      std::cout << "  Description: " << nodeSettings.description << std::endl;
      std::cout << "  Exposure Time: " << nodeSettings.exposure_time << " us" << std::endl;
      std::cout << "  Auto Exposure: " << (nodeSettings.auto_exposure ? "ON" : "OFF") << std::endl;
      std::cout << "  Gain: " << nodeSettings.gain << std::endl;
      std::cout << "  Auto Gain: " << (nodeSettings.auto_gain ? "ON" : "OFF") << std::endl;
    }
    else {
      std::cout << "Node not found, will use default settings" << std::endl;
    }

    // Apply settings
    std::cout << "\nApplying settings..." << std::endl;
    if (ApplySettingsForNode(camera, nodeId)) {
      std::cout << "Successfully applied settings for node: " << nodeId << std::endl;

      // Read back and verify
      std::cout << "\nVerifying applied settings..." << std::endl;
      NodeExposureSettings current;
      if (ReadCurrentCameraSettings(camera, current)) {
        std::cout << "Current Camera Settings after applying:" << std::endl;
        std::cout << "  Exposure Time: " << current.exposure_time << " us" << std::endl;
        std::cout << "  Auto Exposure: " << (current.auto_exposure ? "ON" : "OFF") << std::endl;
        std::cout << "  Gain: " << current.gain << std::endl;
        std::cout << "  Auto Gain: " << (current.auto_gain ? "ON" : "OFF") << std::endl;

        // Check if settings match
        bool match = true;
        if (GetNodeSettings(nodeId, nodeSettings)) {
          if (std::abs(current.exposure_time - nodeSettings.exposure_time) > 1.0) {
            std::cout << "WARNING: Exposure time mismatch!" << std::endl;
            match = false;
          }
          if (std::abs(current.gain - nodeSettings.gain) > 0.1) {
            std::cout << "WARNING: Gain mismatch!" << std::endl;
            match = false;
          }
          if (current.auto_exposure != nodeSettings.auto_exposure) {
            std::cout << "WARNING: Auto exposure setting mismatch!" << std::endl;
            match = false;
          }
          if (current.auto_gain != nodeSettings.auto_gain) {
            std::cout << "WARNING: Auto gain setting mismatch!" << std::endl;
            match = false;
          }

          if (match) {
            std::cout << "\n✓ Settings successfully verified!" << std::endl;
          }
        }
      }
      else {
        std::cout << "Failed to verify settings: " << m_lastError << std::endl;
      }
    }
    else {
      std::cout << "Failed to apply settings: " << m_lastError << std::endl;
    }
  }

  std::cout << "========================================" << std::endl;
}