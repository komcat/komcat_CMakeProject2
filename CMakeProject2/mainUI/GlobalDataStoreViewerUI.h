// GlobalDataStoreViewerUI.h
#pragma once

#include "include/data/global_data_store.h"
#include "include/data/data_client_manager.h"
#include "imgui.h"
#include "implot/implot.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>

class GlobalDataStoreViewerUI {
private:
  // Chart data management
  std::map<std::string, std::deque<float>> m_channelData;
  std::map<std::string, std::deque<double>> m_channelTimestamps;
  std::map<std::string, ServerConfig> m_serverConfigs;

  // Settings
  static const size_t MAX_DATA_POINTS = 100;
  static const float TIME_WINDOW; // 30 seconds

  // State
  bool m_configLoaded = false;
  DataClientManager* m_dataClientManager = nullptr;

  // Value change tracking
  std::map<std::string, float> m_lastValues;

  // ADD THESE NEW MEMBERS FOR REFRESH TIMER
  std::chrono::steady_clock::time_point m_lastRefreshTime;
  static const std::chrono::milliseconds REFRESH_INTERVAL; // 100ms refresh rate
  bool m_forceRefresh = false;

public:
  GlobalDataStoreViewerUI();
  ~GlobalDataStoreViewerUI() = default;

  // Main render function
  void Render();

  // Set the data client manager for configuration access
  void SetDataClientManager(DataClientManager* manager);

private:
  // Helper functions
  void LoadServerConfigurations();
  void UpdateChannelData();
  void RenderConnectionStatus();
  void RenderDataTable();
  void RenderControls();
  void RenderMiniChart(const std::string& channel);

  // Formatting helpers
  void FormatValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize);
  bool HasValueChanged(const std::string& channel, float currentValue);

  // ADD THIS NEW METHOD
  bool ShouldRefresh();
};