// UIJogWindow.h
#pragma once

#include <memory>
#include <string>

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

  // Render - just forwards to GlobalJogPanel but sets predetermined position
  void RenderUI();

  // Key input forwarding  
  void ProcessKeyInput(int keyCode, bool keyDown);

  // Set predetermined position
  void SetPredeterminedPosition();

private:
  // Remove GlobalJogPanel dependency for now - use only mock mode
  bool m_firstRender = true;

  // Mock functionality for testing without controllers
  bool m_mockWindowVisible = false;
  void RenderMockJogWindow();
};