// HexRightControllerWindow.h
#pragma once

#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include "include/motions/pi_controller_manager.h" // For PIControllerManager
#include "include/logger.h"
#include "imgui.h"
#include <string>

// Simple window to display hex-right controller info
class HexRightControllerWindow : public ITogglableUI {
public:
  HexRightControllerWindow(PIControllerManager& controllerManager);
  ~HexRightControllerWindow() = default;

  // ITogglableUI interface implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Render the window UI
  void RenderUI();

  // Start the FSM scan
  bool StartFSMScan();

private:
  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Hex-Right Controller";

  // Reference to controller manager
  PIControllerManager& m_controllerManager;

  // Scan parameters
  std::string m_axis1 = "X";
  std::string m_axis2 = "Y";
  double m_length1 = 0.5;
  double m_length2 = 0.5;
  double m_threshold = 1.0;
  double m_distance = 0.1;
  int m_analogInput = 5;

  // Logger
  Logger* m_logger;
};