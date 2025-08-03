#pragma once

#include "pylon_camera_test.h"
#include "CameraExposureManager.h"
#include "CameraFrameData.h"
#include "ICameraHardware.h"
#include "CameraHardwareFactory.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <set>
#include <mutex>

// Forward declaration
class CameraFrameSubscriber;

// Enhanced structure to identify cameras
struct  CameraInfo {
  std::string id;           // Camera identifier (e.g., "camera_1", "camera_2") 
  std::string serialNumber; // Camera serial number (if connecting by serial)
  std::string ipAddress;    // Camera IP address (for GigE cameras)
  std::string description;  // Human readable description
  bool autoConnect;         // Whether to auto-connect this camera

  // Connection method preference
  enum class ConnectionMethod {
    AUTO,           // Connect to first available
    SERIAL_NUMBER,  // Connect by serial number
    IP_ADDRESS,     // Connect by IP address (GigE cameras)
    DEVICE_INDEX    // Connect by device index
  };

  ConnectionMethod connectionMethod;
  int deviceIndex = 0;  // For device index connection

  // EXISTING: Constructor for auto/description only (backward compatibility)
  CameraInfo(const std::string& cameraId, const std::string& desc = "", bool autoConn = true)
    : id(cameraId), description(desc), autoConnect(autoConn), connectionMethod(ConnectionMethod::AUTO) {
  }

  // EXISTING: Constructor for serial number connection (backward compatibility)
  CameraInfo(const std::string& cameraId, const std::string& serial, const std::string& desc, bool autoConn = true)
    : id(cameraId), serialNumber(serial), description(desc), autoConnect(autoConn),
    connectionMethod(ConnectionMethod::SERIAL_NUMBER) {
  }

  // NEW: Static factory method for IP-based connection (GigE cameras)
  static CameraInfo CreateByIP(const std::string& cameraId, const std::string& ip,
    const std::string& desc = "", bool autoConn = true) {
    CameraInfo info;
    info.id = cameraId;
    info.ipAddress = ip;
    info.description = desc;
    info.autoConnect = autoConn;
    info.connectionMethod = ConnectionMethod::IP_ADDRESS;
    return info;
  }

  // NEW: Static factory method for device index connection
  static CameraInfo CreateByIndex(const std::string& cameraId, int index,
    const std::string& desc = "", bool autoConn = true) {
    CameraInfo info;
    info.id = cameraId;
    info.deviceIndex = index;
    info.description = desc;
    info.autoConnect = autoConn;
    info.connectionMethod = ConnectionMethod::DEVICE_INDEX;
    return info;
  }

  // Helper methods for checking connection type
  bool IsIPConnection() const { return connectionMethod == ConnectionMethod::IP_ADDRESS; }
  bool IsSerialConnection() const { return connectionMethod == ConnectionMethod::SERIAL_NUMBER; }
  bool IsIndexConnection() const { return connectionMethod == ConnectionMethod::DEVICE_INDEX; }
  bool IsAutoConnection() const { return connectionMethod == ConnectionMethod::AUTO; }

  // Get connection info string for debugging and UI display
  std::string GetConnectionInfo() const {
    switch (connectionMethod) {
    case ConnectionMethod::IP_ADDRESS:
      return "IP: " + ipAddress;
    case ConnectionMethod::SERIAL_NUMBER:
      return "Serial: " + serialNumber;
    case ConnectionMethod::DEVICE_INDEX:
      return "Index: " + std::to_string(deviceIndex);
    default:
      return "Auto";
    }
  }

private:
  // Private default constructor for factory methods
  CameraInfo() : autoConnect(true), connectionMethod(ConnectionMethod::AUTO), deviceIndex(0) {}
};

// Enhanced camera info with type support
struct ExtendedCameraInfo : public CameraInfo {
  ICameraHardware::CameraType type = ICameraHardware::CameraType::UNKNOWN;
  std::string deviceInfo;  // Additional device-specific info

  ExtendedCameraInfo() = default;
  ExtendedCameraInfo(const CameraInfo& baseInfo, ICameraHardware::CameraType camType, const std::string& devInfo = "")
    : CameraInfo(baseInfo), type(camType), deviceInfo(devInfo) {
  }
};

// Multi-camera manager supporting both Pylon and IDS cameras via interface
class CameraManager {
public:
  CameraManager();
  ~CameraManager();

  // ============================================================================
  // CAMERA MANAGEMENT - LEGACY METHODS (for backward compatibility)
  // ============================================================================

  // Add a camera to be managed (creates new PylonCameraTest instance)
  bool AddCamera(const CameraInfo& cameraInfo);

  // Remove a camera by ID
  bool RemoveCamera(const std::string& cameraId);

  // Get camera by ID (returns PylonCameraTest* for backward compatibility)
  PylonCameraTest* GetCamera(const std::string& cameraId);

  // Get all camera IDs
  std::vector<std::string> GetCameraIds() const;

  // Get camera count
  size_t GetCameraCount() const { return m_cameras.size(); }

  // ============================================================================
  // CAMERA MANAGEMENT - NEW INTERFACE METHODS
  // ============================================================================

  // Enhanced camera management with type support
  bool AddCamera(const ExtendedCameraInfo& cameraInfo);
  bool AddPylonCamera(const CameraInfo& cameraInfo);
  bool AddIDSCamera(const CameraInfo& cameraInfo, int deviceId = 0);
  bool AddCameraAutoDetect(const CameraInfo& cameraInfo, const std::string& deviceInfo);

  // Get camera by interface
  ICameraHardware* GetCameraHardware(const std::string& cameraId);

  // Get cameras by type
  std::vector<std::string> GetCameraIdsByType(ICameraHardware::CameraType type) const;

  // ============================================================================
  // CAMERA OPERATIONS
  // ============================================================================

  // Initialize and connect all cameras
  bool InitializeAllCameras();

  // Connect specific camera by ID
  bool ConnectCamera(const std::string& cameraId);

  // Disconnect specific camera by ID
  bool DisconnectCamera(const std::string& cameraId);

  // Start grabbing on all connected cameras
  bool StartGrabbingAll();

  // Stop grabbing on all cameras
  bool StopGrabbingAll();

  // Start grabbing on specific camera
  bool StartGrabbing(const std::string& cameraId);

  // Stop grabbing on specific camera
  bool StopGrabbing(const std::string& cameraId);

  // Enhanced StartGrabbing with broadcasting
  bool StartGrabbingWithBroadcast(const std::string& cameraId);

  // ============================================================================
  // EXPOSURE CONTROL
  // ============================================================================

  // Apply exposure settings to specific camera using node ID (Pylon legacy)
  bool ApplyExposureForNode(const std::string& cameraId, const std::string& nodeId);

  // Apply exposure settings to all cameras using node ID (Pylon legacy)
  bool ApplyExposureForNodeAll(const std::string& nodeId);

  // Apply direct exposure settings to specific camera (Legacy Pylon)
  bool ApplyExposureSettings(const std::string& cameraId, const PylonCamera::ExposureSettings& settings);

  // Apply direct exposure settings via interface (New)
  bool ApplyExposureSettings(const std::string& cameraId, const ICameraHardware::ExposureSettings& settings);

  // ============================================================================
  // IMAGE CAPTURE
  // ============================================================================

  // Capture image from specific camera
  bool CaptureImage(const std::string& cameraId);

  // Capture images from all cameras (synchronized)
  bool CaptureImageAll();

  // ============================================================================
  // CAMERA STATUS AND CONFIGURATION
  // ============================================================================

  // Enhanced camera status
  struct CameraStatus {
    std::string id;
    ICameraHardware::CameraType type;
    bool connected;
    bool grabbing;
    bool deviceRemoved;
    std::string deviceInfo;
    std::string modelName;
    std::string serialNumber;
    ICameraHardware::ExposureSettings currentExposure;
  };

  CameraStatus GetCameraStatus(const std::string& cameraId) const;
  std::vector<CameraStatus> GetAllCameraStatus() const;

  // Camera configuration
  bool SetCameraConfiguration(const std::string& cameraId, const std::string& key, const std::string& value);
  std::string GetCameraConfiguration(const std::string& cameraId, const std::string& key) const;

  // ============================================================================
  // BROADCASTING SYSTEM
  // ============================================================================

  void StartBroadcastSystem();
  void StopBroadcastSystem();
  void SubscribeToFrames(std::shared_ptr<CameraFrameSubscriber> subscriber);
  void UnsubscribeFromFrames(const std::string& subscriberId);
  void SetGlobalBroadcastRate(float fps);
  size_t GetSubscriberCount() const;
  std::vector<std::string> GetSubscriberIds() const;

  // ============================================================================
  // UI RENDERING
  // ============================================================================

  // UI rendering for all cameras
  void RenderUI();

  // Toggle visibility of camera manager UI
  void ToggleWindow() { m_showUI = !m_showUI; }
  bool IsVisible() const { return m_showUI; }

  // ============================================================================
  // CAMERA DISCOVERY
  // ============================================================================

  // Camera discovery and enumeration
  static std::vector<ExtendedCameraInfo> DiscoverAllCameras();
  static std::vector<CameraInfo> DiscoverPylonCameras();
  static std::vector<ExtendedCameraInfo> DiscoverIDSCameras();


  // NEW: Combined discovery and addition method
  bool DiscoverAndAddAllCameras();

private:
  // ============================================================================
  // INTERNAL STRUCTURES
  // ============================================================================

  // Internal camera data using interface
  struct ManagedCamera {
    ExtendedCameraInfo info;
    std::unique_ptr<ICameraHardware> camera;
    std::unique_ptr<PylonCameraTest> pylonCameraTest; // For backward compatibility

    ManagedCamera(const ExtendedCameraInfo& cameraInfo);
  };

  // Subscriber tracking structure
 // In CameraManager.h, update the SubscriberInfo struct to include a default constructor:

// Subscriber tracking structure
  struct SubscriberInfo {
    std::shared_ptr<CameraFrameSubscriber> subscriber;
    std::chrono::steady_clock::time_point lastBroadcast;
    std::set<std::string> interestedCameras;

    // Add default constructor
    SubscriberInfo()
      : subscriber(nullptr), lastBroadcast(std::chrono::steady_clock::now()) {
    }

    SubscriberInfo(std::shared_ptr<CameraFrameSubscriber> sub)
      : subscriber(sub), lastBroadcast(std::chrono::steady_clock::now()) {
    }

    // Helper method to check if subscriber wants frames from a camera
    bool WantsFramesFrom(const std::string& cameraId) const {
      return subscriber && subscriber->WantsFramesFromCamera(cameraId);
    }
  };

  // ============================================================================
  // MEMBER VARIABLES
  // ============================================================================

  // Camera storage
  mutable std::mutex m_camerasMutex;
  std::unordered_map<std::string, std::unique_ptr<ManagedCamera>> m_cameras;

  // Broadcasting system
  mutable std::mutex m_subscribersMutex;
  std::unordered_map<std::string, SubscriberInfo> m_subscribers;
  std::atomic<bool> m_broadcastSystemRunning;
  std::atomic<bool> m_shouldStopBroadcast;
  std::thread m_broadcastThread;
  float m_globalBroadcastRate;

  // UI state
  bool m_showUI = true;
  std::string m_selectedCameraId;

  // Legacy broadcasting system members (for compatibility)
  std::atomic<bool> m_broadcastActive{ false };
  std::queue<CameraFrameData> m_frameQueue;
  std::mutex m_frameQueueMutex;
  std::condition_variable m_frameQueueCV;
  std::atomic<int> m_globalMinFrameInterval{ 33 }; // 30fps default
  std::atomic<size_t> m_maxQueueSize{ 10 }; // Prevent memory buildup

  // ============================================================================
  // HELPER METHODS
  // ============================================================================

  // Camera finding helpers
  ManagedCamera* FindCamera(const std::string& cameraId);
  const ManagedCamera* FindCamera(const std::string& cameraId) const;

  // UI rendering methods
  void RenderCameraList();
  void RenderSelectedCameraPanel();
  void RenderCameraStatusTable();
  void RenderBulkOperations();
  void RenderEnhancedCameraList();         // Enhanced camera list with connection info
  void RenderEnhancedCameraStatusTable();  // Enhanced status table with network info

  // Broadcasting helper methods
  void BroadcastFrame(const CameraFrameData& frameData);
  void BroadcastThreadFunction();
  void EnqueueFrame(const CameraFrameData& frameData);
  void OnCameraFrameReceived(const std::string& cameraId, const Pylon::CGrabResultPtr& grabResult);
  void CleanupExpiredSubscribers();
  bool RestartGrabbingWithBroadcast(const std::string& cameraId);
};