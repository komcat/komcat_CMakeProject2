// machine_operations.h - FIXED VERSION
#pragma once

#include "include/motions/motion_control_layer.h"
#include "include/motions/pi_controller_manager.h"
#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "include/data/global_data_store.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/logger.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/CameraExposureManager.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <map>
#include <mutex>

// Forward declare CLD101xOperations instead of including the header
class CLD101xOperations;

class MachineOperations {
public:
	MachineOperations(
		MotionControlLayer& motionLayer,
		PIControllerManager& piControllerManager,
		EziIOManager& ioManager,
		PneumaticManager& pneumaticManager,
		CLD101xOperations* laserOps = nullptr,
		PylonCameraTest* cameraTest = nullptr
	);

	~MachineOperations();

	// Motion control methods
	bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
		const std::string& targetNodeId, bool blocking = true);

	bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
		const std::string& startNodeId, const std::string& endNodeId,
		bool blocking = true);

	bool MoveToPointName(const std::string& deviceName, const std::string& positionName, bool blocking = true);
	bool MoveRelative(const std::string& deviceName, const std::string& axis,
		double distance, bool blocking = true);

	// IO control methods
	bool SetOutput(const std::string& deviceName, int outputPin, bool state);
	bool SetOutput(int deviceId, int outputPin, bool state);
	bool ReadInput(const std::string& deviceName, int inputPin, bool& state);
	bool ReadInput(int deviceId, int inputPin, bool& state);
	bool ClearLatch(const std::string& deviceName, int inputPin);
	bool ClearLatch(int deviceId, uint32_t latchMask);

	// Pneumatic control methods
	bool ExtendSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000);
	bool RetractSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000);
	SlideState GetSlideState(const std::string& slideName);
	bool WaitForSlideState(const std::string& slideName, SlideState targetState, int timeoutMs = 5000);

	// Utility methods
	void Wait(int milliseconds);
	float ReadDataValue(const std::string& dataId, float defaultValue = 0.0f);
	bool HasDataValue(const std::string& dataId);

	// Scanning methods
	bool PerformScan(const std::string& deviceName, const std::string& dataChannel,
		const std::vector<double>& stepSizes, int settlingTimeMs,
		const std::vector<std::string>& axesToScan = { "Z", "X", "Y" });

	bool StartScan(const std::string& deviceName, const std::string& dataChannel,
		const std::vector<double>& stepSizes, int settlingTimeMs,
		const std::vector<std::string>& axesToScan = { "Z", "X", "Y" });

	bool StopScan(const std::string& deviceName);

	// Get scan status
	bool IsScanActive(const std::string& deviceName) const;
	double GetScanProgress(const std::string& deviceName) const;
	std::string GetScanStatus(const std::string& deviceName) const;

	// Get scan results
	bool GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const;

	// Device status methods
	bool IsDeviceConnected(const std::string& deviceName);
	bool IsSlideExtended(const std::string& slideName);
	bool IsSlideRetracted(const std::string& slideName);
	bool IsSlideMoving(const std::string& slideName);
	bool IsSlideInError(const std::string& slideName);

	// Helper method to get EziIO device ID from name
	int GetDeviceId(const std::string& deviceName);

	// Laser and TEC control methods
	bool LaserOn(const std::string& laserName = "");
	bool LaserOff(const std::string& laserName = "");
	bool TECOn(const std::string& laserName = "");
	bool TECOff(const std::string& laserName = "");
	bool SetLaserCurrent(float current, const std::string& laserName = "");
	bool SetTECTemperature(float temperature, const std::string& laserName = "");
	float GetLaserTemperature(const std::string& laserName = "");
	float GetLaserCurrent(const std::string& laserName = "");
	bool WaitForLaserTemperature(float targetTemp, float tolerance = 0.5f,
		int timeoutMs = 30000, const std::string& laserName = "");

	// Motion and device status methods
	bool SafelyCleanupScanner(const std::string& deviceName);
	bool WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs);
	bool IsDeviceMoving(const std::string& deviceName);
	bool IsDevicePIController(const std::string& deviceName) const;

	// Public logging methods for operation classes to use
	void LogInfo(const std::string& message) const {
		if (m_logger) {
			m_logger->LogInfo("MachineOperations: " + message);
		}
	}

	void LogWarning(const std::string& message) const {
		if (m_logger) {
			m_logger->LogWarning("MachineOperations: " + message);
		}
	}

	void LogError(const std::string& message) const {
		if (m_logger) {
			m_logger->LogError("MachineOperations: " + message);
		}
	}

	// Camera control methods
	bool InitializeCamera();
	bool ConnectCamera();
	bool DisconnectCamera();
	bool StartCameraGrabbing();
	bool StopCameraGrabbing();
	bool IsCameraInitialized() const;
	bool IsCameraConnected() const;
	bool IsCameraGrabbing() const;

	// Camera capture methods
	bool CaptureImageToFile(const std::string& filename = "");
	bool UpdateCameraDisplay();
	bool IntegrateCameraWithMotion(PylonCameraTest* cameraTest);

	// Get current position information
	std::string GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName);
	std::string GetDeviceCurrentPositionName(const std::string& deviceName);
	bool GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position);

	// FIXED: Only ONE declaration of GetDistanceBetweenPositions, marked as const
	double GetDistanceBetweenPositions(const PositionStruct& pos1, const PositionStruct& pos2,
		bool includeRotation = false) const;

	// Scanner cleanup methods
	bool CleanupAllScanners();
	bool ResetScanState(const std::string& deviceName);

	// Camera exposure control methods
	bool ApplyCameraExposureForNode(const std::string& nodeId);
	bool ApplyDefaultCameraExposure();
	CameraExposureManager* GetCameraExposureManager() { return m_cameraExposureManager.get(); }

	// Enable/disable automatic camera exposure adjustment on gantry moves
	void SetAutoExposureEnabled(bool enabled) { m_autoExposureEnabled = enabled; }
	bool IsAutoExposureEnabled() const { return m_autoExposureEnabled; }

	// Test method to verify current camera settings
	void TestCameraSettings(const std::string& nodeId = "") {
		if (!m_cameraTest || !m_cameraExposureManager) {
			std::cout << "Camera or exposure manager not available for testing" << std::endl;
			return;
		}

		if (!m_cameraTest->GetCamera().IsConnected()) {
			std::cout << "Camera not connected for testing" << std::endl;
			return;
		}

		if (nodeId.empty()) {
			std::cout << "\n=== READING CURRENT CAMERA SETTINGS ===" << std::endl;
			m_cameraExposureManager->ReadCurrentCameraSettings(m_cameraTest->GetCamera());
		}
		else {
			std::cout << "\n=== TESTING CAMERA SETTINGS FOR NODE " << nodeId << " ===" << std::endl;
			ApplyCameraExposureForNode(nodeId);
		}
	}

	// NEW: Temporary Position Storage for Process Sequences
	// ====================================================

	// Capture and store current position with a global label
	bool CaptureCurrentPosition(const std::string& deviceName, const std::string& label);

	// Retrieve stored position by label
	bool GetStoredPosition(const std::string& label, PositionStruct& position) const;

	// Get all stored position labels (optionally filtered by device)
	std::vector<std::string> GetStoredPositionLabels(const std::string& deviceName = "") const;

	// FIXED: Calculate distance from current position to stored position (NOT const)
	double CalculateDistanceFromStored(const std::string& deviceName, const std::string& storedLabel);

	// FIXED: Check if device has moved significantly from stored position (NOT const)
	bool HasMovedFromStored(const std::string& deviceName, const std::string& storedLabel,
		double tolerance = 0.001);

	// Clear stored positions (all or by device filter)
	void ClearStoredPositions(const std::string& deviceNameFilter = "");

	// Clear positions older than specified minutes
	void ClearOldStoredPositions(int maxAgeMinutes = 60);

	// Get information about stored position (device name, timestamp, etc.)
	bool GetStoredPositionInfo(const std::string& label, std::string& deviceName,
		std::chrono::steady_clock::time_point& timestamp) const;



	// Position monitoring and storage methods
	std::map<std::string, PositionStruct> GetCurrentPositions();
	bool UpdateAllCurrentPositions();
	const std::map<std::string, PositionStruct>& GetCachedPositions() const;
	void RefreshPositionCache();

	// Configuration management methods for saving positions
	bool SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName);
	bool UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName);
	bool SaveAllCurrentPositionsToConfig(const std::string& prefix = "current_");

	// Backup and restore config operations
	bool BackupMotionConfig(const std::string& backupSuffix = "");
	bool RestoreMotionConfigFromBackup(const std::string& backupSuffix);

private:
	MotionControlLayer& m_motionLayer;
	PIControllerManager& m_piControllerManager;
	EziIOManager& m_ioManager;
	PneumaticManager& m_pneumaticManager;
	Logger* m_logger;
	CLD101xOperations* m_laserOps;




	// Helper methods
	bool ConvertPinStateToBoolean(uint32_t inputs, int pin);

	std::map<std::string, std::unique_ptr<ScanningAlgorithm>> m_activeScans;
	std::mutex m_scanMutex;

	// Scan status information (tracked per device)
	struct ScanInfo {
		std::atomic<bool> isActive{ false };
		std::atomic<double> progress{ 0.0 };
		std::string status;
		mutable std::mutex statusMutex;
		double peakValue{ 0.0 };
		PositionStruct peakPosition;
		mutable std::mutex peakMutex;
	};
	std::map<std::string, ScanInfo> m_scanInfo;

	// Add camera reference
	PylonCameraTest* m_cameraTest;

	// Camera exposure management
	std::unique_ptr<CameraExposureManager> m_cameraExposureManager;
	bool m_autoExposureEnabled = true;

	// NEW: Temporary position storage for process calculations
	struct StoredPositionInfo {
		std::string deviceName;
		PositionStruct position;
		std::chrono::steady_clock::time_point timestamp;

		StoredPositionInfo() = default;
		StoredPositionInfo(const std::string& device, const PositionStruct& pos)
			: deviceName(device), position(pos), timestamp(std::chrono::steady_clock::now()) {
		}
	};

	std::map<std::string, StoredPositionInfo> m_storedPositions;
	mutable std::mutex m_positionStorageMutex;




	// Current position cache for all controllers
	std::map<std::string, PositionStruct> m_currentPositions;
	mutable std::mutex m_currentPositionsMutex;
	std::chrono::steady_clock::time_point m_lastPositionUpdate;
	static constexpr std::chrono::milliseconds POSITION_CACHE_TIMEOUT{ 100 }; // 100ms cache timeout
};