#pragma once

#include "pylon_camera_test.h"
#include "CameraExposureManager.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

// Simple structure to identify cameras
struct CameraInfo {
  std::string id;           // Camera identifier (e.g., "camera_1", "camera_2")
  std::string serialNumber; // Camera serial number (if connecting by serial)
  std::string description;  // Human readable description
  bool autoConnect;         // Whether to auto-connect this camera

  CameraInfo(const std::string& cameraId, const std::string& desc = "", bool autoConn = true)
    : id(cameraId), description(desc), autoConnect(autoConn) {
  }

  CameraInfo(const std::string& cameraId, const std::string& serial, const std::string& desc, bool autoConn = true)
    : id(cameraId), serialNumber(serial), description(desc), autoConnect(autoConn) {
  }
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
};