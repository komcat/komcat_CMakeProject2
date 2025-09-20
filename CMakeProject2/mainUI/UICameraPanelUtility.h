// UICameraPanelUtility.h - Camera Utility and Control Panel
#pragma once

#include <string>
#include <chrono>

// Forward declarations
class CameraManager;
class ICameraHardware;

class UICameraPanelUtility {
public:
  UICameraPanelUtility(CameraManager& cameraManager);
  ~UICameraPanelUtility();

  // Disable copy/move to avoid issues with references
  UICameraPanelUtility(const UICameraPanelUtility&) = delete;
  UICameraPanelUtility& operator=(const UICameraPanelUtility&) = delete;
  UICameraPanelUtility(UICameraPanelUtility&&) = delete;
  UICameraPanelUtility& operator=(UICameraPanelUtility&&) = delete;

  // Panel rendering for use in UICameraPanel
  void RenderPanel(ICameraHardware* camera, const std::string& cameraId);

  // Camera management
  void SetSelectedCamera(ICameraHardware* camera, const std::string& cameraId);
  void ClearCamera();

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  ICameraHardware* m_currentCamera = nullptr;
  std::string m_currentCameraId;

  // UI state for exposure controls
  float m_exposureTimeUI = 10000.0f; // microseconds
  float m_gainUI = 1.0f;
  bool m_autoExposureUI = false;
  bool m_autoGainUI = false;

  // Error rate limiting
  std::chrono::steady_clock::time_point m_lastErrorTime;
  static constexpr std::chrono::seconds ERROR_RATE_LIMIT{ 3 }; // Limit errors to once per 3 seconds

  // UI rendering methods
  void RenderCameraHeader();
  void RenderConnectionControls();
  void RenderCameraStatus();
  void RenderGrabbingControls();
  void RenderExposureControls();
  void RenderImageControls();
  void RenderAdvancedControls();
  void RenderDebugControls();

  // Validation and helper methods
  bool ValidateCamera() const;
  void UpdateExposureUIFromCamera();
  void ApplyExposureSettingsFromUI();

  // Error rate limiting helper
  bool CanLogError();
};