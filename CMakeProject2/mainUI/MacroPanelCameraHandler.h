// MacroPanelCameraHandler.h - Updated with Broadcasting Support
#pragma once

#include "LiveVideoSubscriber.h"  // NEW: Include subscriber
#include "include/camera/CameraFrameData.h"  // NEW: Include frame data
#include "imgui.h"
#include <string>
#include <memory>
#include <chrono>

// Forward declarations
class CameraManager;
class PylonCameraTest;

class MacroPanelCameraHandler {
public:
  MacroPanelCameraHandler(CameraManager* cameraManager = nullptr);
  ~MacroPanelCameraHandler();

  // Disable copy/move
  MacroPanelCameraHandler(const MacroPanelCameraHandler&) = delete;
  MacroPanelCameraHandler& operator=(const MacroPanelCameraHandler&) = delete;
  MacroPanelCameraHandler(MacroPanelCameraHandler&&) = delete;
  MacroPanelCameraHandler& operator=(MacroPanelCameraHandler&&) = delete;

  // Main rendering method
  void RenderCameraCanvas();

  // Camera management
  void SetCameraManager(CameraManager* cameraManager);
  bool HasCameraManager() const { return m_cameraManager != nullptr; }

  // NEW: Broadcasting-based camera selection
  const std::string& GetSelectedCameraId() const { return m_selectedCameraId; }
  void SetSelectedCameraId(const std::string& cameraId);

  // NEW: Feed status queries
  bool IsCameraInitialized() const { return m_cameraInitialized; }
  bool IsLiveActive() const;
  std::string GetStatusText() const;

  // NEW: Access to subscriber for debugging
  std::shared_ptr<LiveVideoSubscriber> GetSubscriber() const { return m_subscriber; }

private:
  // Camera system references
  CameraManager* m_cameraManager = nullptr;

  // NEW: Broadcasting subscriber (replaces direct CameraFeedDisplay)
  std::shared_ptr<LiveVideoSubscriber> m_subscriber;

  // NEW: Display texture for ImGui (direct texture management)
  unsigned int m_textureID = 0;
  bool m_textureInitialized = false;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;

  // Camera state
  std::string m_selectedCameraId;
  bool m_cameraInitialized = false;

  // State tracking
  bool m_isGrabbing = false;
  std::chrono::steady_clock::time_point m_lastStatusUpdate;

  // UI rendering methods
  void RenderCameraControls();
  void RenderCameraFeed(ImVec2 canvasSize);
  void RenderPlaceholder(ImVec2 canvasSize);

  // NEW: Camera operations with broadcasting
  void InitializeCameraFeed();
  void StartLiveVideo();
  void StopLiveVideo();
  void ToggleLiveVideo();

  // NEW: Frame processing methods (for subscriber pattern)
  void UpdateTextureFromFrameData(const CameraFrameData& frameData);
  void CreateOrUpdateTexture(const uint8_t* imageData, uint32_t width, uint32_t height);
  void CleanupTexture();

  // Internal state management
  void UpdateGrabbingState();
  bool ValidateCamera() const;
  ImVec2 CalculateCanvasSize();
};