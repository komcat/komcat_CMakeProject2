// include/ui/DigitalDisplayWithChart.h
#pragma once

#include "include/data/global_data_store.h"
#include <imgui.h>
#include "implot/implot.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <fstream>
#include <nlohmann/json.hpp>
#include <mutex>
#include <set>
#include <algorithm>
#include <chrono>

class DigitalDisplayWithChart {
public:
  struct DataBuffer {
    std::deque<float> values;
    std::deque<double> timestamps;
    std::string displayName;
    std::string unit;
    ImVec4 color;
    bool enabled = true;

    void addValue(float value, double timestamp) {
      values.push_back(value);
      timestamps.push_back(timestamp);

      // Keep only recent data (last 1000 points max)
      if (values.size() > 1000) {
        values.pop_front();
        timestamps.pop_front();
      }
    }

    void clearOldData(double currentTime, float timeWindow) {
      double cutoffTime = currentTime - timeWindow;

      // Remove data points that are older than the cutoff time
      // But keep at least 2 points to maintain continuity
      while (timestamps.size() > 2 && timestamps.front() < cutoffTime) {
        timestamps.pop_front();
        values.pop_front();
      }
    }
  };

  struct UnitInfo {
    std::string baseUnit;
    std::vector<std::pair<float, std::string>> prefixes;
  };

private:
  // Data management
  std::string m_selectedDataName;
  std::vector<std::pair<std::string, std::string>> m_availableChannels;
  bool m_channelsLoaded = false;
  bool m_showChannelPopup = false;

  // Chart data
  std::map<std::string, DataBuffer> m_dataBuffers;
  float m_timeWindow = 30.0f;
  bool m_showChart = false;
  bool m_autoScaleY = true;
  float m_yMin = -1.0f;
  float m_yMax = 1.0f;

  // Display settings
  bool m_digitalDisplayEnabled = true;
  ImVec2 m_windowSize = ImVec2(280, 120);
  std::string m_windowId;

  // Units mapping
  std::map<std::string, UnitInfo> m_unitsMap;
  std::map<std::string, std::string> m_displayNameMap;

  // Thread safety
  mutable std::mutex m_dataMutex;

public:
  DigitalDisplayWithChart(const std::string& initialDataName = "GPIB-Current");
  ~DigitalDisplayWithChart() = default;

  // Main render function
  void Render();

  // Configuration
  void SetSelectedChannel(const std::string& channelName);
  std::string GetSelectedChannel() const { return m_selectedDataName; }

  // Display controls
  void SetShowChart(bool show) { m_showChart = show; }
  bool IsShowChart() const { return m_showChart; }

  void SetShowDigital(bool show) { m_digitalDisplayEnabled = show; }
  bool IsShowDigital() const { return m_digitalDisplayEnabled; }

  // Chart settings
  void SetTimeWindow(float seconds) { m_timeWindow = seconds; }
  float GetTimeWindow() const { return m_timeWindow; }

  void SetAutoScaleY(bool autoScale) { m_autoScaleY = autoScale; }
  bool IsAutoScaleY() const { return m_autoScaleY; }

  void SetYRange(float min, float max) { m_yMin = min; m_yMax = max; }
  std::pair<float, float> GetYRange() const { return { m_yMin, m_yMax }; }

private:
  // Initialization functions
  void initializeUnitsMap();
  void initializeDisplayNameMap();
  void loadChannelsFromConfig();

  // Update and rendering functions
  void updateData();
  void handleRightClickMenu();
  void renderDigitalDisplay();
  void renderControls();
  void renderChart();

  // Helper functions
  std::string getDisplayName(const std::string& dataName);
  std::string getBaseUnit(const std::string& dataName);
  std::pair<float, std::string> getScaledValueAndUnit(float absValue);
  ImVec4 generateColor(const std::string& channelName);

  bool m_showDebug = false; // Add this line
};