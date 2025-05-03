// pi_analog_reader.h - simplified version
#pragma once

#include "pi_controller.h"
#include "../logger.h"
#include <vector>
#include <map>
#include <string>

class PIAnalogReader {
public:
  PIAnalogReader(PIController& controller, const std::string& deviceName);
  ~PIAnalogReader();

  // Core functionality
  bool GetNumberOfChannels(int& numChannels);
  bool GetVoltageValues(std::map<int, double>& voltageValues);
  bool GetVoltageValue(int channel, double& voltage);
  bool UpdateAllValues();

  // Get the latest values
  const std::map<int, double>& GetLatestVoltageValues() const { return m_voltageValues; }

  // Simplified UI - only show essential information
  void RenderUI();
  void SetWindowVisible(bool visible) { m_showWindow = visible; }

private:
  PIController& m_controller;
  std::string m_deviceName;
  Logger* m_logger;

  // Cached data about analog channels
  int m_numChannels = 0;
  std::map<int, double> m_voltageValues; // Values in volts

  // Minimal UI state
  bool m_showWindow = false;
  std::string m_windowTitle;

  // Helper methods
  bool IsControllerValid() const;
};