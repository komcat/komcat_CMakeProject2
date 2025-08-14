// src/ui/services/data/DataMonitorService.h
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"
#include "implot/implot.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>

class DataMonitorService : public IUIService {
private:
  // Chart data management (similar to GlobalDataStoreViewerUI)
  std::map<std::string, std::deque<float>> m_channelData;
  std::map<std::string, std::deque<double>> m_channelTimestamps;
  std::map<std::string, std::string> m_channelConfigs; // Simple config storage
  std::map<std::string, std::string> m_channelUnits;   // Unit storage

  // Settings
  static const size_t MAX_DATA_POINTS = 100;
  static const float TIME_WINDOW; // 30 seconds

  // State tracking
  bool m_configLoaded = false;

  // Value change tracking (from GlobalDataStoreViewerUI)
  std::map<std::string, float> m_lastValues;

  // Refresh timer system (from GlobalDataStoreViewerUI)
  std::chrono::steady_clock::time_point m_lastRefreshTime;
  static const std::chrono::milliseconds REFRESH_INTERVAL; // 100ms refresh rate
  bool m_forceRefresh = false;

  // Data logging state (original DataMonitorService features)
  static inline bool isLogging = false;
  static inline int logInterval = 1; // seconds
  static inline std::string logFilename = "data_log.csv";

public:
  DataMonitorService();
  ~DataMonitorService() = default;

  // IUIService interface
  void RenderUI() override;
  std::string GetServiceName() const override { return "data_monitor"; }
  std::string GetDisplayName() const override { return "Data Monitor"; }
  std::string GetCategory() const override { return "Data"; }
  bool IsAvailable() const override { return true; }

private:
  // Core rendering methods (restructured from GlobalDataStoreViewerUI)
  void LoadChannelConfigurations();
  void UpdateChannelData();
  void RenderConnectionStatus();
  void RenderDataTable();
  void RenderMiniChart(const std::string& channel);
  void RenderControls();
  void RenderDataLogging();

  // Real-time data methods (from original DataMonitorService)
  void UpdateLiveData();
  void RenderSystemOverview();
  void RenderRealTimeCharts();

  // Formatting helpers (from GlobalDataStoreViewerUI)
  void FormatValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize);
  bool HasValueChanged(const std::string& channel, float currentValue);
  bool ShouldRefresh();

  // Data management helpers (enhanced from both sources)
  void InitializeChannelData();
  void AddChannelData(const std::string& channel, float value);
  void CleanupOldData(const std::string& channel, double currentTime);

  // Utility methods (from original DataMonitorService)
  float GetDataQuality() const;
  void ResizeDataBuffers(int newSize);
  void ExportChartData();
  void StartDataLogging();
  void StopDataLogging();
  void SaveCurrentData();
  void LoadHistoricalData();
  int GetLogRecordCount() const;
  float GetLogFileSize() const;
  float GetTimeToNextLog() const;

  // Demo data generation (enhanced)
  void GenerateTestData();
  void CreateDemoChannels();

  // Channel management
  std::vector<std::string> GetAvailableChannels() const;
  float GetChannelValue(const std::string& channel, float defaultValue = 0.0f) const;
  void SetChannelValue(const std::string& channel, float value);
  void RegisterChannel(const std::string& channel, const std::string& unit = "", const std::string& description = "");

  // Current values (from original, but expanded)
  std::map<std::string, float> m_currentValues;

  // Default demo channels
  struct DemoChannel {
    std::string name;
    std::string unit;
    std::string description;
    float baseValue;
    float amplitude;
    float frequency;
  };

  std::vector<DemoChannel> m_demoChannels;
};