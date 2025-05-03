// DataChartManager.h
#pragma once

#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include "imgui.h" // Include ImGui for ImVec4
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <mutex>
// Structure to define a channel to monitor
struct ChannelInfo {
  std::string id;           // Channel identifier in GlobalDataStore
  std::string displayName;  // Display name for the UI
  std::string unit;         // Unit for display
  bool displayUnitSuffix;   // Whether to display the unit suffix
  ImVec4 color;             // Color for the chart line
};
// Forward declaration for ImPlot (avoid including implot.h in header)
struct ImPlotContext;

// Structure to hold chart data for each data source
struct ChartDataBuffer {
  std::string serverId;                // Server identifier
  std::string displayName;             // Display name for the chart
  std::string unit;                    // Unit for display
  bool displayUnitSuffix;              // Whether to display the unit suffix
  std::deque<float> values;            // Circular buffer of values
  std::deque<double> timestamps;       // Circular buffer of timestamps
  ImVec4 color;                        // Line color

  // Default constructor needed for std::map::emplace
  ChartDataBuffer() : displayUnitSuffix(false), color(0, 0, 0, 1) {}

  // Constructor with proper parameter passing
  ChartDataBuffer(const std::string& id, const std::string& name, const std::string& unitStr,
    bool showUnitSuffix, const ImVec4& lineColor);
};

// Class to manage data charts
class DataChartManager : public ITogglableUI {
public:
  // Constructor and destructor - no longer requires DataClientManager
  DataChartManager();
  ~DataChartManager();

  // Initialize chart data
  void Initialize();

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
  void DataChartManager::AddChannel(const std::string& id, const std::string& displayName,
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
  std::mutex m_dataMutex; // For thread safety
};

// Helper function to create a toggleable UI adapter for the chart manager
std::shared_ptr<ITogglableUI> CreateDataChartManagerUI(DataChartManager& manager);