// UIJogWindow.h - Updated to wrap GlobalJogPanel
#pragma once

#include "include/motions/global_jog_panel.h"
#include <memory>

// Forward declarations
class MotionConfigManager;
class PIControllerManager;
class ACSControllerManager;

class UIJogWindow {
public:
  // Constructor with managers (for real operation)
  UIJogWindow(MotionConfigManager& configManager,
    PIControllerManager& piControllerManager,
    ACSControllerManager& acsControllerManager);

  // Constructor without managers (for testing/mock mode)
  UIJogWindow(MotionConfigManager& configManager);

  ~UIJogWindow();

  // Disable copy/move
  UIJogWindow(const UIJogWindow&) = delete;
  UIJogWindow& operator=(const UIJogWindow&) = delete;
  UIJogWindow(UIJogWindow&&) = delete;
  UIJogWindow& operator=(UIJogWindow&&) = delete;

  // Window control
  void ToggleWindow();
  bool IsVisible() const;
  void SetVisible(bool visible);

  // Render - forwards to GlobalJogPanel but sets predetermined position
  void RenderUI();

  // Key input forwarding  
  void ProcessKeyInput(int keyCode, bool keyDown);

  // Set predetermined position
  void SetPredeterminedPosition();

  // Methods to set controller managers (called from MainUIManager)
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);

private:
  // References to managers
  MotionConfigManager& m_configManager;
  PIControllerManager* m_piControllerManager = nullptr;
  ACSControllerManager* m_acsControllerManager = nullptr;

  // GlobalJogPanel instance (created when controllers are available)
  std::unique_ptr<GlobalJogPanel> m_globalJogPanel;

  // Window management
  bool m_firstRender = true;

  // Mock functionality for testing without controllers
  bool m_mockWindowVisible = false;
  void RenderMockJogWindow();

  // Helper to create GlobalJogPanel when managers are available
  void CreateGlobalJogPanel();
};