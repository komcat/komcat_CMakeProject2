#pragma once

#include "pylon_camera.h"
#include <string>
#include <unordered_map>  // Changed from std::map for O(1) lookups instead of O(log n)
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include "nlohmann/json.hpp"

// Structure to hold camera exposure settings for a specific node
struct CameraExposureSettings {
	double exposure_time;      // Exposure time in microseconds
	double gain;              // Gain value in dB
	bool exposure_auto;       // Auto exposure enabled/disabled
	bool gain_auto;          // Auto gain enabled/disabled
	std::string description; // Human readable description

	CameraExposureSettings()
		: exposure_time(10000.0), gain(0.0), exposure_auto(false), gain_auto(false), description("") {
	}

	CameraExposureSettings(double exp_time, double gain_val, bool exp_auto, bool gain_auto_val, const std::string& desc = "")
		: exposure_time(exp_time), gain(gain_val), exposure_auto(exp_auto), gain_auto(gain_auto_val), description(desc) {
	}
};

// Manager class to handle camera exposure settings based on gantry positions
class CameraExposureManager {
public:
	// Constructor - loads configuration from JSON file
	CameraExposureManager(const std::string& configPath = "camera_exposure_config.json");

	// Destructor
	~CameraExposureManager();

	// Load configuration from JSON file
	bool LoadConfiguration(const std::string& configPath);

	// Save current configuration to JSON file
	bool SaveConfiguration(const std::string& configPath = "");

	// Apply exposure settings for a specific node
	bool ApplySettingsForNode(PylonCamera& camera, const std::string& nodeId);

	// Apply default exposure settings
	bool ApplyDefaultSettings(PylonCamera& camera);

	// OPTIMIZED: Get exposure settings for a specific node (returns const reference)
	const CameraExposureSettings& GetSettingsForNodeRef(const std::string& nodeId) const;

	// LEGACY: Get exposure settings for a specific node (returns copy - slower)
	CameraExposureSettings GetSettingsForNode(const std::string& nodeId) const;

	// OPTIMIZED: Batch get settings for multiple nodes (eliminates repeated lookups)
	std::vector<std::pair<std::string, const CameraExposureSettings*>>
		GetAllSettingsForNodes(const std::vector<std::string>& nodeIds) const;

	// OPTIMIZED: Check and get in one operation (eliminates double lookup)
	std::pair<bool, const CameraExposureSettings&>
		TryGetSettingsForNode(const std::string& nodeId) const;

	// Set exposure settings for a specific node
	void SetSettingsForNode(const std::string& nodeId, const CameraExposureSettings& settings);

	// Get default settings (const reference)
	const CameraExposureSettings& GetDefaultSettings() const { return m_defaultSettings; }

	// Set default settings
	void SetDefaultSettings(const CameraExposureSettings& settings) {
		m_defaultSettings = settings;
		UpdateModificationTime();
	}

	// Get all node settings (const reference)
	const std::unordered_map<std::string, CameraExposureSettings>& GetAllNodeSettings() const {
		return m_nodeSettings;
	}

	// OPTIMIZED: Check if settings exist for a node (uses cached result when possible)
	bool HasSettingsForNode(const std::string& nodeId) const;

	// Set callback for when settings are applied
	void SetSettingsAppliedCallback(std::function<void(const std::string&, const CameraExposureSettings&)> callback);

	// OPTIMIZED: Get modification timestamp to check if data changed
	std::chrono::steady_clock::time_point GetLastModified() const {
		return m_lastModified.load();
	}

	// Render ImGui interface for editing settings
	void RenderUI();

	// Enable/disable the UI
	void ToggleWindow() { m_showUI = !m_showUI; }
	bool IsVisible() const { return m_showUI; }

	// Verify what settings are actually applied to the camera
	CameraExposureSettings ReadCurrentCameraSettings(PylonCamera& camera) const;

	// Compare expected vs actual settings
	bool VerifySettingsApplied(PylonCamera& camera, const std::string& nodeId) const;

	// Show complete camera status
	void ShowCameraStatus(PylonCamera& camera) const;


	// Set a camera instance for testing settings
	void SetTestCamera(PylonCamera* camera) { m_testCamera = camera; }

	// Get the test camera instance
	PylonCamera* GetTestCamera() const { return m_testCamera; }

private:
	// Apply the actual camera settings
	bool ApplyCameraSettings(PylonCamera& camera, const CameraExposureSettings& settings);

	// Parse JSON configuration
	bool ParseConfiguration(const nlohmann::json& config);

	// Convert settings to JSON
	nlohmann::json SettingsToJson(const CameraExposureSettings& settings) const;

	// Convert JSON to settings
	CameraExposureSettings JsonToSettings(const nlohmann::json& json) const;

	// Update modification timestamp
	void UpdateModificationTime() {
		m_lastModified.store(std::chrono::steady_clock::now());
	}

	std::string m_configPath;
	CameraExposureSettings m_defaultSettings;

	// OPTIMIZED: Use unordered_map for O(1) lookups instead of O(log n)
	std::unordered_map<std::string, CameraExposureSettings> m_nodeSettings;

	// OPTIMIZED: Track modification time for efficient change detection
	std::atomic<std::chrono::steady_clock::time_point> m_lastModified;

	// Callback for when settings are applied
	std::function<void(const std::string&, const CameraExposureSettings&)> m_settingsAppliedCallback;

	// UI state
	bool m_showUI;
	std::string m_selectedNode;
	CameraExposureSettings m_editingSettings;
	bool m_editingDefault;

	// Status tracking
	std::string m_lastAppliedNode;
	CameraExposureSettings m_lastAppliedSettings;
	bool m_settingsApplySuccess;
	std::string m_lastErrorMessage;

	// Thread safety for modification timestamp
	mutable std::mutex m_accessMutex;

	std::string m_currentAppliedNodeId;
	CameraExposureSettings m_currentAppliedSettings;

	// Camera instance for testing settings
	PylonCamera* m_testCamera = nullptr;
};