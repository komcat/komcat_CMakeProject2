// UICameraPanelUtility.h - Camera Utility and Control Panel
#pragma once

#include <memory>
#include <string>

// Forward declarations
class CameraManager;
class PylonCameraTest;
class PylonCamera;

class UICameraPanelUtility {
public:
  UICameraPanelUtility(CameraManager& cameraManager);
  ~UICameraPanelUtility();

  // Disable copy/move
  UICameraPanelUtility(const UICameraPanelUtility&) = delete;
  UICameraPanelUtility& operator=(const UICameraPanelUtility&) = delete;
  UICameraPanelUtility(UICameraPanelUtility&&) = delete;
  UICameraPanelUtility& operator=(UICameraPanelUtility&&) = delete;

  // Main rendering method
  void RenderPanel(PylonCameraTest* camera, const std::string& cameraId);

  // Camera management
  void SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId);
  void ClearCamera();

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  std::string m_currentCameraId;
  PylonCameraTest* m_currentCamera = nullptr;

  // UI state for exposure controls
  float m_customExposureTime = 1000.0f;  // microseconds
  float m_customGain = 1.0f;              // 0-10 scale
  bool m_exposureAuto = false;
  bool m_gainAuto = false;

  // UI sections
  void RenderCameraHeader();
  void RenderConnectionControls();
  void RenderCameraStatus();
  void RenderGrabbingControls();
  void RenderExposureControls();
  void RenderImageControls();
  void RenderAdvancedControls();
  void RenderDebugControls();

  // Helper methods
  bool ValidateCamera() const;
  void UpdateExposureUIFromCamera();
  void ApplyExposureSettingsToCamera();
  void SafeDisconnectCamera();
};