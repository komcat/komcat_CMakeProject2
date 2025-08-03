// UICameraPanelLiveVideo.h - Live Video Feed Panel
#pragma once

#include <memory>
#include <string>
#include <atomic>

// Forward declarations
class CameraManager;
class ICameraHardware;
class LiveVideoSubscriber;

class UICameraPanelLiveVideo {
public:
  UICameraPanelLiveVideo(CameraManager& cameraManager);
  ~UICameraPanelLiveVideo();

  // Disable copy/move to avoid issues with references
  UICameraPanelLiveVideo(const UICameraPanelLiveVideo&) = delete;
  UICameraPanelLiveVideo& operator=(const UICameraPanelLiveVideo&) = delete;
  UICameraPanelLiveVideo(UICameraPanelLiveVideo&&) = delete;
  UICameraPanelLiveVideo& operator=(UICameraPanelLiveVideo&&) = delete;

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
  std::atomic<bool> m_isGrabbing{ false };

  // Broadcasting subscriber for frame data
  std::shared_ptr<LiveVideoSubscriber> m_subscriber;

  // OpenGL texture for display
  unsigned int m_textureId = 0;
  int m_textureWidth = 0;
  int m_textureHeight = 0;
  bool m_needsTextureCleanup = false;

  // UI rendering methods
  void RenderControls();
  void RenderFeedDisplay();
  void RenderDetailedStatus();

  // Live video control
  void ToggleLiveVideo();
  void UpdateGrabbingState();

  // Texture management
  void UpdateTextureFromFrameData(const class CameraFrameData& frameData);
  void CleanupTexture();

  // Validation
  bool ValidateCamera() const;
};