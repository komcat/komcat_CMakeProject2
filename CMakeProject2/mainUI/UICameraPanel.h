// UICameraPanel.h - UI panel for managing multiple cameras
#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class CameraManager;
class PylonCameraTest;
class CameraFeedDisplay;

class UICameraPanel {
public:
  UICameraPanel(CameraManager& cameraManager);
  ~UICameraPanel();

  // Disable copy/move to avoid issues with references
  UICameraPanel(const UICameraPanel&) = delete;
  UICameraPanel& operator=(const UICameraPanel&) = delete;
  UICameraPanel(UICameraPanel&&) = delete;
  UICameraPanel& operator=(UICameraPanel&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedCameraId;

  // Camera feed display
  std::unique_ptr<CameraFeedDisplay> m_feedDisplay;

  // Panel rendering methods
  void RenderLeftPanel();   // List of cameras
  void RenderMiddlePanel(); // Live camera feed
  void RenderRightPanel();  // Selected camera interface

  // Helper methods
  void RenderCameraList();
  void RenderSelectedCameraUI();
  void RenderNoSelectionMessage();

  // Embedded UI rendering methods
  void RenderCameraHeader(PylonCameraTest* camera);
  void RenderConnectionControls(PylonCameraTest* camera);
  void RenderCameraStatus(PylonCameraTest* camera);
  void RenderGrabbingControls(PylonCameraTest* camera);
  void RenderExposureControls(PylonCameraTest* camera);
  void RenderImageControls(PylonCameraTest* camera);
  void RenderUtilityControls(PylonCameraTest* camera);

  // UI state for exposure controls
  float m_customExposureTime = 1000.0f;  // microseconds
  float m_customGain = 1.0f;              // 0-10 scale
  bool m_exposureAuto = false;
  bool m_gainAuto = false;
};