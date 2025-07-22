#pragma once

#include "pylon_camera_test.h"
#include "CameraExposureManager.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

// Step 2.1: Update CameraManager.h - Enhanced CameraInfo structure

// REPLACE your existing CameraInfo struct in CameraManager.h with this enhanced version:

// Enhanced structure to identify cameras
struct CameraInfo {
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

// Simple multi-camera manager that wraps PylonCameraTest instances
class CameraManager {
public:
  CameraManager();
  ~CameraManager();

  // Add a camera to be managed (creates new PylonCameraTest instance)
  bool AddCamera(const CameraInfo& cameraInfo);

  // Remove a camera by ID
  bool RemoveCamera(const std::string& cameraId);

  // Get camera by ID
  PylonCameraTest* GetCamera(const std::string& cameraId);

  // Get all camera IDs
  std::vector<std::string> GetCameraIds() const;

  // Get camera count
  size_t GetCameraCount() const { return m_cameras.size(); }

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

  // Apply exposure settings to specific camera using node ID
  bool ApplyExposureForNode(const std::string& cameraId, const std::string& nodeId);

  // Apply exposure settings to all cameras using node ID
  bool ApplyExposureForNodeAll(const std::string& nodeId);

  // Apply direct exposure settings to specific camera
  bool ApplyExposureSettings(const std::string& cameraId, const PylonCamera::ExposureSettings& settings);

  // Capture image from specific camera
  bool CaptureImage(const std::string& cameraId);

  // Capture images from all cameras (synchronized)
  bool CaptureImageAll();

  // Get camera status info
  struct CameraStatus {
    std::string id;
    bool connected;
    bool grabbing;
    bool deviceRemoved;
    std::string deviceInfo;
    PylonCamera::ExposureSettings currentExposure;
  };

  CameraStatus GetCameraStatus(const std::string& cameraId) const;
  std::vector<CameraStatus> GetAllCameraStatus() const;

  // UI rendering for all cameras
  void RenderUI();

  // Toggle visibility of camera manager UI
  void ToggleWindow() { m_showUI = !m_showUI; }
  bool IsVisible() const { return m_showUI; }

private:
  // Internal camera data
  struct ManagedCamera {
    CameraInfo info;
    std::unique_ptr<PylonCameraTest> camera;

    ManagedCamera(const CameraInfo& cameraInfo)
      : info(cameraInfo), camera(std::make_unique<PylonCameraTest>()) {
    }
  };

  // Container for managed cameras
  std::unordered_map<std::string, std::unique_ptr<ManagedCamera>> m_cameras;

  // UI state
  bool m_showUI = true;
  std::string m_selectedCameraId;

  // Helper methods
  ManagedCamera* FindCamera(const std::string& cameraId);
  const ManagedCamera* FindCamera(const std::string& cameraId) const;

  // UI rendering methods
  void RenderCameraList();
  void RenderSelectedCameraPanel();
  void RenderCameraStatusTable();
  void RenderBulkOperations();



  // NEW: Enhanced UI rendering methods
  void RenderEnhancedCameraList();         // Enhanced camera list with connection info
  void RenderEnhancedCameraStatusTable();  // Enhanced status table with network info

};