// vision_ops.h
#pragma once

#include "include/camera/pylon_camera_test.h"
#include "include/camera/CameraManager.h"
#include "include/camera/CameraExposureManager.h"
#include "include/logger.h"
#include "include/data/DatabaseManager.h"
#include "include/data/OperationResultsManager.h"
#include <string>
#include <memory>

class VisionOps {
public:
  VisionOps(
    PylonCameraTest* cameraTest = nullptr,
    CameraManager* cameraManager = nullptr,
    std::shared_ptr<DatabaseManager> dbManager = nullptr,
    std::shared_ptr<OperationResultsManager> resultsManager = nullptr
  );

  // New constructor overload with CameraManager only
  VisionOps(
    CameraManager* cameraManager,
    std::shared_ptr<DatabaseManager> dbManager = nullptr,
    std::shared_ptr<OperationResultsManager> resultsManager = nullptr
  );

  ~VisionOps();

	bool Initialize();

  // Camera control methods
  bool InitializeCamera();
  bool ConnectCamera();
  bool DisconnectCamera();
  bool StartCameraGrabbing();
  bool StopCameraGrabbing();

  // Camera status methods
  bool IsCameraInitialized() const;
  bool IsCameraConnected() const;
  bool IsCameraGrabbing() const;

  // Camera capture methods
  bool CaptureImageToFile(const std::string& filename = "");
  bool UpdateCameraDisplay();
  bool IntegrateCameraWithMotion(PylonCameraTest* cameraTest);

  // Camera exposure control methods
  bool ApplyCameraExposureForNode(const std::string& nodeId);
  bool ApplyDefaultCameraExposure();

  // Enable/disable automatic camera exposure adjustment
  void SetAutoExposureEnabled(bool enabled) { m_autoExposureEnabled = enabled; }
  bool IsAutoExposureEnabled() const { return m_autoExposureEnabled; }

  // Get camera exposure manager
  CameraExposureManager* GetCameraExposureManager() { return m_cameraExposureManager.get(); }

  // Test method to verify current camera settings
  void TestCameraSettings(const std::string& nodeId = "");

  // Access to underlying managers
  PylonCameraTest* GetPylonCameraTest() { return m_cameraTest; }
  CameraManager* GetCameraManager() { return m_cameraManager; }

  // Logging methods
  void LogInfo(const std::string& message) const;
  void LogWarning(const std::string& message) const;
  void LogError(const std::string& message) const;

private:
  Logger* m_logger;

  // Camera components
  PylonCameraTest* m_cameraTest;
  CameraManager* m_cameraManager;

  // Camera exposure management
  std::unique_ptr<CameraExposureManager> m_cameraExposureManager;
  bool m_autoExposureEnabled = true;

  // Database and result tracking
  std::shared_ptr<DatabaseManager> m_dbManager;
  std::shared_ptr<OperationResultsManager> m_resultsManager;
};