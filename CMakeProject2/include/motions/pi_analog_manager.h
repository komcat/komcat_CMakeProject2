// pi_analog_manager.h
#pragma once

#include "pi_analog_reader.h"
#include "pi_controller_manager.h"
#include "MotionConfigManager.h"
#include <map>
#include <memory>
#include <vector>
#include <string>

class PIAnalogManager {
public:
  PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager);
  ~PIAnalogManager();

  // Initialize readers for all controllers
  void InitializeReaders();

  // Get a reader for a specific device
  PIAnalogReader* GetReader(const std::string& deviceName);

  // Update readings for all connected devices
  void UpdateAllReadings();

  // Render UI
  void RenderUI();

private:
  PIControllerManager& m_controllerManager;
  MotionConfigManager& m_configManager;
  std::map<std::string, std::unique_ptr<PIAnalogReader>> m_readers;
  Logger* m_logger;

  // Get a list of device names that have PI controllers
  std::vector<std::string> GetPIControllerDeviceNames() const;

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "PI Analog Manager";
};