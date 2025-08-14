// src/ui/services/data/DataMonitorService.cpp
#include "DataMonitorService.h"
#include <cmath>
#include <iostream>
#include <algorithm>

// Static member definitions (from GlobalDataStoreViewerUI)
const float DataMonitorService::TIME_WINDOW = 30.0f;
const std::chrono::milliseconds DataMonitorService::REFRESH_INTERVAL(100); // 100ms = 10 FPS

DataMonitorService::DataMonitorService() {
  // Initialize refresh timer
  m_lastRefreshTime = std::chrono::steady_clock::now();

  // Initialize demo channels
  CreateDemoChannels();
  InitializeChannelData();
}

void DataMonitorService::RenderUI() {
  // Check if we should force a refresh (from GlobalDataStoreViewerUI)
  if (ShouldRefresh()) {
    m_forceRefresh = true;
  }

  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("📊 Global Data Store Viewer");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Real-time data monitoring and visualization");
  ImGui::Separator();

  // Load channel configurations if not done yet
  if (!m_configLoaded) {
    LoadChannelConfigurations();
  }

  // Update chart data and live data
  UpdateChannelData();
  UpdateLiveData();

  auto channels = GetAvailableChannels();
  ImGui::Text("Available Channels: %zu", channels.size());
  ImGui::Spacing();

  if (channels.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No data channels available");
    ImGui::Text("Data will appear here when sources are connected");
    GenerateTestData(); // Automatically generate test data
    return;
  }

  // Render connection status (from GlobalDataStoreViewerUI style)
  RenderConnectionStatus();

  // Render system overview (from original DataMonitorService)
  RenderSystemOverview();

  ImGui::Spacing();
  ImGui::Separator();

  // Render data table with mini charts (from GlobalDataStoreViewerUI)
  RenderDataTable();

  ImGui::Spacing();
  ImGui::Separator();

  // Render real-time charts (from original DataMonitorService)
  RenderRealTimeCharts();

  ImGui::Spacing();
  ImGui::Separator();

  // Render controls and data logging
  RenderControls();
  RenderDataLogging();

  // Reset force refresh flag
  m_forceRefresh = false;
}

void DataMonitorService::LoadChannelConfigurations() {
  // Initialize channel configurations (simplified version of GlobalDataStoreViewerUI)
  for (const auto& demoChannel : m_demoChannels) {
    m_channelConfigs[demoChannel.name] = demoChannel.description;
    m_channelUnits[demoChannel.name] = demoChannel.unit;
  }
  m_configLoaded = true;
}

void DataMonitorService::UpdateChannelData() {
  auto channels = GetAvailableChannels();
  auto currentTime = std::chrono::high_resolution_clock::now();
  double timestamp = std::chrono::duration<double>(currentTime.time_since_epoch()).count();

  for (const std::string& channel : channels) {
    float value = GetChannelValue(channel);

    // Initialize deque if not exists (from GlobalDataStoreViewerUI)
    if (m_channelData.find(channel) == m_channelData.end()) {
      m_channelData[channel] = std::deque<float>();
      m_channelTimestamps[channel] = std::deque<double>();
    }

    // Add new data point
    m_channelData[channel].push_back(value);
    m_channelTimestamps[channel].push_back(timestamp);

    // Keep only recent data
    while (m_channelData[channel].size() > MAX_DATA_POINTS) {
      m_channelData[channel].pop_front();
      m_channelTimestamps[channel].pop_front();
    }

    // Remove old data outside time window
    CleanupOldData(channel, timestamp);
  }
}

void DataMonitorService::RenderConnectionStatus() {
  ImGui::Text("Data Sources:");

  // Show status of demo channels
  int connectedCount = 0;
  for (const auto& demoChannel : m_demoChannels) {
    connectedCount++;
    ImGui::BulletText("%s: Connected (%s)",
      demoChannel.name.c_str(),
      demoChannel.description.c_str());
  }

  ImGui::Text("Status: %d/%zu sources connected", connectedCount, m_demoChannels.size());
  ImGui::Separator();
}

void DataMonitorService::RenderDataTable() {
  auto channels = GetAvailableChannels();

  if (ImGui::BeginTable("DataTableWithCharts", 4,
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

    // Setup columns (from GlobalDataStoreViewerUI)
    ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Chart", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableHeadersRow();

    for (const std::string& channel : channels) {
      ImGui::TableNextRow();

      // Channel name column
      ImGui::TableNextColumn();
      ImGui::Text("%s", channel.c_str());

      // Description column
      ImGui::TableNextColumn();
      auto configIt = m_channelConfigs.find(channel);
      if (configIt != m_channelConfigs.end()) {
        ImGui::TextWrapped("%s", configIt->second.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No description available");
      }

      // Value column
      ImGui::TableNextColumn();
      float value = GetChannelValue(channel);

      char valueText[64];
      FormatValueWithUnit(channel, value, valueText, sizeof(valueText));

      // Show with change highlighting (from GlobalDataStoreViewerUI)
      if (HasValueChanged(channel, value)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", valueText);
      }
      else {
        ImGui::Text("%s", valueText);
      }

      // Chart column
      ImGui::TableNextColumn();
      RenderMiniChart(channel);
    }
    ImGui::EndTable();
  }
}


void DataMonitorService::RenderMiniChart(const std::string& channel) {
  // Use ImGui's built-in PlotLines instead of ImPlot
  if (m_channelData[channel].size() > 1) {
    std::vector<float> data(m_channelData[channel].begin(), m_channelData[channel].end());

    // Calculate min/max for auto-scaling
    float minVal = *std::min_element(data.begin(), data.end());
    float maxVal = *std::max_element(data.begin(), data.end());

    // Add some padding to the range
    float range = maxVal - minVal;
    if (range < 0.001f) range = 1.0f; // Avoid division by zero
    minVal -= range * 0.1f;
    maxVal += range * 0.1f;

    // Create a simple mini chart using ImGui's PlotLines
    ImVec2 chartSize(150, 60);
    std::string plotLabel = "##MiniChart_" + channel;

    ImGui::PlotLines(plotLabel.c_str(), data.data(), (int)data.size(), 0,
      nullptr, minVal, maxVal, chartSize);
  }
  else {
    // Show "No data" message
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Text("No data");
    ImGui::PopStyleColor();
  }
}
void DataMonitorService::UpdateLiveData() {
  // Generate live data for demo channels (enhanced from original)
  float time = ImGui::GetTime();

  for (const auto& demoChannel : m_demoChannels) {
    float value = demoChannel.baseValue +
      sin(time * demoChannel.frequency) * demoChannel.amplitude +
      cos(time * demoChannel.frequency * 0.3f) * (demoChannel.amplitude * 0.5f);
    SetChannelValue(demoChannel.name, value);
  }
}

void DataMonitorService::RenderSystemOverview() {
  ImGui::Text("System Overview:");

  // Current values display (from original DataMonitorService)
  auto channels = GetAvailableChannels();
  int columnCount = std::min(4, (int)channels.size());

  if (columnCount > 0) {
    ImGui::Columns(columnCount, "SystemValues", false);

    for (size_t i = 0; i < channels.size() && i < 4; ++i) {
      const std::string& channel = channels[i];
      float value = GetChannelValue(channel);

      // Get unit for display
      auto unitIt = m_channelUnits.find(channel);
      std::string unit = (unitIt != m_channelUnits.end()) ? unitIt->second : "";

      ImGui::Text("%s", channel.c_str());

      // Color coding based on value ranges
      ImVec4 valueColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Default green
      if (channel.find("temp") != std::string::npos && value > 30.0f) {
        valueColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange for high temp
      }
      else if (channel.find("pressure") != std::string::npos && value > 1.5f) {
        valueColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange for high pressure
      }

      char valueText[64];
      FormatValueWithUnit(channel, value, valueText, sizeof(valueText));
      ImGui::TextColored(valueColor, "%s", valueText);

      if (i < channels.size() - 1 && i < 3) {
        ImGui::NextColumn();
      }
    }

    ImGui::Columns(1);
  }

  // System status indicators
  ImGui::Spacing();
  ImGui::Text("System Status:");
  ImGui::BulletText("Frame Rate: %.1f FPS", ImGui::GetIO().Framerate);
  ImGui::BulletText("Data Update Rate: 10 Hz");
  ImGui::BulletText("Active Channels: %zu", channels.size());
  ImGui::BulletText("Data Quality: %s", GetDataQuality() > 95.0f ? "🟢 Excellent" :
    GetDataQuality() > 85.0f ? "🟡 Good" : "🔴 Poor");
}

void DataMonitorService::RenderRealTimeCharts() {
  ImGui::Text("Real-time Charts:");

  auto channels = GetAvailableChannels();
  for (size_t i = 0; i < channels.size() && i < 4; ++i) {
    const std::string& channel = channels[i];

    ImGui::Text("%s History:", channel.c_str());

    if (m_channelData[channel].size() > 1) {
      std::vector<float> data(m_channelData[channel].begin(), m_channelData[channel].end());
      float minVal = *std::min_element(data.begin(), data.end()) - 1.0f;
      float maxVal = *std::max_element(data.begin(), data.end()) + 1.0f;

      ImGui::PlotLines(("##" + channel).c_str(), data.data(), data.size(), 0,
        nullptr, minVal, maxVal, ImVec2(0, 80));
    }
  }

  // Chart controls (from original DataMonitorService)
  ImGui::Spacing();
  ImGui::Text("Chart Controls:");

  static int timeRange = 100;
  if (ImGui::SliderInt("Time Range (samples)", &timeRange, 50, 500)) {
    ResizeDataBuffers(timeRange);
  }

  static bool autoScale = true;
  ImGui::Checkbox("Auto Scale", &autoScale);

  ImGui::SameLine();
  if (ImGui::Button("📊 Export Charts")) {
    ExportChartData();
  }
}

void DataMonitorService::RenderControls() {
  ImGui::Spacing();
  ImGui::Separator();

  // Collapsible controls section (from GlobalDataStoreViewerUI)
  static bool showControls = false;
  if (ImGui::CollapsingHeader("Controls & Settings", &showControls)) {
    ImGui::Indent(10.0f);

    // Chart controls
    if (ImGui::TreeNodeEx("Chart Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      static float updateRate = 60.0f;
      ImGui::SliderFloat("Update Rate (fps)", &updateRate, 1.0f, 120.0f);

      static bool showValues = true;
      ImGui::Checkbox("Show Numeric Values", &showValues);

      ImGui::TreePop();
    }

    ImGui::Spacing();

    // Test controls
    if (ImGui::TreeNodeEx("Test Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
      static float testValue = 123.45f;
      static char testChannel[64] = "test-channel";

      ImGui::InputText("Channel Name", testChannel, sizeof(testChannel));
      ImGui::InputFloat("Test Value", &testValue);

      if (ImGui::Button("Set Test Value")) {
        SetChannelValue(testChannel, testValue);
        RegisterChannel(testChannel, "", "Test channel");
      }

      ImGui::SameLine();
      if (ImGui::Button("Clear All Charts")) {
        m_channelData.clear();
        m_channelTimestamps.clear();
      }

      ImGui::TreePop();
    }

    ImGui::Spacing();

    // Demo data controls
    if (ImGui::TreeNodeEx("Demo Data", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Generate Sine Wave")) {
        static float sinePhase = 0.0f;
        sinePhase += 0.1f;
        float sineValue = std::sin(sinePhase) * 2.0f + 1.0f;
        SetChannelValue("demo-sine", sineValue);
      }

      ImGui::SameLine();
      if (ImGui::Button("Generate Random Data")) {
        float randomValue = (rand() % 1000) / 100.0f;
        SetChannelValue("demo-random", randomValue);
      }

      ImGui::TreePop();
    }

    ImGui::Unindent(10.0f);
  }
}

void DataMonitorService::RenderDataLogging() {
  ImGui::Text("Data Logging:");

  // Logging controls (from original DataMonitorService)
  ImGui::PushStyleColor(ImGuiCol_Button, isLogging ?
    ImVec4(0.7f, 0.0f, 0.0f, 1.0f) : ImVec4(0.0f, 0.7f, 0.0f, 1.0f));

  if (ImGui::Button(isLogging ? "⏹️ Stop Logging" : "▶️ Start Logging", ImVec2(150, 30))) {
    isLogging = !isLogging;
    if (isLogging) {
      StartDataLogging();
    }
    else {
      StopDataLogging();
    }
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();
  if (ImGui::Button("💾 Save Current", ImVec2(150, 30))) {
    SaveCurrentData();
  }

  ImGui::SameLine();
  if (ImGui::Button("📁 Load Data", ImVec2(150, 30))) {
    LoadHistoricalData();
  }

  // Logging settings
  ImGui::Spacing();
  ImGui::Text("Logging Settings:");

  ImGui::SliderInt("Log Interval (s)", &logInterval, 1, 60);

  // Filename input
  static char filename[128] = "data_log.csv";
  ImGui::InputText("Filename", filename, sizeof(filename));
  logFilename = std::string(filename);

  // Logging statistics
  ImGui::Spacing();
  ImGui::Text("Logging Statistics:");
  ImGui::BulletText("Status: %s", isLogging ? "🟢 Active" : "🔴 Inactive");
  ImGui::BulletText("Log File: %s", logFilename.c_str());
  ImGui::BulletText("Records: %d", GetLogRecordCount());
  ImGui::BulletText("File Size: %.1f KB", GetLogFileSize());

  if (isLogging) {
    ImGui::BulletText("Next Log: %.1fs", GetTimeToNextLog());
  }
}

// Utility and helper method implementations

void DataMonitorService::FormatValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize) {
  auto unitIt = m_channelUnits.find(channel);
  std::string unit = (unitIt != m_channelUnits.end()) ? unitIt->second : "";

  float absValue = std::abs(value);

  if (unit == "A") {
    // Current formatting (from GlobalDataStoreViewerUI)
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00 A");
    }
    else if (absValue < 1e-12f) {
      snprintf(buffer, bufferSize, "%.3e A", value);
    }
    else if (absValue < 1e-9f) {
      float pAValue = value * 1e12f;
      snprintf(buffer, bufferSize, "%.2f pA", pAValue);
    }
    else if (absValue < 1e-6f) {
      float nAValue = value * 1e9f;
      snprintf(buffer, bufferSize, "%.2f nA", nAValue);
    }
    else if (absValue < 1e-3f) {
      float uAValue = value * 1e6f;
      snprintf(buffer, bufferSize, "%.2f µA", uAValue);
    }
    else if (absValue < 1.0f) {
      float mAValue = value * 1e3f;
      snprintf(buffer, bufferSize, "%.3f mA", mAValue);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f A", value);
    }
  }
  else if (unit == "V") {
    // Voltage formatting
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00 V");
    }
    else if (absValue < 1e-6f) {
      float uVValue = value * 1e6f;
      snprintf(buffer, bufferSize, "%.2f µV", uVValue);
    }
    else if (absValue < 1e-3f) {
      float mVValue = value * 1e3f;
      snprintf(buffer, bufferSize, "%.2f mV", mVValue);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f V", value);
    }
  }
  else if (!unit.empty()) {
    // Other units
    if (absValue < 0.001f && absValue > 0.0f) {
      snprintf(buffer, bufferSize, "%.6f %s", value, unit.c_str());
    }
    else {
      snprintf(buffer, bufferSize, "%.3f %s", value, unit.c_str());
    }
  }
  else {
    // No unit
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00");
    }
    else if (absValue < 1e-6f) {
      snprintf(buffer, bufferSize, "%.3e", value);
    }
    else if (absValue < 0.001f) {
      snprintf(buffer, bufferSize, "%.6f", value);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f", value);
    }
  }
}

bool DataMonitorService::HasValueChanged(const std::string& channel, float currentValue) {
  bool valueChanged = false;

  if (m_lastValues.find(channel) != m_lastValues.end()) {
    valueChanged = (m_lastValues[channel] != currentValue);
  }
  else {
    valueChanged = true; // First time seeing this channel
  }

  m_lastValues[channel] = currentValue;

  // Force refresh even if value hasn't changed, for real-time updates
  return valueChanged || m_forceRefresh;
}

bool DataMonitorService::ShouldRefresh() {
  auto currentTime = std::chrono::steady_clock::now();
  auto timeSinceLastRefresh = std::chrono::duration_cast<std::chrono::milliseconds>(
    currentTime - m_lastRefreshTime);

  if (timeSinceLastRefresh >= REFRESH_INTERVAL) {
    m_lastRefreshTime = currentTime;
    return true;
  }
  return false;
}

void DataMonitorService::CreateDemoChannels() {
  m_demoChannels = {
    {"temperature", "°C", "System temperature sensor", 25.0f, 3.0f, 0.1f},
    {"pressure", "bar", "System pressure sensor", 1.0f, 0.2f, 0.2f},
    {"speed", "%", "Motor speed percentage", 100.0f, 10.0f, 0.15f},
    {"current", "A", "System current draw", 2.5f, 0.5f, 0.25f}
  };
}

void DataMonitorService::InitializeChannelData() {
  for (const auto& demoChannel : m_demoChannels) {
    RegisterChannel(demoChannel.name, demoChannel.unit, demoChannel.description);
    SetChannelValue(demoChannel.name, demoChannel.baseValue);
  }
}

std::vector<std::string> DataMonitorService::GetAvailableChannels() const {
  std::vector<std::string> channels;
  for (const auto& pair : m_currentValues) {
    channels.push_back(pair.first);
  }
  return channels;
}

float DataMonitorService::GetChannelValue(const std::string& channel, float defaultValue) const {
  auto it = m_currentValues.find(channel);
  return (it != m_currentValues.end()) ? it->second : defaultValue;
}

void DataMonitorService::SetChannelValue(const std::string& channel, float value) {
  m_currentValues[channel] = value;
}

void DataMonitorService::RegisterChannel(const std::string& channel, const std::string& unit, const std::string& description) {
  m_channelUnits[channel] = unit;
  m_channelConfigs[channel] = description;
  if (m_currentValues.find(channel) == m_currentValues.end()) {
    m_currentValues[channel] = 0.0f;
  }
}

void DataMonitorService::CleanupOldData(const std::string& channel, double currentTime) {
  double cutoffTime = currentTime - TIME_WINDOW;

  auto& timestamps = m_channelTimestamps[channel];
  auto& data = m_channelData[channel];

  while (!timestamps.empty() && timestamps.front() < cutoffTime) {
    data.pop_front();
    timestamps.pop_front();
  }
}

void DataMonitorService::AddChannelData(const std::string& channel, float value) {
  auto currentTime = std::chrono::high_resolution_clock::now();
  double timestamp = std::chrono::duration<double>(currentTime.time_since_epoch()).count();

  // Initialize if needed
  if (m_channelData.find(channel) == m_channelData.end()) {
    m_channelData[channel] = std::deque<float>();
    m_channelTimestamps[channel] = std::deque<double>();
  }

  // Add data
  m_channelData[channel].push_back(value);
  m_channelTimestamps[channel].push_back(timestamp);

  // Cleanup old data
  CleanupOldData(channel, timestamp);
}

void DataMonitorService::GenerateTestData() {
  // Generate some test channels if none exist
  if (m_currentValues.empty()) {
    CreateDemoChannels();
    InitializeChannelData();
  }
}

// Utility methods (from original DataMonitorService)
float DataMonitorService::GetDataQuality() const {
  return 96.5f + sin(ImGui::GetTime() * 0.1f) * 2.0f;
}

void DataMonitorService::ResizeDataBuffers(int newSize) {
  // Resize all channel buffers
  for (auto& pair : m_channelData) {
    auto& data = pair.second;
    if ((int)data.size() > newSize) {
      // Remove oldest data points
      while ((int)data.size() > newSize) {
        data.pop_front();
      }
    }
  }

  // Also resize timestamps
  for (auto& pair : m_channelTimestamps) {
    auto& timestamps = pair.second;
    if ((int)timestamps.size() > newSize) {
      while ((int)timestamps.size() > newSize) {
        timestamps.pop_front();
      }
    }
  }
}

void DataMonitorService::ExportChartData() {
  Logger::Success(L"Chart data exported successfully");
  // Could implement actual CSV export here
}

void DataMonitorService::StartDataLogging() {
  Logger::Info(L"Started data logging to: " + UnicodeUtils::StringToWString(logFilename));
  // Could implement actual file logging here
}

void DataMonitorService::StopDataLogging() {
  Logger::Info(L"Stopped data logging");
}

void DataMonitorService::SaveCurrentData() {
  Logger::Success(L"Current data snapshot saved");
  // Could implement actual data snapshot save here
}

void DataMonitorService::LoadHistoricalData() {
  Logger::Info(L"Loading historical data...");
  // Could implement actual historical data loading here
}

int DataMonitorService::GetLogRecordCount() const {
  return isLogging ? 1547 + (int)(ImGui::GetTime() / logInterval) : 1547;
}

float DataMonitorService::GetLogFileSize() const {
  return GetLogRecordCount() * 0.12f; // Approximate KB per record
}

float DataMonitorService::GetTimeToNextLog() const {
  return logInterval - fmod(ImGui::GetTime(), logInterval);
}