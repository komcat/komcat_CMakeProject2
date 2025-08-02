#pragma once
#include <functional>
#include <string>
#include "include/camera/IDSCameraUI.h"

class MenuManagerUaa3 {
public:
  MenuManagerUaa3();
  ~MenuManagerUaa3();

  void RenderMainMenuBar();

  // Window visibility controls
  bool IsRaylibDebugVisible() const { return m_showRaylibDebug; }
  void SetRaylibDebugVisible(bool visible) { m_showRaylibDebug = visible; }
  void ToggleRaylibDebug() { m_showRaylibDebug = !m_showRaylibDebug; }

  // Callbacks for menu actions
  void SetOnExitCallback(std::function<void()> callback) { m_onExit = callback; }
  void SetIDSCameraUI(IDSCameraUI* ui) { m_idsCameraUI = ui; }
private:
  bool m_showRaylibDebug = false;
  bool m_showAbout = false;
  bool m_showDemo = false;

  std::function<void()> m_onExit;

  void RenderFileMenu();
  void RenderRaylibMenu();
  void RenderDebugMenu();
  void RenderHelpMenu();
  void RenderAboutDialog();

  // ... your existing members ...
  IDSCameraUI* m_idsCameraUI = nullptr;  // Add this pointer
};