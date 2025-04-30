// pi_analog_reader.h
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

  // Get the number of available analog channels
  bool GetNumberOfChannels(int& numChannels);

  // Get the raw ADC values (dimensionless) for all channels
  bool GetRawValues(std::map<int, int>& rawValues);

  // Get the raw ADC value for a specific channel
  bool GetRawValue(int channel, int& value);

  // Get the voltage values (in volts) for all channels
  bool GetVoltageValues(std::map<int, double>& voltageValues);

  // Get the voltage value for a specific channel
  bool GetVoltageValue(int channel, double& voltage);

  // Poll and update all values
  bool UpdateAllValues();

  // Render UI for displaying analog readings
  void RenderUI();

  // Set visibility of the window
  void SetWindowVisible(bool visible) { m_showWindow = visible; }

private:
  PIController& m_controller;
  std::string m_deviceName;
  Logger* m_logger;

  // Cached data about analog channels
  int m_numChannels = 0;
  std::map<int, int> m_rawValues;      // ADC values
  std::map<int, double> m_voltageValues; // Values in volts

  // UI state
  bool m_showWindow = false;
  std::string m_windowTitle;
  bool m_autoRefresh = true;
  float m_refreshInterval = 0.2f; // in seconds
  float m_lastRefreshTime = 0.0f;

  // Helper methods
  bool IsControllerValid() const;
};