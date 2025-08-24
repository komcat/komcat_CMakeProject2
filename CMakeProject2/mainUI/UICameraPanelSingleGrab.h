// UICameraPanelSingleGrab.h - Single Frame Capture Panel
#pragma once

#include <string>

// Forward declarations
class CameraManager;
class ICameraHardware;

class UICameraPanelSingleGrab {
public:
  UICameraPanelSingleGrab(CameraManager& cameraManager);
  ~UICameraPanelSingleGrab();

  // Disable copy/move to avoid issues with references
  UICameraPanelSingleGrab(const UICameraPanelSingleGrab&) = delete;
  UICameraPanelSingleGrab& operator=(const UICameraPanelSingleGrab&) = delete;
  UICameraPanelSingleGrab(UICameraPanelSingleGrab&&) = delete;
  UICameraPanelSingleGrab& operator=(UICameraPanelSingleGrab&&) = delete;

  // Tab rendering for use in UICameraPanel
  void RenderTab(ICameraHardware* camera, const std::string& cameraId);

  // Camera management
  void SetSelectedCamera(ICameraHardware* camera, const std::string& cameraId);
  void ClearCamera();

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  ICameraHardware* m_currentCamera = nullptr;
  std::string m_currentCameraId;

  // Single frame capture state
  bool m_captureInProgress = false;
  std::string m_lastCaptureStatus;

  // OpenGL texture for captured frame display
  unsigned int m_capturedTextureId = 0;
  int m_capturedWidth = 0;
  int m_capturedHeight = 0;
  bool m_hasCapturedFrame = false;

  // UI rendering methods
  void RenderCaptureControls();
  void RenderCapturedFrameDisplay();
  void RenderCaptureStatus();

  // Capture operations
  void CaptureSingleFrame();
  void ClearCapturedFrame();

  // Texture management
  void UpdateCapturedTexture(const struct CameraFrameData& frameData);
  void CleanupCapturedTexture();

  // Validation
  bool ValidateCamera() const;
};