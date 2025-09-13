#pragma once

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <mutex>
#include "imgui.h"
#include "implot/implot.h"
#include <map>


// Forward declaration
class GlobalDataStore;

/// <summary>
/// LiveDataPlot - A reusable ImGui component for displaying live data from a single channel
/// in the GlobalDataStore as a real-time scrolling plot.
/// </summary>
class LiveDataPlot {
public:
  /// <summary>
  /// Configuration structure for the live data plot
  /// </summary>
  struct Config {
    std::string channelName;           // Name of the channel to display
    int historySize = 500;             // Number of data points to keep in history
    float timeWindow = 10.0f;          // Time window to display (seconds)
    bool autoScale = true;             // Auto-scale Y axis
    float minValue = -1.0f;           // Min Y value (if not auto-scaling)
    float maxValue = 1.0f;            // Max Y value (if not auto-scaling)
    bool showGrid = true;             // Show grid lines
    bool showLegend = true;           // Show legend
    bool showCurrentValue = true;     // Show current value overlay
    ImVec4 lineColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Line color (green)
    float lineThickness = 2.0f;       // Line thickness
    std::string yAxisLabel = "";      // Y-axis label (auto-detected if empty)
    bool enableSpec = false;          // Enable spec line
    float specValue = 0.0f;          // Spec value to display
    bool enableChannelSelector = true; // Enable channel selector popup
  };

  /// <summary>
  /// Data point structure with timestamp
  /// </summary>
  struct DataPoint {
    float value;
    double timestamp;  // Relative time in seconds from start

    DataPoint() : value(0.0f), timestamp(0.0) {}
    DataPoint(float v, double t) : value(v), timestamp(t) {}
  };

public:
  /// <summary>
  /// Constructor
  /// </summary>
  /// <param name="id">Unique identifier for this plot instance</param>
  LiveDataPlot(const std::string& id);

  /// <summary>
  /// Destructor
  /// </summary>
  ~LiveDataPlot();

  /// <summary>
  /// Initialize the plot with configuration
  /// </summary>
  /// <param name="config">Configuration settings</param>
  void Initialize(const Config& config);

  /// <summary>
  /// Render the plot
  /// </summary>
  /// <param name="size">Size of the plot area (-1 for auto)</param>
  void Render(const ImVec2& size = ImVec2(-1, -1));

  /// <summary>
  /// Update configuration on the fly
  /// </summary>
  /// <param name="config">New configuration</param>
  void UpdateConfig(const Config& config);

  /// <summary>
  /// Set the channel to monitor
  /// </summary>
  /// <param name="channelName">Name of the channel</param>
  void SetChannel(const std::string& channelName);

  /// <summary>
  /// Set spec value
  /// </summary>
  /// <param name="value">Spec value</param>
  /// <param name="enabled">Whether to show spec line</param>
  void SetSpec(float value, bool enabled = true);

  /// <summary>
  /// Clear all data history
  /// </summary>
  void Clear();

  /// <summary>
  /// Get current value
  /// </summary>
  /// <returns>Current value from the channel</returns>
  float GetCurrentValue() const { return m_currentValue; }

  /// <summary>
  /// Check if channel is available
  /// </summary>
  /// <returns>True if channel exists in GlobalDataStore</returns>
  bool IsChannelAvailable() const;

  /// <summary>
  /// Set Y-axis range manually
  /// </summary>
  /// <param name="min">Minimum value</param>
  /// <param name="max">Maximum value</param>
  void SetYRange(float min, float max);

  /// <summary>
  /// Enable/disable auto-scaling
  /// </summary>
  /// <param name="enable">True to enable auto-scaling</param>
  void SetAutoScale(bool enable);

  /// <summary>
  /// Enable/disable channel selector popup
  /// </summary>
  /// <param name="enable">True to enable channel selector</param>
  void SetChannelSelectorEnabled(bool enable);

private:
  /// <summary>
  /// Update data from GlobalDataStore
  /// </summary>
  void UpdateData();

  /// <summary>
  /// Format value with appropriate units
  /// </summary>
  std::string FormatValue(float value) const;

  /// <summary>
  /// Auto-detect units from channel name
  /// </summary>
  std::string DetectUnit() const;

  /// <summary>
  /// Calculate Y-axis range for auto-scaling
  /// </summary>
  void CalculateAutoScale();

  /// <summary>
  /// Render the channel selector popup
  /// </summary>
  void RenderChannelSelector();

  /// <summary>
  /// Render the current value display with clickable functionality
  /// </summary>
  void RenderCurrentValueDisplay();

  /// <summary>
  /// Get available channels from GlobalDataStore
  /// </summary>
  std::vector<std::string> GetAvailableChannels() const;

private:
  std::string m_id;                              // Unique plot identifier
  Config m_config;                               // Current configuration
  std::deque<DataPoint> m_dataHistory;          // Historical data points
  float m_currentValue;                          // Current value
  std::string m_detectedUnit;                   // Auto-detected unit

  // Timing
  std::chrono::steady_clock::time_point m_startTime;
  std::chrono::steady_clock::time_point m_lastUpdateTime;

  // Auto-scaling
  float m_autoMinY;
  float m_autoMaxY;
  bool m_needsAutoScale;

  // Channel selector state
  bool m_showChannelSelector;
  std::vector<std::string> m_availableChannels;
  char m_channelFilter[256];                     // Filter text for channel search (char buffer for ImGui)
  int m_selectedChannelIndex;                    // Currently selected channel in popup

  // Thread safety
  mutable std::mutex m_dataMutex;

  // Plot state
  bool m_initialized;
  ImPlotContext* m_plotContext;

  // Helper buffers for ImGui IDs
  mutable char m_tempIdBuffer[128];
};

/// <summary>
/// Helper class to manage multiple LiveDataPlot instances
/// </summary>
class LiveDataPlotManager {
public:
  static LiveDataPlotManager* GetInstance();

  /// <summary>
  /// Create or get a plot instance
  /// </summary>
  LiveDataPlot* GetPlot(const std::string& id);

  /// <summary>
  /// Remove a plot instance
  /// </summary>
  void RemovePlot(const std::string& id);

  /// <summary>
  /// Clear all plots
  /// </summary>
  void ClearAll();

private:
  LiveDataPlotManager() = default;
  std::map<std::string, std::unique_ptr<LiveDataPlot>> m_plots;
  std::mutex m_managerMutex;
};