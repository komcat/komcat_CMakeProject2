#pragma once

#include "EziIO_Manager.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <map>

// Simple class for displaying and controlling specific IO pins
class IOControlPanel {
public:
  IOControlPanel(EziIOManager& manager);
  ~IOControlPanel() = default;

  // Render the control panel UI
  void RenderUI();

  // Show/hide the window
  bool IsVisible() const { return m_showWindow; }
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  void Show() { m_showWindow = true; }
  void Hide() { m_showWindow = false; }

  // Get component name (needed for ToolbarMenu compatibility)
  const std::string& GetName() const { return m_name; }

private:
  // Reference to the IO manager
  EziIOManager& m_ioManager;

  // Window visibility state
  bool m_showWindow = true;

  // Component name for ToolbarMenu
  std::string m_name = "IO Quick Control";

  // Structure to hold pin configuration
  struct PinConfig {
    std::string deviceName;
    int deviceId;
    int pinNumber;
    std::string label;
  };

  // List of output pins to control
  std::vector<PinConfig> m_outputPins;

  // Initialize the list of pins to control
  void InitializePins();
};