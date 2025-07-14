// global_jog_panel.h - Split version header
#pragma once

#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/logger.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <map>

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

  // Main interface
  void RenderUI();
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }
  const std::string& GetName() const { return m_windowTitle; }
  void ProcessKeyInput(int keyCode, bool keyDown);

  bool debugverbose = false;

private:
  // Core members
  MotionConfigManager& m_configManager;
  PIControllerManager& m_piControllerManager;
  ACSControllerManager& m_acsControllerManager;
  Logger* m_logger;

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Global Jog Control";
  bool m_keyBindingEnabled = false;
  bool m_showPositions = false;
  std::string m_selectedDevice;

  // Step configuration
  std::vector<double> m_jogSteps;
  int m_currentStepIndex = 6; // Default 10 micron

  // Key bindings
  struct KeyBinding {
    std::string key;
    int keyCode;
    std::string action;
    std::string description;
  };
  std::vector<KeyBinding> m_keyBindings;

  // Device transformations
  std::vector<DeviceTransform> m_deviceTransforms;

  // Core functionality (will be in global_jog_panel_core.cpp)
  void InitializeStepSizes();
  void InitializeKeyBindings();
  bool LoadTransformations(const std::string& filePath);
  void TransformMovement(const std::string& deviceId,
    double globalX, double globalY, double globalZ,
    double& deviceX, double& deviceY, double& deviceZ);

  // Movement functions (will be in global_jog_panel_movement.cpp)
  void MoveAxis(const std::string& axis);
  void IncreaseStep();
  void DecreaseStep();
  void MoveRotationAxis(const std::string& axis, double amount);

  // UI rendering functions (will be in global_jog_panel_ui.cpp)
  void RenderDeviceButtons();
  void RenderStepSizeControls();
  void RenderPositionDisplay();
  void RenderRotationControls();
  ImVec4 GetButtonColor(const std::string& key);
  std::string FormatStepSize(double stepSize) const;

  // Device functions (will be in global_jog_panel_device.cpp)
  std::map<std::string, double> GetCurrentPositions();
  bool IsDeviceConnected() const;
  bool DeviceSupportsUVW(const std::string& deviceId);

  // Quick step indices
  static const int QUICK_STEP_0_5_MICRON = 2;   // 0.5 micron
  static const int QUICK_STEP_1_MICRON = 3;     // 1 micron
  static const int QUICK_STEP_5_MICRON = 5;     // 5 micron
  static const int QUICK_STEP_10_MICRON = 6;    // 10 micron
};