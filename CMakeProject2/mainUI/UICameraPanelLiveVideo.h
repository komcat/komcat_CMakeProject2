// UICameraPanelLiveVideo.h - Updated with new method declarations
#pragma once

#include <memory>
#include <string>
#include <chrono>

// Forward declarations
class CameraManager;
class PylonCameraTest;
class CameraFeedDisplay;

class UICameraPanelLiveVideo {
public:
  UICameraPanelLiveVideo(CameraManager& cameraManager);
  ~UICameraPanelLiveVideo();

  // Disable copy/move
  UICameraPanelLiveVideo(const UICameraPanelLiveVideo&) = delete;
  UICameraPanelLiveVideo& operator=(const UICameraPanelLiveVideo&) = delete;
  UICameraPanelLiveVideo(UICameraPanelLiveVideo&&) = delete;
  UICameraPanelLiveVideo& operator=(UICameraPanelLiveVideo&&) = delete;

  // Main rendering method
  void RenderTab(PylonCameraTest* camera, const std::string& cameraId);

  // Camera management
  void SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId);
  void ClearCamera();

  // Status queries
  bool IsLiveActive() const;
  std::string GetStatusText() const;

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  std::string m_currentCameraId;
  PylonCameraTest* m_currentCamera = nullptr;

  // Live video feed display
  std::unique_ptr<CameraFeedDisplay> m_feedDisplay;

  // State tracking
  bool m_isGrabbing = false;
  std::chrono::steady_clock::time_point m_lastStatusUpdate;

  // **NEW: UI rendering helpers for reorganized layout**
  void RenderControls();               // Main controls for left column
  void RenderDetailedStatus();         // Detailed status for right column
  void RenderFeedDisplay();            // Feed display area
  void RenderErrorCanvas(float width, float height, const std::string& errorText);

  // Camera operations
  void StartLiveVideo();
  void StopLiveVideo();
  void ToggleLiveVideo();

  // Internal state management
  void UpdateGrabbingState();
  bool ValidateCamera() const;
};