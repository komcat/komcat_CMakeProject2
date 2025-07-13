// UICameraPanelSingleGrab.h - Single Frame Capture Panel
#pragma once
#include "imgui.h"
#include <memory>
#include <string>
#include <chrono>

// Forward declarations
class CameraManager;
class PylonCameraTest;
class CameraFeedDisplay;

class UICameraPanelSingleGrab {
public:
  UICameraPanelSingleGrab(CameraManager& cameraManager);
  ~UICameraPanelSingleGrab();

  // Disable copy/move
  UICameraPanelSingleGrab(const UICameraPanelSingleGrab&) = delete;
  UICameraPanelSingleGrab& operator=(const UICameraPanelSingleGrab&) = delete;
  UICameraPanelSingleGrab(UICameraPanelSingleGrab&&) = delete;
  UICameraPanelSingleGrab& operator=(UICameraPanelSingleGrab&&) = delete;

  // Main rendering method
  void RenderTab(PylonCameraTest* camera, const std::string& cameraId);

  // Camera management
  void SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId);
  void ClearCamera();

  // Frame capture operations
  bool GrabSingleFrame();
  void ClearCapturedFrame();
  bool SaveFrameToDisk();
  bool GrabSingleFrameAlternative();
  // Status queries
  bool HasCapturedFrame() const { return m_hasCapturedFrame; }
  std::string GetCaptureTimeText() const;

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // Current camera state
  std::string m_currentCameraId;
  PylonCameraTest* m_currentCamera = nullptr;

  // Single frame display
  std::unique_ptr<CameraFeedDisplay> m_frameDisplay;

  // Capture state
  bool m_hasCapturedFrame = false;
  std::chrono::steady_clock::time_point m_lastCaptureTime;
  std::string m_lastSavedPath;

  // UI settings
  bool m_autoSave = false;
  bool m_showCaptureInfo = true;

  // UI rendering helpers
  void RenderControls();
  void RenderStatus();
  void RenderFrameDisplay();
  void RenderCaptureSettings();
  void RenderPlaceholderCanvas(float width, float height, const std::string& text);
  void RenderCaptureInfoOverlay(ImVec2 canvasSize);
  // Internal operations
  bool ValidateCamera() const;
  void UpdateCaptureDisplay();
  std::string GenerateFilename() const;

  void DebugCameraTextureState();
};