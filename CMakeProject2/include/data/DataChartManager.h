// DataChartManager.h
#pragma once

#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include "imgui.h" // Include ImGui for ImVec4
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp> // Add JSON support

// Structure to define a channel to monitor
struct ChannelInfo {
  std::string id;           // Channel identifier in GlobalDataStore
  std::string displayName;  // Display name for the UI
  std::string unit;         // Unit for display
  bool displayUnitSuffix;   // Whether to display the unit suffix
  bool enable;              // Whether the channel is enabled
  ImVec4 color;             // Color for the chart line
};

// Forward declaration for ImPlot (avoid including implot.h in header)
struct ImPlotContext;

// In DataChartManager.h, update the ChartDataBuffer struct:
struct ChartDataBuffer {
  std::string serverId;                // Server identifier
  std::string displayName;             // Display name for the chart
  std::string unit;                    // Unit for display
  bool displayUnitSuffix;              // Whether to display the unit suffix
  std::deque<float> values;            // Circular buffer of values
  std::deque<double> timestamps;       // Circular buffer of timestamps
  ImVec4 color;                        // Line color
  bool visible;                        // Whether the channel is visible in UI
  bool enabled;                        // Whether the channel is enabled for data collection

  // Default constructor with visible and enabled set to true
  ChartDataBuffer() : displayUnitSuffix(false), color(0, 0, 0, 1), visible(true), enabled(true) {}

  // Updated constructor that includes visibility and enabled state
  ChartDataBuffer(const std::string& id, const std::string& name, const std::string& unitStr,
    bool showUnitSuffix, const ImVec4& lineColor, bool isEnabled = true)
    : serverId(id), displayName(name), unit(unitStr),
    displayUnitSuffix(showUnitSuffix), color(lineColor), visible(true), enabled(isEnabled) {
  }
};

// Class to manage data charts
class DataChartManager : public ITogglableUI {
public:
  // Constructor and destructor - no longer requires DataClientManager
  DataChartManager();
  // Constructor that takes a config file path
  DataChartManager(const std::string& configFilePath);
  ~DataChartManager();

  // Initialize chart data
  void Initialize();

  // Load channels from config file
  bool LoadConfig(const std::string& configFilePath);

  // Update chart data from data sources
  void Update();

  // Render UI for charts
  void RenderUI();

  // Toggle window visibility - implements ITogglableUI
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Settings
  void SetMaxPoints(int maxPoints) { m_maxPoints = maxPoints; }
  void SetTimeWindow(float seconds) { m_timeWindow = seconds; }
  void AddChannel(const std::string& id, const std::string& displayName,
    const std::string& unit, bool displayUnitSuffix);
private:
  // Helper methods
  std::string FormatWithSIPrefix(float value, const std::string& unit, bool displayUnitSuffix) const;
  void InitializeColors();

  // ImPlot initialization and cleanup
  void InitializeImPlot();
  void ShutdownImPlot();

  // Data members
  std::map<std::string, ChartDataBuffer> m_chartBuffers;
  std::vector<ImVec4> m_colors;
  bool m_initialized;
  bool m_implotInitialized;
  int m_maxPoints;
  float m_timeWindow;
  bool m_showWindow;
  std::string m_windowTitle;
  std::string m_configFilePath;  // Added for config file path
  std::mutex m_dataMutex; // For thread safety
};

// Helper function to create a toggleable UI adapter for the chart manager
std::shared_ptr<ITogglableUI> CreateDataChartManagerUI(DataChartManager& manager);