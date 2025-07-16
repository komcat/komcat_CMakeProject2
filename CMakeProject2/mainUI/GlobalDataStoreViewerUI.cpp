// GlobalDataStoreViewerUI.cpp
#include "GlobalDataStoreViewerUI.h"
#include <cmath>
#include <iostream>

// Static member definitions
const float GlobalDataStoreViewerUI::TIME_WINDOW = 30.0f;
const std::chrono::milliseconds GlobalDataStoreViewerUI::REFRESH_INTERVAL(100); // 100ms = 10 FPS

GlobalDataStoreViewerUI::GlobalDataStoreViewerUI() {
  // Initialize refresh timer
  m_lastRefreshTime = std::chrono::steady_clock::now();
}


// MODIFY the Render() method - add refresh check at the beginning:
void GlobalDataStoreViewerUI::Render() {
  // Check if we should force a refresh
  if (ShouldRefresh()) {
    m_forceRefresh = true;
  }

  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Global Data Store Viewer");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Real-time data monitoring and visualization");
  ImGui::Separator();

  // Load server configurations if not done yet
  if (!m_configLoaded && m_dataClientManager) {
    LoadServerConfigurations();
  }

  // Update chart data
  UpdateChannelData();

  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Global Data Store not available");
    return;
  }

  auto channels = dataStore->GetAvailableChannels();
  ImGui::Text("Available Channels: %zu", channels.size());
  ImGui::Spacing();

  if (channels.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No data channels available");
    ImGui::Text("Data will appear here when sources are connected");
    return;
  }

  // Render connection status
  RenderConnectionStatus();

  // Render data table with charts
  RenderDataTable();

  // Render controls
  RenderControls();

  // Reset force refresh flag
  m_forceRefresh = false;
}

void GlobalDataStoreViewerUI::SetDataClientManager(DataClientManager* manager) {
  m_dataClientManager = manager;
  m_configLoaded = false; // Force reload of configurations
}

void GlobalDataStoreViewerUI::LoadServerConfigurations() {
  if (!m_dataClientManager) return;

  m_serverConfigs.clear();
  size_t clientCount = m_dataClientManager->GetClientCount();

  for (size_t i = 0; i < clientCount; ++i) {
    auto& clientInfo = m_dataClientManager->GetClientInfo(static_cast<int>(i));
    m_serverConfigs[clientInfo.config.id] = clientInfo.config;
  }

  m_configLoaded = true;
}

void GlobalDataStoreViewerUI::UpdateChannelData() {
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) return;

  auto channels = dataStore->GetAvailableChannels();
  auto currentTime = std::chrono::high_resolution_clock::now();
  double timestamp = std::chrono::duration<double>(currentTime.time_since_epoch()).count();

  for (const std::string& channel : channels) {
    float value = dataStore->GetValue(channel);

    // Initialize deque if not exists
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
    double cutoffTime = timestamp - TIME_WINDOW;
    while (!m_channelTimestamps[channel].empty() &&
      m_channelTimestamps[channel].front() < cutoffTime) {
      m_channelData[channel].pop_front();
      m_channelTimestamps[channel].pop_front();
    }
  }
}

void GlobalDataStoreViewerUI::RenderConnectionStatus() {
  if (!m_dataClientManager) return;

  ImGui::Text("Data Sources:");
  size_t clientCount = m_dataClientManager->GetClientCount();
  int connectedCount = 0;

  for (size_t i = 0; i < clientCount; ++i) {
    auto& clientInfo = m_dataClientManager->GetClientInfo(static_cast<int>(i));
    if (clientInfo.connected) {
      connectedCount++;
      ImGui::BulletText("%s: Connected (%s)",
        clientInfo.config.name.c_str(),
        clientInfo.config.description.c_str());
    }
    else {
      ImGui::BulletText("%s: Disconnected", clientInfo.config.name.c_str());
    }
  }

  ImGui::Text("Status: %d/%zu sources connected", connectedCount, clientCount);
  ImGui::Separator();
}

void GlobalDataStoreViewerUI::RenderDataTable() {
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  auto channels = dataStore->GetAvailableChannels();

  if (ImGui::BeginTable("DataTableWithCharts", 4,
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

    // Setup columns
    ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Chart", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableHeadersRow();

    for (const std::string& channel : channels) {
      ImGui::TableNextRow();

      // Channel name column
      ImGui::TableNextColumn();
      ServerConfig* config = nullptr;
      if (m_serverConfigs.find(channel) != m_serverConfigs.end()) {
        config = &m_serverConfigs[channel];
      }

      if (config) {
        ImGui::Text("%s", config->name.c_str());
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%s)", channel.c_str());
      }
      else {
        ImGui::Text("%s", channel.c_str());
      }

      // Description column
      ImGui::TableNextColumn();
      if (config) {
        ImGui::TextWrapped("%s", config->description.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No config available");
      }

      // Value column
      ImGui::TableNextColumn();
      float value = dataStore->GetValue(channel, 0.0f);

      char valueText[64];
      FormatValueWithUnit(channel, value, valueText, sizeof(valueText));

      // Show with change highlighting
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

void GlobalDataStoreViewerUI::RenderMiniChart(const std::string& channel) {
  std::string plotId = "##MiniChart_" + channel;

  if (m_channelData[channel].size() > 1) {
    ImVec2 chartSize(150, 60);

    if (ImPlot::BeginPlot(plotId.c_str(), chartSize,
      ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {

      ImPlot::SetupAxes("", "",
        ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_AutoFit,
        ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_AutoFit);

      // Convert data to vectors for plotting
      std::vector<float> xData, yData;
      for (size_t i = 0; i < m_channelData[channel].size(); ++i) {
        double relativeTime = m_channelTimestamps[channel][i] - m_channelTimestamps[channel].front();
        xData.push_back(static_cast<float>(relativeTime));
        yData.push_back(m_channelData[channel][i]);
      }

      ImVec4 lineColor = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);
      ImPlot::PushStyleColor(ImPlotCol_Line, lineColor);
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f);

      ImPlot::PlotLine("##data", xData.data(), yData.data(), (int)xData.size());

      ImPlot::PopStyleVar();
      ImPlot::PopStyleColor();

      ImPlot::EndPlot();
    }
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Text("No data");
    ImGui::PopStyleColor();
  }
}

void GlobalDataStoreViewerUI::RenderControls() {
  ImGui::Spacing();
  ImGui::Separator();

  // Collapsible controls section
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
        GlobalDataStore::GetInstance()->SetValue(testChannel, testValue);
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
        GlobalDataStore::GetInstance()->SetValue("demo-sine", sineValue);
      }

      ImGui::SameLine();
      if (ImGui::Button("Generate Random Data")) {
        float randomValue = (rand() % 1000) / 100.0f;
        GlobalDataStore::GetInstance()->SetValue("demo-random", randomValue);
      }

      ImGui::TreePop();
    }

    ImGui::Unindent(10.0f);
  }
}

void GlobalDataStoreViewerUI::FormatValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize) {
  ServerConfig* config = nullptr;
  if (m_serverConfigs.find(channel) != m_serverConfigs.end()) {
    config = &m_serverConfigs[channel];
  }

  float absValue = std::abs(value);

  if (config && config->displayUnitSuffix) {
    std::string unit = config->unit;

    if (unit == "A") {
      // Current formatting
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
    else {
      // Other units
      if (absValue < 0.001f && absValue > 0.0f) {
        snprintf(buffer, bufferSize, "%.6f %s", value, unit.c_str());
      }
      else {
        snprintf(buffer, bufferSize, "%.3f %s", value, unit.c_str());
      }
    }
  }
  else {
    // No config or no unit suffix
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


// MODIFY the HasValueChanged method to include force refresh:
bool GlobalDataStoreViewerUI::HasValueChanged(const std::string& channel, float currentValue) {
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

// ADD this new method:
bool GlobalDataStoreViewerUI::ShouldRefresh() {
  auto currentTime = std::chrono::steady_clock::now();
  auto timeSinceLastRefresh = std::chrono::duration_cast<std::chrono::milliseconds>(
    currentTime - m_lastRefreshTime);

  if (timeSinceLastRefresh >= REFRESH_INTERVAL) {
    m_lastRefreshTime = currentTime;
    return true;
  }
  return false;
}