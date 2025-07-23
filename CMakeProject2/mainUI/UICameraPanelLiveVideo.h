// UICameraPanelLiveVideo.h - Updated with LiveVideoSubscriber
#pragma once

#include <memory>
#include <string>
#include <chrono>
#include "include/camera/CameraFrameData.h"

// Forward declarations
class CameraManager;
class PylonCameraTest;
class LiveVideoSubscriber;  // Changed from CameraFeedDisplay

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

  // Access to subscriber for debugging
  std::shared_ptr<LiveVideoSubscriber> GetSubscriber() const { return m_subscriber; }

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  std::string m_currentCameraId;
  PylonCameraTest* m_currentCamera = nullptr;

  // Broadcasting subscriber (NEW: replaced CameraFeedDisplay)
  std::shared_ptr<LiveVideoSubscriber> m_subscriber;

  // Display texture for ImGui (NEW: direct texture management)
  unsigned int m_textureID = 0;
  bool m_textureInitialized = false;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;

  // State tracking
  bool m_isGrabbing = false;
  std::chrono::steady_clock::time_point m_lastStatusUpdate;

  // UI rendering helpers
  void RenderControls();
  void RenderDetailedStatus();
  void RenderFeedDisplay();
  void RenderErrorCanvas(float width, float height, const std::string& errorText);

  // Camera operations
  void StartLiveVideo();
  void StopLiveVideo();
  void ToggleLiveVideo();

  // Frame processing methods (NEW: for subscriber pattern)
  void UpdateTextureFromFrameData(const CameraFrameData& frameData);
  void CreateOrUpdateTexture(const uint8_t* imageData, uint32_t width, uint32_t height);
  void CleanupTexture();

  // Internal state management
  void UpdateGrabbingState();
  bool ValidateCamera() const;
};