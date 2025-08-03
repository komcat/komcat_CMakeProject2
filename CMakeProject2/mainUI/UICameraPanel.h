// UICameraPanel.h - Main Camera Panel coordinating sub-panels
#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class CameraManager;
class ICameraHardware;
class UICameraPanelLiveVideo;
class UICameraPanelSingleGrab;
class UICameraPanelUtility;

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

  // Camera selection management
  const std::string& GetSelectedCameraId() const { return m_selectedCameraId; }
  ICameraHardware* GetSelectedCamera() const;

private:
  // Reference to camera manager
  CameraManager& m_cameraManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedCameraId;

  // Sub-panel components
  std::unique_ptr<UICameraPanelLiveVideo> m_liveVideoPanel;
  std::unique_ptr<UICameraPanelSingleGrab> m_singleGrabPanel;
  std::unique_ptr<UICameraPanelUtility> m_utilityPanel;

  // Panel rendering methods
  void RenderLeftPanel();         // Camera list and global controls
  void RenderMiddlePanelTabs();   // Tabbed camera feed display
  void RenderRightPanel();       // Selected camera interface

  // Helper methods
  void RenderCameraList();
  void RenderGlobalControls();
  void RenderNoSelectionMessage();

  // Camera selection handling
  void OnCameraSelectionChanged(const std::string& newCameraId);
  void ClearAllPanels();
};