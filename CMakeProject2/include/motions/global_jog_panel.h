// global_jog_panel.h
#pragma once

#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/logger.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <array>
#include <functional>

// Matrix structure for transformation
struct TransformationMatrix {
  double M11, M12, M13;
  double M21, M22, M23;
  double M31, M32, M33;
};

// Device transformation data
struct DeviceTransform {
  std::string deviceId;
  TransformationMatrix matrix;
};

class GlobalJogPanel {
public:
  GlobalJogPanel(MotionConfigManager& configManager,
    PIControllerManager& piControllerManager,
    ACSControllerManager& acsControllerManager);
  ~GlobalJogPanel();

  // Render the UI
  void RenderUI();

  // Show/hide the window
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }
  const std::string& GetName() const { return m_windowTitle; }

  // Process key input - call this from the main input handling loop
  void ProcessKeyInput(int keyCode, bool keyDown);
  bool debugverbose = false;
private:
  // References to managers
  MotionConfigManager& m_configManager;
  PIControllerManager& m_piControllerManager;
  ACSControllerManager& m_acsControllerManager;
  Logger* m_logger;

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Global Jog Control";
  bool m_keyBindingEnabled = false;

  // Available jog steps in mm
  std::vector<double> m_jogSteps = {
      0.0001, 0.0002, 0.0005,
      0.001, 0.002, 0.005,
      0.01, 0.02, 0.05,
      0.1, 0.2, 0.5,
      1.0, 2.0, 5.0
  };

  // Current step index
  int m_currentStepIndex = 6; // Default 0.01mm

  // Key bindings
  struct KeyBinding {
    std::string key;
    int keyCode;  // SDL key code
    std::string action;
    std::string description;
  };

  std::vector<KeyBinding> m_keyBindings;

  // Selected device for jogging
  std::string m_selectedDevice;

  // Device transformations
  std::vector<DeviceTransform> m_deviceTransforms;

  // Helper methods
  bool LoadTransformations(const std::string& filePath);
  void MoveAxis(const std::string& axis);
  void IncreaseStep();
  void DecreaseStep();

  // Transform a global movement vector to device-specific coordinates
  void TransformMovement(const std::string& deviceId,
    double globalX, double globalY, double globalZ,
    double& deviceX, double& deviceY, double& deviceZ);

  // Render styles
  ImVec4 GetButtonColor(const std::string& key);

  // Add these to the private section of the GlobalJogPanel class

// Check if the selected device supports UVW axes
  bool DeviceSupportsUVW(const std::string& deviceId);

  // Helper method for handling UVW axis movement
  void MoveRotationAxis(const std::string& axis, double amount);

  // UI helper for displaying the rotation controls
  void RenderRotationControls();

};