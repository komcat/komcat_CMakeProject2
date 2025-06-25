#include "CameraExposureManager.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include "imgui.h"

CameraExposureManager::CameraExposureManager(const std::string& configPath)
  : m_configPath(configPath)
  , m_showUI(false)
  , m_editingDefault(false)
  , m_settingsApplySuccess(true)
  , m_lastModified(std::chrono::steady_clock::now())
{
  // Set default settings
  m_defaultSettings = CameraExposureSettings(10000.0, 0.0, false, false, "Default camera settings");

  // Try to load configuration
  if (!LoadConfiguration(configPath)) {
    std::cout << "Could not load camera exposure config from " << configPath
      << ", using defaults" << std::endl;
  }
}

CameraExposureManager::~CameraExposureManager() {
  // Optionally save configuration on exit
  // SaveConfiguration();
}

bool CameraExposureManager::LoadConfiguration(const std::string& configPath) {
  try {
    std::cout << "Loading camera exposure configuration from: " << configPath << std::endl;

    std::ifstream file(configPath);
    if (!file.is_open()) {
      std::cerr << "Cannot open camera exposure config file: " << configPath << std::endl;
      return false;
    }

    nlohmann::json config;
    file >> config;
    file.close();

    // Store old settings count for comparison
    size_t oldSettingsCount = m_nodeSettings.size();

    bool success = ParseConfiguration(config);

    if (success) {
      // Update modification timestamp
      UpdateModificationTime();

      std::cout << "[Yes] Configuration loaded successfully!" << std::endl;
      std::cout << "  - Previous settings: " << oldSettingsCount << " nodes" << std::endl;
      std::cout << "  - New settings: " << m_nodeSettings.size() << " nodes" << std::endl;
    }

    return success;
  }
  catch (const std::exception& e) {
    std::cerr << "Error loading camera exposure configuration: " << e.what() << std::endl;
    return false;
  }
}

bool CameraExposureManager::SaveConfiguration(const std::string& configPath) {
  try {
    std::string savePath = configPath.empty() ? m_configPath : configPath;

    nlohmann::json config;
    config["camera_exposure_settings"]["default"] = SettingsToJson(m_defaultSettings);

    nlohmann::json nodesJson;
    for (const auto& [nodeId, settings] : m_nodeSettings) {
      nodesJson[nodeId] = SettingsToJson(settings);
    }
    config["camera_exposure_settings"]["nodes"] = nodesJson;

    std::ofstream file(savePath);
    if (!file.is_open()) {
      std::cerr << "Cannot save camera exposure config to: " << savePath << std::endl;
      return false;
    }

    file << std::setw(2) << config << std::endl;
    file.close();

    std::cout << "Camera exposure configuration saved to: " << savePath << std::endl;
    UpdateModificationTime();
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error saving camera exposure configuration: " << e.what() << std::endl;
    return false;
  }
}

bool CameraExposureManager::ParseConfiguration(const nlohmann::json& config) {
  try {
    if (!config.contains("camera_exposure_settings")) {
      std::cerr << "Missing 'camera_exposure_settings' in config" << std::endl;
      return false;
    }

    const auto& settings = config["camera_exposure_settings"];

    // Load default settings
    if (settings.contains("default")) {
      m_defaultSettings = JsonToSettings(settings["default"]);
      std::cout << "  - Default settings: "
        << m_defaultSettings.exposure_time << "us, "
        << m_defaultSettings.gain << "dB" << std::endl;
    }

    // Load node-specific settings
    if (settings.contains("nodes")) {
      std::cout << "  - Clearing existing node settings..." << std::endl;
      m_nodeSettings.clear();  // Clear existing settings

      std::cout << "  - Loading node settings:" << std::endl;
      for (const auto& [nodeId, nodeConfig] : settings["nodes"].items()) {
        auto nodeSettings = JsonToSettings(nodeConfig);
        m_nodeSettings[nodeId] = nodeSettings;

        std::cout << "    * " << nodeId << ": "
          << nodeSettings.exposure_time << "us, "
          << nodeSettings.gain << "dB"
          << " (" << nodeSettings.description << ")" << std::endl;
      }
    }

    std::cout << "Loaded camera exposure settings for " << m_nodeSettings.size()
      << " nodes" << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error parsing camera exposure configuration: " << e.what() << std::endl;
    return false;
  }
}

nlohmann::json CameraExposureManager::SettingsToJson(const CameraExposureSettings& settings) const {
  nlohmann::json json;
  json["exposure_time"] = settings.exposure_time;
  json["gain"] = settings.gain;
  json["exposure_auto"] = settings.exposure_auto;
  json["gain_auto"] = settings.gain_auto;
  if (!settings.description.empty()) {
    json["description"] = settings.description;
  }
  return json;
}

CameraExposureSettings CameraExposureManager::JsonToSettings(const nlohmann::json& json) const {
  CameraExposureSettings settings;

  if (json.contains("exposure_time")) {
    settings.exposure_time = json["exposure_time"].get<double>();
  }
  if (json.contains("gain")) {
    settings.gain = json["gain"].get<double>();
  }
  if (json.contains("exposure_auto")) {
    settings.exposure_auto = json["exposure_auto"].get<bool>();
  }
  if (json.contains("gain_auto")) {
    settings.gain_auto = json["gain_auto"].get<bool>();
  }
  if (json.contains("description")) {
    settings.description = json["description"].get<std::string>();
  }

  return settings;
}

// OPTIMIZED: Return const reference to avoid copy
const CameraExposureSettings& CameraExposureManager::GetSettingsForNodeRef(const std::string& nodeId) const {
  auto it = m_nodeSettings.find(nodeId);  // O(1) lookup with unordered_map
  if (it != m_nodeSettings.end()) {
    return it->second;  // Return reference, no copy
  }
  return m_defaultSettings;  // Return reference to default
}

// LEGACY: Keep original method for backward compatibility (but mark it as slower)
CameraExposureSettings CameraExposureManager::GetSettingsForNode(const std::string& nodeId) const {
  return GetSettingsForNodeRef(nodeId);  // Delegate to optimized version, but still returns copy
}

// OPTIMIZED: Batch method to get all settings at once
std::vector<std::pair<std::string, const CameraExposureSettings*>>
CameraExposureManager::GetAllSettingsForNodes(const std::vector<std::string>& nodeIds) const {
  std::vector<std::pair<std::string, const CameraExposureSettings*>> result;
  result.reserve(nodeIds.size());

  for (const auto& nodeId : nodeIds) {
    auto it = m_nodeSettings.find(nodeId);
    if (it != m_nodeSettings.end()) {
      result.emplace_back(nodeId, &it->second);
    }
    else {
      result.emplace_back(nodeId, &m_defaultSettings);
    }
  }
  return result;
}

// OPTIMIZED: Check and get in one operation
std::pair<bool, const CameraExposureSettings&>
CameraExposureManager::TryGetSettingsForNode(const std::string& nodeId) const {
  auto it = m_nodeSettings.find(nodeId);
  if (it != m_nodeSettings.end()) {
    return { true, it->second };
  }
  return { false, m_defaultSettings };
}

void CameraExposureManager::SetSettingsForNode(const std::string& nodeId, const CameraExposureSettings& settings) {
  m_nodeSettings[nodeId] = settings;
  UpdateModificationTime();
}

// OPTIMIZED: Single lookup instead of separate find operation
bool CameraExposureManager::HasSettingsForNode(const std::string& nodeId) const {
  return m_nodeSettings.find(nodeId) != m_nodeSettings.end();
}

// Modify ApplySettingsForNode:
bool CameraExposureManager::ApplySettingsForNode(PylonCamera& camera, const std::string& nodeId) {
  auto [hasSettings, settings] = TryGetSettingsForNode(nodeId);

  // Skip if we're already using these exact settings
  if (m_currentAppliedNodeId == nodeId &&
    m_currentAppliedSettings.exposure_time == settings.exposure_time &&
    m_currentAppliedSettings.gain == settings.gain) {
    std::cout << "Exposure settings already applied for node " << nodeId << std::endl;
    return true;
  }


  if (!hasSettings) {
    std::cout << "No specific exposure settings for node " << nodeId
      << ", applying default settings" << std::endl;
    return ApplyDefaultSettings(camera);
  }

  std::cout << "Applying camera exposure settings for node " << nodeId
    << " (" << settings.description << ")" << std::endl;

  bool success = ApplyCameraSettings(camera, settings);

  // Track what was applied
  m_lastAppliedNode = nodeId;
  m_lastAppliedSettings = settings;
  m_settingsApplySuccess = success;

  // Call callback if set
  if (m_settingsAppliedCallback) {
    m_settingsAppliedCallback(nodeId, settings);
  }


  // Cache what we applied
  if (success) {
    m_currentAppliedNodeId = nodeId;
    m_currentAppliedSettings = settings;
  }

  return success;
}

bool CameraExposureManager::ApplyDefaultSettings(PylonCamera& camera) {
  std::cout << "Applying default camera exposure settings" << std::endl;

  bool success = ApplyCameraSettings(camera, m_defaultSettings);

  // Track what was applied
  m_lastAppliedNode = "default";
  m_lastAppliedSettings = m_defaultSettings;
  m_settingsApplySuccess = success;

  // Call callback if set
  if (m_settingsAppliedCallback) {
    m_settingsAppliedCallback("default", m_defaultSettings);
  }

  return success;
}

void CameraExposureManager::SetSettingsAppliedCallback(std::function<void(const std::string&, const CameraExposureSettings&)> callback) {
  m_settingsAppliedCallback = callback;
}


bool CameraExposureManager::ApplyCameraSettings(PylonCamera& camera, const CameraExposureSettings& settings) {
  if (!camera.IsConnected()) {
    m_lastErrorMessage = "Camera is not connected";
    std::cerr << "Cannot apply camera settings: " << m_lastErrorMessage << std::endl;
    return false;
  }

  try {
    auto& internalCamera = camera.GetInternalCamera();

    if (!internalCamera.IsOpen()) {
      m_lastErrorMessage = "Camera is not open";
      std::cerr << "Cannot apply camera settings: " << m_lastErrorMessage << std::endl;
      return false;
    }

    std::cout << "Applying camera settings:" << std::endl;
    std::cout << "  Target Exposure: " << settings.exposure_time << " us" << std::endl;
    std::cout << "  Target Gain: " << settings.gain << " (0-10 scale)" << std::endl;

    // Set exposure mode to Timed first (required for Basler cameras)
    try {
      if (internalCamera.GetNodeMap().GetNode("ExposureMode")) {
        Pylon::CEnumParameter exposureMode(internalCamera.GetNodeMap(), "ExposureMode");
        exposureMode.SetValue("Timed");
        std::cout << "  Exposure Mode: Timed" << std::endl;
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cerr << "Warning: Could not set exposure mode: " << e.GetDescription() << std::endl;
    }

    // Set exposure auto mode
    try {
      if (internalCamera.GetNodeMap().GetNode("ExposureAuto")) {
        Pylon::CEnumParameter exposureAuto(internalCamera.GetNodeMap(), "ExposureAuto");
        exposureAuto.SetValue(settings.exposure_auto ? "Continuous" : "Off");
        std::cout << "  Exposure Auto: " << (settings.exposure_auto ? "Continuous" : "Off") << std::endl;
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cerr << "Warning: Could not set exposure auto: " << e.GetDescription() << std::endl;
    }

    // Set exposure time (using ExposureTimeAbs for Basler cameras)
    if (!settings.exposure_auto) {
      try {
        // Use ExposureTimeAbs for Basler cameras (microseconds)
        if (internalCamera.GetNodeMap().GetNode("ExposureTimeAbs")) {
          Pylon::CFloatParameter exposureTime(internalCamera.GetNodeMap(), "ExposureTimeAbs");

          // Get valid range for this camera
          double minExposure = exposureTime.GetMin();
          double maxExposure = exposureTime.GetMax();

          // Clamp to valid range
          double clampedExposure = (std::max)(minExposure, (std::min)(maxExposure, settings.exposure_time));

          exposureTime.SetValue(clampedExposure);
          std::cout << "  Exposure Time Abs: " << clampedExposure << " us"
            << " (range: " << minExposure << "-" << maxExposure << " us)" << std::endl;

          if (std::abs(clampedExposure - settings.exposure_time) > 1.0) {
            std::cout << "  Warning: Exposure clamped from " << settings.exposure_time
              << " to " << clampedExposure << " us" << std::endl;
          }
        }
        else {
          std::cout << "  Warning: ExposureTimeAbs parameter not found" << std::endl;
        }
      }
      catch (const Pylon::GenericException& e) {
        std::cerr << "Warning: Could not set exposure time: " << e.GetDescription() << std::endl;
      }
    }

    // Set gain auto mode first
    try {
      if (internalCamera.GetNodeMap().GetNode("GainAuto")) {
        Pylon::CEnumParameter gainAuto(internalCamera.GetNodeMap(), "GainAuto");
        gainAuto.SetValue(settings.gain_auto ? "Continuous" : "Off");
        std::cout << "  Gain Auto: " << (settings.gain_auto ? "Continuous" : "Off") << std::endl;
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cerr << "Warning: Could not set gain auto: " << e.GetDescription() << std::endl;
    }

    // Set gain value (using GainRaw for Basler cameras)
    if (!settings.gain_auto) {
      try {
        // First set GainSelector to AnalogAll (recommended for most cases)
        if (internalCamera.GetNodeMap().GetNode("GainSelector")) {
          Pylon::CEnumParameter gainSelector(internalCamera.GetNodeMap(), "GainSelector");
          gainSelector.SetValue("AnalogAll");
          std::cout << "  Gain Selector: AnalogAll" << std::endl;
        }

        // Now set GainRaw value
        if (internalCamera.GetNodeMap().GetNode("GainRaw")) {
          Pylon::CIntegerParameter gainRaw(internalCamera.GetNodeMap(), "GainRaw");

          // Get valid range for this camera
          int64_t minGain = gainRaw.GetMin();
          int64_t maxGain = gainRaw.GetMax();

          // Convert our "gain" setting (0-10 range) to camera's raw range
          // Map 0-10 to the camera's min-max range
          int64_t rawGain;
          if (settings.gain <= 0) {
            rawGain = minGain; // Minimum gain
          }
          else if (settings.gain >= 10) {
            rawGain = maxGain; // Maximum gain
          }
          else {
            // Linear interpolation between min and max
            double ratio = settings.gain / 10.0; // 0.0 to 1.0
            rawGain = static_cast<int64_t>(minGain + (maxGain - minGain) * ratio);
          }

          gainRaw.SetValue(rawGain);
          std::cout << "  Gain Raw: " << rawGain << " (from setting " << settings.gain
            << ", range: " << minGain << "-" << maxGain << ")" << std::endl;
        }
      }
      catch (const Pylon::GenericException& e) {
        std::cerr << "Warning: Could not set gain: " << e.GetDescription() << std::endl;
      }
    }

    m_lastErrorMessage.clear();
    std::cout << "Camera settings applied successfully" << std::endl;
    return true;
  }
  catch (const Pylon::GenericException& e) {
    m_lastErrorMessage = std::string("Pylon error: ") + e.GetDescription();
    std::cerr << "Error applying camera settings: " << m_lastErrorMessage << std::endl;
    return false;
  }
  catch (const std::exception& e) {
    m_lastErrorMessage = std::string("Standard error: ") + e.what();
    std::cerr << "Error applying camera settings: " << m_lastErrorMessage << std::endl;
    return false;
  }
}

void CameraExposureManager::RenderUI() {
  if (!m_showUI) return;

  ImGui::Begin("Camera Exposure Manager", &m_showUI);

  // Status section
  ImGui::Text("Status");
  ImGui::Separator();

  if (!m_lastAppliedNode.empty()) {
    ImGui::Text("Last Applied: %s", m_lastAppliedNode.c_str());
    ImGui::Text("Exposure Time: %.0f us", m_lastAppliedSettings.exposure_time);
    ImGui::Text("Gain: %.1f dB", m_lastAppliedSettings.gain);
    ImGui::Text("Exposure Auto: %s", m_lastAppliedSettings.exposure_auto ? "On" : "Off");
    ImGui::Text("Gain Auto: %s", m_lastAppliedSettings.gain_auto ? "On" : "Off");

    if (m_settingsApplySuccess) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Settings applied successfully");
    }
    else {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to apply settings");
      if (!m_lastErrorMessage.empty()) {
        ImGui::TextWrapped("Error: %s", m_lastErrorMessage.c_str());
      }
    }
  }
  else {
    ImGui::Text("No settings applied yet");
  }

  ImGui::Spacing();

  // Configuration section
  ImGui::Text("Configuration");
  ImGui::Separator();

  // Default settings editing
  if (ImGui::CollapsingHeader("Default Settings")) {
    static CameraExposureSettings defaultEdit = m_defaultSettings;

    ImGui::InputDouble("Exposure Time (us)", &defaultEdit.exposure_time, 1.0f, 5000.0, "%.0f");
    ImGui::InputDouble("Gain (dB)", &defaultEdit.gain, 0.1, 1.0, "%.1f");
    ImGui::Checkbox("Exposure Auto", &defaultEdit.exposure_auto);
    ImGui::Checkbox("Gain Auto", &defaultEdit.gain_auto);

    // Handle description input safely
    static char descBuffer[256];
    strncpy_s(descBuffer, sizeof(descBuffer), defaultEdit.description.c_str(), sizeof(descBuffer) - 1);
    descBuffer[sizeof(descBuffer) - 1] = '\0';

    if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer))) {
      defaultEdit.description = std::string(descBuffer);
    }

    if (ImGui::Button("Apply Default Settings")) {
      m_defaultSettings = defaultEdit;
      UpdateModificationTime();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Default")) {
      defaultEdit = m_defaultSettings;
    }
  }


  // Add this section to CameraExposureManager::RenderUI() method
// Insert after the "Default Settings" section and before "Node Settings"

// Add New Node Settings section
  if (ImGui::CollapsingHeader("Add New Node Settings")) {

    // Structure to hold node info
    struct NodeInfo {
      std::string nodeId;
      std::string positionName;
      std::string displayText;
    };

    static char newNodeId[256] = "";
    static CameraExposureSettings newNodeSettings;
    static bool copyFromExisting = false;
    static char copyFromNodeId[256] = "";
    static std::vector<std::string> motionNodes;
    static std::vector<NodeInfo> motionNodeInfos;  // ADD THIS LINE - was missing!
    static bool motionNodesLoaded = false;
    static int selectedMotionNodeIndex = -1;

    // Load motion nodes from config (one-time load)
    if (!motionNodesLoaded) {
      motionNodes.clear();
      motionNodeInfos.clear();  // Clear this too
      try {
        std::ifstream motionConfigFile("motion_config.json");
        if (motionConfigFile.is_open()) {
          nlohmann::json motionConfig;
          motionConfigFile >> motionConfig;

          // Extract nodes from Process_Flow graph
          if (motionConfig.contains("Graphs") &&
            motionConfig["Graphs"].contains("Process_Flow") &&
            motionConfig["Graphs"]["Process_Flow"].contains("Nodes")) {

            for (const auto& node : motionConfig["Graphs"]["Process_Flow"]["Nodes"]) {
              if (node.contains("Id")) {
                NodeInfo nodeInfo;
                nodeInfo.nodeId = node["Id"].get<std::string>();

                // Get position name if available
                if (node.contains("Position")) {
                  nodeInfo.positionName = node["Position"].get<std::string>();
                  nodeInfo.displayText = nodeInfo.nodeId + " (" + nodeInfo.positionName + ")";
                }
                else {
                  nodeInfo.positionName = "";
                  nodeInfo.displayText = nodeInfo.nodeId;
                }

                motionNodes.push_back(nodeInfo.nodeId); // Keep for compatibility
                motionNodeInfos.push_back(nodeInfo);    // Add to the new vector
              }
            }
          }
          motionConfigFile.close();
          std::cout << "Loaded " << motionNodeInfos.size() << " motion nodes for camera exposure UI" << std::endl;
        }
      }
      catch (const std::exception& e) {
        std::cout << "Could not load motion nodes: " << e.what() << std::endl;
      }
      motionNodesLoaded = true;
    }

    ImGui::Text("Create exposure settings for a new node:");

    // Motion Config Node Selector
    ImGui::Text("Select from Motion Config:");
    if (!motionNodeInfos.empty()) {
      // Create a combo box with motion nodes showing node ID and position name
      std::string previewText = selectedMotionNodeIndex >= 0 ?
        motionNodeInfos[selectedMotionNodeIndex].displayText : "Select a node...";

      if (ImGui::BeginCombo("Motion Nodes", previewText.c_str())) {

        for (int i = 0; i < motionNodeInfos.size(); i++) {
          const NodeInfo& nodeInfo = motionNodeInfos[i];
          bool isSelected = (selectedMotionNodeIndex == i);

          // Check if this node already has exposure settings
          bool hasSettings = HasSettingsForNode(nodeInfo.nodeId);

          // Set text color: bright white for available nodes, gray for nodes with existing settings
          ImVec4 textColor = hasSettings ? ImVec4(0.6f, 0.6f, 0.6f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

          // Override text color when selected to ensure readability
          if (isSelected) {
            // Use black text on blue selection background
            textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
          }

          ImGui::PushStyleColor(ImGuiCol_Text, textColor);
          if (ImGui::Selectable(nodeInfo.displayText.c_str(), isSelected)) {
            selectedMotionNodeIndex = i;
            // Copy the selected node ID to the input field
            strncpy(newNodeId, nodeInfo.nodeId.c_str(), sizeof(newNodeId) - 1);
            newNodeId[sizeof(newNodeId) - 1] = '\0';
          }
          ImGui::PopStyleColor();

          if (hasSettings && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Node already has exposure settings");
          }

          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }
    else {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No motion nodes found in motion_config.json");
    }

    ImGui::Separator();
    ImGui::Text("Or enter custom Node ID:");

    // Manual Node ID input
    ImGui::InputText("Node ID", newNodeId, sizeof(newNodeId));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      newNodeId[0] = '\0';
      selectedMotionNodeIndex = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Motion Config")) {
      motionNodesLoaded = false; // Force reload on next frame
    }

    // Copy from existing node option
    ImGui::Checkbox("Copy from existing node", &copyFromExisting);

    if (copyFromExisting) {
      ImGui::InputText("Copy from Node ID", copyFromNodeId, sizeof(copyFromNodeId));
      ImGui::SameLine();
      if (ImGui::Button("Copy Settings")) {
        std::string sourceNodeId(copyFromNodeId);
        if (HasSettingsForNode(sourceNodeId)) {
          newNodeSettings = GetSettingsForNode(sourceNodeId);
          // Update description
          newNodeSettings.description = "Copied from " + sourceNodeId;
        }
        else {
          newNodeSettings = m_defaultSettings;
          newNodeSettings.description = "Copied from default";
        }
      }
    }

    // New node settings configuration
    ImGui::Separator();
    ImGui::Text("Configure Settings for New Node:");

    ImGui::InputDouble("Exposure Time (us)", &newNodeSettings.exposure_time, 1.0f, 100.0f, "%.0f");
    ImGui::InputDouble("Gain (0-10 scale)", &newNodeSettings.gain, 0.1, 1.0, "%.1f");
    ImGui::Checkbox("Exposure Auto", &newNodeSettings.exposure_auto);
    ImGui::Checkbox("Gain Auto", &newNodeSettings.gain_auto);

    // Description input
    static char descBuffer[256];
    strncpy(descBuffer, newNodeSettings.description.c_str(), sizeof(descBuffer) - 1);
    descBuffer[sizeof(descBuffer) - 1] = '\0';

    if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer))) {
      newNodeSettings.description = std::string(descBuffer);
    }

    // Add node button
    ImGui::Separator();
    bool canAddNode = strlen(newNodeId) > 0;

    // Test Current Settings button - add this BEFORE the Add Node Settings button
    if (ImGui::Button("Test Current Settings on Camera")) {
      // Apply the current settings temporarily to the camera for preview
      std::cout << "Testing exposure settings on camera:" << std::endl;
      std::cout << "  Exposure Time: " << newNodeSettings.exposure_time << " μs" << std::endl;
      std::cout << "  Gain: " << newNodeSettings.gain << " dB" << std::endl;
      std::cout << "  Exposure Auto: " << (newNodeSettings.exposure_auto ? "On" : "Off") << std::endl;
      std::cout << "  Gain Auto: " << (newNodeSettings.gain_auto ? "On" : "Off") << std::endl;

      // Apply settings to camera using the same method as existing nodes
      // This uses the existing ApplyCameraSettings method
      try {
        // Get a camera instance and check if it's valid
        PylonCamera* testCamera = GetTestCamera();
        if (!testCamera) {
          std::cout << "✗ No test camera available" << std::endl;
          ImGui::OpenPopup("Test Settings Failed");
          return; // Exit early if no camera
        }

        if (!testCamera->IsConnected()) {
          std::cout << "✗ Test camera is not connected" << std::endl;
          ImGui::OpenPopup("Test Settings Failed");
          return; // Exit early if camera not connected
        }

        // Create a temporary camera settings object
        CameraExposureSettings testSettings = newNodeSettings;
        testSettings.description = "Test settings from UI";

        // Apply the settings using the existing camera application logic
        // Dereference the pointer to get a reference for ApplyCameraSettings
        bool success = ApplyCameraSettings(*testCamera, testSettings);

        if (success) {
          std::cout << "✓ Test settings applied successfully to camera" << std::endl;
          ImGui::OpenPopup("Test Settings Applied");
        }
        else {
          std::cout << "✗ Failed to apply test settings to camera" << std::endl;
          ImGui::OpenPopup("Test Settings Failed");
        }

      }
      catch (const std::exception& e) {
        std::cout << "Error applying test settings: " << e.what() << std::endl;
        ImGui::OpenPopup("Test Settings Failed");
      }
    }

    // Show popup confirmation that settings were applied for testing
    if (ImGui::BeginPopupModal("Test Settings Applied", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Settings successfully applied to camera for testing!");
      ImGui::Text("Check the camera image to verify the exposure looks good.");
      ImGui::Separator();
      ImGui::Text("Applied settings:");
      ImGui::Text("  Exposure Time: %.0f μs", newNodeSettings.exposure_time);
      ImGui::Text("  Gain: %.1f dB", newNodeSettings.gain);
      ImGui::Text("  Exposure Auto: %s", newNodeSettings.exposure_auto ? "On" : "Off");
      ImGui::Text("  Gain Auto: %s", newNodeSettings.gain_auto ? "On" : "Off");
      ImGui::Separator();
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Note: These are temporary test settings.");
      ImGui::Text("Click 'Add Node Settings' to save them permanently.");

      if (ImGui::Button("OK")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // Show popup for failed settings application
    if (ImGui::BeginPopupModal("Test Settings Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Failed to apply settings to camera!");
      ImGui::Text("Make sure the camera is connected and grabbing.");
      ImGui::Separator();
      ImGui::Text("Attempted settings:");
      ImGui::Text("  Exposure Time: %.0f μs", newNodeSettings.exposure_time);
      ImGui::Text("  Gain: %.1f dB", newNodeSettings.gain);

      if (ImGui::Button("OK")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Add verification button
    if (ImGui::Button("Verify Current Camera Settings")) {
      std::cout << "Reading current camera settings for verification..." << std::endl;

      try {
        // Get camera instance and check if it's valid
        PylonCamera* testCamera = GetTestCamera();
        if (!testCamera) {
          std::cout << "✗ No test camera available for verification" << std::endl;
          return; // Exit early if no camera
        }

        if (!testCamera->IsConnected()) {
          std::cout << "✗ Test camera is not connected" << std::endl;
          return; // Exit early if camera not connected
        }

        // Read what's actually applied to the camera (dereference pointer to reference)
        CameraExposureSettings currentSettings = ReadCurrentCameraSettings(*testCamera);

        std::cout << "Current camera settings:" << std::endl;
        std::cout << "  Actual Exposure Time: " << currentSettings.exposure_time << " μs" << std::endl;
        std::cout << "  Actual Gain: " << currentSettings.gain << " dB" << std::endl;
        std::cout << "  Actual Exposure Auto: " << (currentSettings.exposure_auto ? "On" : "Off") << std::endl;
        std::cout << "  Actual Gain Auto: " << (currentSettings.gain_auto ? "On" : "Off") << std::endl;

        ImGui::OpenPopup("Current Camera Settings");

      }
      catch (const std::exception& e) {
        std::cout << "Error reading camera settings: " << e.what() << std::endl;
      }
    }

    // Show popup with current camera settings
    if (ImGui::BeginPopupModal("Current Camera Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Current camera settings (as read from hardware):");
      ImGui::Separator();

      try {
        PylonCamera* testCamera = GetTestCamera();
        if (testCamera && testCamera->IsConnected()) {
          CameraExposureSettings currentSettings = ReadCurrentCameraSettings(*testCamera);
          ImGui::Text("Exposure Time: %.0f μs", currentSettings.exposure_time);
          ImGui::Text("Gain: %.1f dB", currentSettings.gain);
          ImGui::Text("Exposure Auto: %s", currentSettings.exposure_auto ? "On" : "Off");
          ImGui::Text("Gain Auto: %s", currentSettings.gain_auto ? "On" : "Off");
        }
        else {
          ImGui::TextColored(ImVec4(1, 0, 0, 1), "Camera not available or not connected");
        }
      }
      catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error reading camera: %s", e.what());
      }

      if (ImGui::Button("OK")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (!canAddNode) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Add Node Settings")) {
      std::string nodeId(newNodeId);

      // Check if node already exists
      if (HasSettingsForNode(nodeId)) {
        // Show confirmation dialog or warning
        ImGui::OpenPopup("Node Exists");
      }
      else {
        // Add the new node settings
        SetSettingsForNode(nodeId, newNodeSettings);
        UpdateModificationTime();

        // Clear the form
        newNodeId[0] = '\0';
        newNodeSettings = m_defaultSettings;
        newNodeSettings.description = "";

        std::cout << "Added new exposure settings for node: " << nodeId << std::endl;
      }
    }

    if (!canAddNode) {
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Enter a Node ID");
    }

    // Confirmation popup for existing nodes
    if (ImGui::BeginPopupModal("Node Exists", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Node '%s' already has exposure settings.", newNodeId);
      ImGui::Text("Do you want to overwrite the existing settings?");
      ImGui::Separator();

      if (ImGui::Button("Yes, Overwrite")) {
        std::string nodeId(newNodeId);
        SetSettingsForNode(nodeId, newNodeSettings);
        UpdateModificationTime();

        // Clear the form
        newNodeId[0] = '\0';
        newNodeSettings = m_defaultSettings;
        newNodeSettings.description = "";

        std::cout << "Overwrote exposure settings for node: " << nodeId << std::endl;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("No, Cancel")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // Quick presets section
    ImGui::Separator();
    ImGui::Text("Quick Presets:");

    if (ImGui::Button("Low Light (High Exposure)")) {
      newNodeSettings.exposure_time = 15000.0;
      newNodeSettings.gain = 3.0;
      newNodeSettings.exposure_auto = false;
      newNodeSettings.gain_auto = false;
      newNodeSettings.description = "Low light viewing preset";
    }
    ImGui::SameLine();
    if (ImGui::Button("Bright Light (Low Exposure)")) {
      newNodeSettings.exposure_time = 500.0;
      newNodeSettings.gain = 0.0;
      newNodeSettings.exposure_auto = false;
      newNodeSettings.gain_auto = false;
      newNodeSettings.description = "Bright light viewing preset";
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto Mode")) {
      newNodeSettings.exposure_time = 5000.0;
      newNodeSettings.gain = 1.0;
      newNodeSettings.exposure_auto = true;
      newNodeSettings.gain_auto = true;
      newNodeSettings.description = "Automatic exposure and gain";
    }

    // Show existing nodes for reference
    if (ImGui::CollapsingHeader("Existing Nodes (for reference)")) {
      ImGui::BeginChild("ExistingNodesList", ImVec2(0, 150), true);
      for (const auto& [nodeId, settings] : m_nodeSettings) {
        if (ImGui::Selectable(nodeId.c_str())) {
          // Copy the node ID to the copy field
          strncpy(copyFromNodeId, nodeId.c_str(), sizeof(copyFromNodeId) - 1);
          copyFromNodeId[sizeof(copyFromNodeId) - 1] = '\0';
          copyFromExisting = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Click to use as copy source\n%s\nExp: %.0fμs, Gain: %.1f",
            settings.description.c_str(), settings.exposure_time, settings.gain);
        }
      }
      ImGui::EndChild();
      ImGui::TextDisabled("Click any existing node to use as copy source");
    }
  }





  // Node settings display and editing
  if (ImGui::CollapsingHeader("Node Settings")) {
    // Show existing node settings
    for (auto& [nodeId, settings] : m_nodeSettings) {
      if (ImGui::TreeNode(nodeId.c_str())) {
        ImGui::Text("Description: %s", settings.description.c_str());

        // Edit controls
        float expTime = static_cast<float>(settings.exposure_time);
        if (ImGui::SliderFloat("Exposure Time (us)", &expTime, 1.0f, 50000.0f, "%.0f")) {
          settings.exposure_time = expTime;
          UpdateModificationTime();
        }

        float gainVal = static_cast<float>(settings.gain);
        if (ImGui::SliderFloat("Gain (dB)", &gainVal, 0.0f, 20.0f, "%.1f")) {
          settings.gain = gainVal;
          UpdateModificationTime();
        }

        if (ImGui::Checkbox(("Exposure Auto##" + nodeId).c_str(), &settings.exposure_auto)) {
          UpdateModificationTime();
        }
        if (ImGui::Checkbox(("Gain Auto##" + nodeId).c_str(), &settings.gain_auto)) {
          UpdateModificationTime();
        }

        ImGui::TreePop();
      }
    }
  }

  ImGui::Spacing();

  // File operations
  if (ImGui::Button("Save Configuration")) {
    SaveConfiguration();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload Configuration")) {
    LoadConfiguration(m_configPath);
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh UI")) {
    std::cout << "Refreshing Camera Exposure Manager UI..." << std::endl;
  }

  ImGui::Separator();
  ImGui::Text("Current Configuration Summary:");
  ImGui::Text("Loaded nodes: %zu", m_nodeSettings.size());
  if (ImGui::BeginChild("ConfigSummary", ImVec2(0, 100), true)) {
    for (const auto& [nodeId, settings] : m_nodeSettings) {
      ImGui::Text("%s: %.0fms, gain %.1f",
        nodeId.c_str(),
        settings.exposure_time / 1000.0,
        settings.gain);
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::Text("Performance Info:");
  ImGui::Text("Data structure: unordered_map (O(1) lookups)");

  auto now = std::chrono::steady_clock::now();
  auto timeSinceModified = std::chrono::duration_cast<std::chrono::seconds>(
    now - m_lastModified.load());
  ImGui::Text("Last modified: %lld seconds ago", static_cast<long long>(timeSinceModified.count()));

  ImGui::End();
}

CameraExposureSettings CameraExposureManager::ReadCurrentCameraSettings(PylonCamera& camera) const {
  CameraExposureSettings currentSettings;

  if (!camera.IsConnected()) {
    std::cout << "Camera not connected - cannot read settings" << std::endl;
    return currentSettings;
  }

  try {
    auto& internalCamera = camera.GetInternalCamera();

    if (!internalCamera.IsOpen()) {
      std::cout << "Camera not open - cannot read settings" << std::endl;
      return currentSettings;
    }

    std::cout << "=== READING CURRENT CAMERA SETTINGS ===" << std::endl;

    // Read current exposure settings (Basler-specific)
    bool exposureFound = false;
    try {
      // Check exposure mode
      if (internalCamera.GetNodeMap().GetNode("ExposureMode")) {
        Pylon::CEnumParameter exposureMode(internalCamera.GetNodeMap(), "ExposureMode");
        if (exposureMode.IsReadable()) {
          std::string modeValue = exposureMode.GetValue();
          std::cout << "Current exposure mode: " << modeValue << std::endl;
        }
      }

      // Read ExposureTimeAbs value
      if (internalCamera.GetNodeMap().GetNode("ExposureTimeAbs")) {
        Pylon::CFloatParameter exposureTime(internalCamera.GetNodeMap(), "ExposureTimeAbs");
        if (exposureTime.IsReadable()) {
          currentSettings.exposure_time = exposureTime.GetValue();
          double minExposure = exposureTime.GetMin();
          double maxExposure = exposureTime.GetMax();

          std::cout << "Current exposure time: " << currentSettings.exposure_time << " us"
            << " (range: " << minExposure << "-" << maxExposure << " us)" << std::endl;
          exposureFound = true;
        }
      }

      // Also check ExposureTimeRaw if ExposureTimeAbs not found
      if (!exposureFound && internalCamera.GetNodeMap().GetNode("ExposureTimeRaw")) {
        Pylon::CIntegerParameter exposureTimeRaw(internalCamera.GetNodeMap(), "ExposureTimeRaw");
        if (exposureTimeRaw.IsReadable()) {
          int64_t rawValue = exposureTimeRaw.GetValue();
          std::cout << "Current exposure time raw: " << rawValue << std::endl;

          // For raw values, we'd need to know the timebase to convert to microseconds
          // For now, just use the raw value as an approximation
          currentSettings.exposure_time = static_cast<double>(rawValue);
          exposureFound = true;
        }
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cout << "Could not read exposure settings: " << e.GetDescription() << std::endl;
    }

    if (!exposureFound) {
      std::cout << "WARNING: Could not find or read any exposure time parameter!" << std::endl;
    }

    // Read current gain (Basler-specific)
    bool gainFound = false;
    try {
      // First check what GainSelector is set to
      if (internalCamera.GetNodeMap().GetNode("GainSelector")) {
        Pylon::CEnumParameter gainSelector(internalCamera.GetNodeMap(), "GainSelector");
        if (gainSelector.IsReadable()) {
          std::string selectorValue = gainSelector.GetValue();
          std::cout << "Current gain selector: " << selectorValue << std::endl;
        }
      }

      // Read GainRaw value
      if (internalCamera.GetNodeMap().GetNode("GainRaw")) {
        Pylon::CIntegerParameter gainRaw(internalCamera.GetNodeMap(), "GainRaw");
        if (gainRaw.IsReadable()) {
          int64_t rawValue = gainRaw.GetValue();
          int64_t minGain = gainRaw.GetMin();
          int64_t maxGain = gainRaw.GetMax();

          // Convert raw value back to our 0-10 scale
          double gainRatio = static_cast<double>(rawValue - minGain) / (maxGain - minGain);
          currentSettings.gain = gainRatio * 10.0;

          std::cout << "Current gain raw: " << rawValue << " (== " << currentSettings.gain
            << " on 0-10 scale, range: " << minGain << "-" << maxGain << ")" << std::endl;
          gainFound = true;
        }
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cout << "Could not read gain: " << e.GetDescription() << std::endl;
    }

    if (!gainFound) {
      std::cout << "WARNING: Could not find or read any gain parameter!" << std::endl;
    }

    // Read auto modes
    try {
      if (internalCamera.GetNodeMap().GetNode("ExposureAuto")) {
        Pylon::CEnumParameter exposureAuto(internalCamera.GetNodeMap(), "ExposureAuto");
        if (exposureAuto.IsReadable()) {
          std::string autoMode = exposureAuto.GetValue();
          currentSettings.exposure_auto = (autoMode == "Continuous" || autoMode == "Once");
          std::cout << "Current exposure auto: " << autoMode << std::endl;
        }
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cout << "Could not read exposure auto: " << e.GetDescription() << std::endl;
    }

    try {
      if (internalCamera.GetNodeMap().GetNode("GainAuto")) {
        Pylon::CEnumParameter gainAuto(internalCamera.GetNodeMap(), "GainAuto");
        if (gainAuto.IsReadable()) {
          std::string autoMode = gainAuto.GetValue();
          currentSettings.gain_auto = (autoMode == "Continuous" || autoMode == "Once");
          std::cout << "Current gain auto: " << autoMode << std::endl;
        }
      }
    }
    catch (const Pylon::GenericException& e) {
      std::cout << "Could not read gain auto: " << e.GetDescription() << std::endl;
    }

    // List some key parameters to help debug
    std::cout << "\n=== CHECKING KEY CAMERA PARAMETERS ===" << std::endl;
    try {
      // Check specific parameters we're interested in
      std::vector<std::string> paramsToCheck = {
          "ExposureMode", "ExposureTimeAbs", "ExposureTimeRaw", "ExposureAuto",
          "GainSelector", "GainRaw", "GainAuto"
      };

      for (const auto& paramName : paramsToCheck) {
        try {
          if (internalCamera.GetNodeMap().GetNode(paramName.c_str())) {
            std::cout << "  [Yes] " << paramName << " - Available" << std::endl;
          }
          else {
            std::cout << "  [No] " << paramName << " - Not available" << std::endl;
          }
        }
        catch (const Pylon::GenericException& e) {
          std::cout << "  ? " << paramName << " - Error: " << e.GetDescription() << std::endl;
        }
      }
    }
    catch (...) {
      std::cout << "Could not check camera parameters" << std::endl;
    }

    std::cout << "===============================================" << std::endl;

  }
  catch (const Pylon::GenericException& e) {
    std::cout << "Error reading camera settings: " << e.GetDescription() << std::endl;
  }

  return currentSettings;
}

bool CameraExposureManager::VerifySettingsApplied(PylonCamera& camera, const std::string& nodeId) const {
  std::cout << "\n=== VERIFYING CAMERA SETTINGS FOR NODE " << nodeId << " ===" << std::endl;

  // Get expected settings
  CameraExposureSettings expectedSettings = GetSettingsForNode(nodeId);
  std::cout << "Expected settings:" << std::endl;
  std::cout << "  - Exposure Time: " << expectedSettings.exposure_time << " us" << std::endl;
  std::cout << "  - Gain: " << expectedSettings.gain << " dB" << std::endl;
  std::cout << "  - Exposure Auto: " << (expectedSettings.exposure_auto ? "On" : "Off") << std::endl;
  std::cout << "  - Gain Auto: " << (expectedSettings.gain_auto ? "On" : "Off") << std::endl;

  // Get actual settings from camera
  CameraExposureSettings actualSettings = ReadCurrentCameraSettings(camera);

  // Compare settings
  bool exposureMatch = std::abs(actualSettings.exposure_time - expectedSettings.exposure_time) < 100; // Within 100us
  bool gainMatch = std::abs(actualSettings.gain - expectedSettings.gain) < 0.1; // Within 0.1dB
  bool exposureAutoMatch = (actualSettings.exposure_auto == expectedSettings.exposure_auto);
  bool gainAutoMatch = (actualSettings.gain_auto == expectedSettings.gain_auto);

  std::cout << "\nComparison results:" << std::endl;
  std::cout << "  - Exposure Time: " << (exposureMatch ? "[Yes] MATCH" : "[No] MISMATCH") << std::endl;
  std::cout << "  - Gain: " << (gainMatch ? "[Yes] MATCH" : "[No] MISMATCH") << std::endl;
  std::cout << "  - Exposure Auto: " << (exposureAutoMatch ? "[Yes] MATCH" : "[No] MISMATCH") << std::endl;
  std::cout << "  - Gain Auto: " << (gainAutoMatch ? "[Yes] MATCH" : "[No] MISMATCH") << std::endl;

  bool allMatch = exposureMatch && gainMatch && exposureAutoMatch && gainAutoMatch;
  std::cout << "\nOverall result: " << (allMatch ? "[Yes] ALL SETTINGS APPLIED CORRECTLY" : "[No] SETTINGS MISMATCH") << std::endl;
  std::cout << "============================================\n" << std::endl;

  return allMatch;
}

void CameraExposureManager::ShowCameraStatus(PylonCamera& camera) const {
  if (!camera.IsConnected()) {
    std::cout << "Camera not connected" << std::endl;
    return;
  }

  try {
    auto& internalCamera = camera.GetInternalCamera();

    if (!internalCamera.IsOpen()) {
      std::cout << "Camera not open" << std::endl;
      return;
    }

    std::cout << "\n=== COMPLETE BASLER CAMERA STATUS ===" << std::endl;

    // Camera model info
    std::cout << "Camera Model: " << camera.GetDeviceInfo() << std::endl;

    // Exposure settings
    std::cout << "\n--- EXPOSURE SETTINGS ---" << std::endl;
    try {
      if (internalCamera.GetNodeMap().GetNode("ExposureMode")) {
        Pylon::CEnumParameter exposureMode(internalCamera.GetNodeMap(), "ExposureMode");
        std::cout << "Exposure Mode: " << exposureMode.GetValue() << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("ExposureAuto")) {
        Pylon::CEnumParameter exposureAuto(internalCamera.GetNodeMap(), "ExposureAuto");
        std::cout << "Exposure Auto: " << exposureAuto.GetValue() << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("ExposureTimeAbs")) {
        Pylon::CFloatParameter exposureTime(internalCamera.GetNodeMap(), "ExposureTimeAbs");
        std::cout << "Exposure Time: " << exposureTime.GetValue() << " us"
          << " (range: " << exposureTime.GetMin() << "-" << exposureTime.GetMax() << ")" << std::endl;
      }
    }
    catch (...) {
      std::cout << "Error reading exposure settings" << std::endl;
    }

    // Gain settings
    std::cout << "\n--- GAIN SETTINGS ---" << std::endl;
    try {
      if (internalCamera.GetNodeMap().GetNode("GainSelector")) {
        Pylon::CEnumParameter gainSelector(internalCamera.GetNodeMap(), "GainSelector");
        std::cout << "Gain Selector: " << gainSelector.GetValue() << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("GainAuto")) {
        Pylon::CEnumParameter gainAuto(internalCamera.GetNodeMap(), "GainAuto");
        std::cout << "Gain Auto: " << gainAuto.GetValue() << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("GainRaw")) {
        Pylon::CIntegerParameter gainRaw(internalCamera.GetNodeMap(), "GainRaw");
        std::cout << "Gain Raw: " << gainRaw.GetValue()
          << " (range: " << gainRaw.GetMin() << "-" << gainRaw.GetMax() << ")" << std::endl;
      }
    }
    catch (...) {
      std::cout << "Error reading gain settings" << std::endl;
    }

    // Image format
    std::cout << "\n--- IMAGE FORMAT ---" << std::endl;
    try {
      if (internalCamera.GetNodeMap().GetNode("Width")) {
        Pylon::CIntegerParameter width(internalCamera.GetNodeMap(), "Width");
        std::cout << "Image Width: " << width.GetValue() << " pixels" << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("Height")) {
        Pylon::CIntegerParameter height(internalCamera.GetNodeMap(), "Height");
        std::cout << "Image Height: " << height.GetValue() << " pixels" << std::endl;
      }

      if (internalCamera.GetNodeMap().GetNode("PixelFormat")) {
        Pylon::CEnumParameter pixelFormat(internalCamera.GetNodeMap(), "PixelFormat");
        std::cout << "Pixel Format: " << pixelFormat.GetValue() << std::endl;
      }
    }
    catch (...) {
      std::cout << "Error reading image format" << std::endl;
    }

    std::cout << "==========================================\n" << std::endl;
  }
  catch (const Pylon::GenericException& e) {
    std::cout << "Error reading camera status: " << e.GetDescription() << std::endl;
  }
}