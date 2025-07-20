// MacroPanelCameraHandler.h
#pragma once
#include "CameraFeedDisplay.h"                      // Fixed: Remove mainUI/ path
#include "imgui.h"
#include <string>
#include <memory>

// Forward declarations
class CameraManager;
class CameraFeedDisplay;
class PylonCameraTest;

class MacroPanelCameraHandler {
public:
  MacroPanelCameraHandler(CameraManager* cameraManager = nullptr);
  ~MacroPanelCameraHandler() = default;

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
  bool IsCameraInitialized() const { return m_cameraInitialized; }

  // Camera selection
  const std::string& GetSelectedCameraId() const { return m_selectedCameraId; }
  void SetSelectedCameraId(const std::string& cameraId);

private:
  // Camera system references
  CameraManager* m_cameraManager = nullptr;
  std::unique_ptr<CameraFeedDisplay> m_cameraFeedDisplay;

  // Camera state
  std::string m_selectedCameraId;
  bool m_cameraInitialized = false;

  // UI rendering methods
  void RenderCameraControls();
  void RenderCameraFeed(ImVec2 canvasSize);
  void RenderPlaceholder(ImVec2 canvasSize);

  // Camera operations
  void InitializeCameraFeed();
  ImVec2 CalculateCanvasSize();
};