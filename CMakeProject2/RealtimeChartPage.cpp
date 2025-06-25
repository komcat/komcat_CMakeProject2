// RealtimeChartPage.cpp
#include "RealtimeChartPage.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>

RealtimeChartPage::RealtimeChartPage(Logger* logger)
  : m_logger(logger), m_dataStore(nullptr), m_fontLoaded(false),
  m_dataChannel("GPIB-Current"), m_timeWindow(10.0f),
  m_currentValue(0.0f), m_scaledValue(0.0f) {

  if (m_logger) {
    m_logger->LogInfo("RealtimeChartPage created");
  }

  // Load custom font
  m_customFont = LoadFont("assets/fonts/CascadiaCode-Regular.ttf");
  if (m_customFont.texture.id != 0) {
    m_fontLoaded = true;
    if (m_logger) {
      m_logger->LogInfo("CascadiaCode font loaded successfully");
    }
  }
  else {
    m_fontLoaded = false;
    if (m_logger) {
      m_logger->LogWarning("Failed to load CascadiaCode font, using default");
    }
  }
}

RealtimeChartPage::~RealtimeChartPage() {
  if (m_fontLoaded) {
    UnloadFont(m_customFont);
  }

  if (m_logger) {
    m_logger->LogInfo("RealtimeChartPage destroyed");
  }
}

void RealtimeChartPage::SetDataStore(GlobalDataStore* store) {
  m_dataStore = store;
}

void RealtimeChartPage::Render() {
  // Update data first
  updateData();

  // Page title and navigation info
  DrawText("Realtime Chart (C)", 10, 10, 20, DARKBLUE);
  DrawText("C: Chart | M: Menu | V: Live Video | S: Status | R: Rectangles", 10, 40, 14, GRAY);

  // Get screen dimensions
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Calculate layout areas
  int topSectionHeight = (int)(screenHeight * 0.6f);
  int chartSectionY = topSectionHeight;
  int chartSectionHeight = screenHeight - topSectionHeight;

  // Render digital display (top 60%)
  renderDigitalDisplay();

  // Render chart (bottom 40%)
  renderChart();
}

void RealtimeChartPage::updateData() {
  if (!m_dataStore) {
    if (m_logger) {
      static int noDataStoreCount = 0;
      if (++noDataStoreCount % 120 == 0) { // Log every 2 seconds at 60 FPS
        m_logger->LogWarning("RealtimeChartPage: dataStore is NULL");
      }
    }
    return;
  }

  // Get current time
  auto now = std::chrono::steady_clock::now();
  double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();

  // Debug: Check available channels
  if (m_logger) {
    static int debugCount = 0;
    if (++debugCount % 300 == 0) { // Log every 5 seconds at 60 FPS
      auto channels = m_dataStore->GetAvailableChannels();
      m_logger->LogInfo("RealtimeChart: Available channels: " + std::to_string(channels.size()));
      for (const auto& ch : channels) {
        float val = m_dataStore->GetValue(ch, -999.0f);
        m_logger->LogInfo("  Channel: " + ch + " = " + std::to_string(val));
      }
    }
  }

  // Try to get current value from data store
  if (m_dataStore->HasValue(m_dataChannel)) {
    float newValue = m_dataStore->GetValue(m_dataChannel, 0.0f);

    // Debug: Log value changes
    if (m_logger && std::abs(newValue - m_currentValue) > 1e-15f) {
      static int valueChangeCount = 0;
      if (++valueChangeCount % 60 == 0) { // Log every second
        m_logger->LogInfo("RealtimeChart: " + m_dataChannel + " = " + std::to_string(newValue));
      }
    }

    m_currentValue = newValue;

    // Add to buffer
    m_dataBuffer.push_back({ currentTime, newValue });

    // Clean old data
    cleanOldData();

    // Calculate display values
    calculateDisplayValue();
  }
  else {
    if (m_logger) {
      static int noValueCount = 0;
      if (++noValueCount % 300 == 0) { // Log every 5 seconds
        m_logger->LogWarning("RealtimeChart: Channel '" + m_dataChannel + "' not found in dataStore");
      }
    }
  }
}

void RealtimeChartPage::cleanOldData() {
  auto now = std::chrono::steady_clock::now();
  double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
  double cutoffTime = currentTime - m_timeWindow;

  // Remove old data points
  while (!m_dataBuffer.empty() && m_dataBuffer.front().timestamp < cutoffTime) {
    m_dataBuffer.pop_front();
  }
}

void RealtimeChartPage::calculateDisplayValue() {
  auto [scaledValue, unit] = getScaledUnit(std::abs(m_currentValue));
  m_scaledValue = (m_currentValue >= 0) ? scaledValue : -scaledValue;
  m_displayUnit = unit;
}

std::pair<float, std::string> RealtimeChartPage::getScaledUnit(float absValue) {
  // Auto-scale units for current (assuming GPIB-Current)
  if (absValue < 1e-9f) {
    return { absValue * 1e12f, "pA" };
  }
  else if (absValue < 1e-6f) {
    return { absValue * 1e9f, "nA" };
  }
  else if (absValue < 1e-3f) {
    return { absValue * 1e6f, "μA" };
  }
  else if (absValue < 1.0f) {
    return { absValue * 1e3f, "mA" };
  }
  else {
    return { absValue, "A" };
  }
}

void RealtimeChartPage::renderDigitalDisplay() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Background for digital display area
  DrawRectangle(0, 70, screenWidth, topSectionHeight - 70, Color{ 30, 30, 40, 255 });
  DrawRectangleLines(0, 70, screenWidth, topSectionHeight - 70, DARKGRAY);

  // Channel name
  int channelFontSize = 24;
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();

  Vector2 channelTextSize = MeasureTextEx(font, m_dataChannel.c_str(), channelFontSize, 2);
  int channelX = screenWidth / 2 - (int)channelTextSize.x / 2;
  DrawTextEx(font, m_dataChannel.c_str(), Vector2{ (float)channelX, 100 }, channelFontSize, 2, LIGHTGRAY);

  // Digital value display
  char valueText[64];
  snprintf(valueText, sizeof(valueText), "%.3f %s", m_scaledValue, m_displayUnit.c_str());

  int valueFontSize = 48;
  Vector2 valueTextSize = MeasureTextEx(font, valueText, valueFontSize, 2);
  int valueX = screenWidth / 2 - (int)valueTextSize.x / 2;
  int valueY = topSectionHeight / 2 - valueFontSize / 2;

  // Value color based on magnitude
  Color valueColor = GREEN;
  if (std::abs(m_currentValue) < 1e-9f) {
    valueColor = GRAY;
  }
  else if (std::abs(m_currentValue) > 1e-3f) {
    valueColor = ORANGE;
  }

  DrawTextEx(font, valueText, Vector2{ (float)valueX, (float)valueY }, valueFontSize, 2, valueColor);

  // Data points info
  char infoText[32];
  snprintf(infoText, sizeof(infoText), "Points: %zu", m_dataBuffer.size());
  DrawText(infoText, 20, topSectionHeight - 30, 16, DARKGRAY);
}

void RealtimeChartPage::renderChart() {
  if (m_dataBuffer.empty()) return;

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);
  int chartY = topSectionHeight;
  int chartHeight = screenHeight - topSectionHeight;

  // Chart area background
  Rectangle chartArea = { 20, (float)chartY + 20, (float)screenWidth - 40, (float)chartHeight - 40 };
  DrawRectangleRec(chartArea, Color{ 20, 20, 30, 255 });
  DrawRectangleLinesEx(chartArea, 2, DARKGRAY);

  // Chart title
  DrawText("10 Second History", 30, chartY + 5, 16, WHITE);

  if (m_dataBuffer.size() < 2) return;

  // Find data range
  auto minMaxValue = std::minmax_element(m_dataBuffer.begin(), m_dataBuffer.end(),
    [](const DataPoint& a, const DataPoint& b) { return a.value < b.value; });

  float minValue = minMaxValue.first->value;
  float maxValue = minMaxValue.second->value;

  // Add some padding to the range
  float range = maxValue - minValue;
  if (range < 1e-12f) range = 1e-12f; // Prevent division by zero
  minValue -= range * 0.1f;
  maxValue += range * 0.1f;

  // Get time range
  double minTime = m_dataBuffer.front().timestamp;
  double maxTime = m_dataBuffer.back().timestamp;
  double timeRange = maxTime - minTime;
  if (timeRange < 0.1) timeRange = 0.1; // Minimum time range

  // Draw chart lines
  for (size_t i = 1; i < m_dataBuffer.size(); ++i) {
    const auto& prev = m_dataBuffer[i - 1];
    const auto& curr = m_dataBuffer[i];

    // Map to screen coordinates
    float x1 = chartArea.x + ((prev.timestamp - minTime) / timeRange) * chartArea.width;
    float y1 = chartArea.y + chartArea.height - ((prev.value - minValue) / (maxValue - minValue)) * chartArea.height;

    float x2 = chartArea.x + ((curr.timestamp - minTime) / timeRange) * chartArea.width;
    float y2 = chartArea.y + chartArea.height - ((curr.value - minValue) / (maxValue - minValue)) * chartArea.height;

    DrawLineEx(Vector2{ x1, y1 }, Vector2{ x2, y2 }, 2.0f, LIME);
  }

  // Draw current value point
  if (!m_dataBuffer.empty()) {
    const auto& last = m_dataBuffer.back();
    float x = chartArea.x + ((last.timestamp - minTime) / timeRange) * chartArea.width;
    float y = chartArea.y + chartArea.height - ((last.value - minValue) / (maxValue - minValue)) * chartArea.height;
    DrawCircle((int)x, (int)y, 4, RED);
  }

  // Y-axis labels
  auto [scaledMin, unitMin] = getScaledUnit(std::abs(minValue));
  auto [scaledMax, unitMax] = getScaledUnit(std::abs(maxValue));

  char minLabel[32], maxLabel[32];
  snprintf(minLabel, sizeof(minLabel), "%.2f%s", (minValue >= 0) ? scaledMin : -scaledMin, unitMin.c_str());
  snprintf(maxLabel, sizeof(maxLabel), "%.2f%s", (maxValue >= 0) ? scaledMax : -scaledMax, unitMax.c_str());

  DrawText(maxLabel, 25, (int)chartArea.y + 5, 12, LIGHTGRAY);
  DrawText(minLabel, 25, (int)(chartArea.y + chartArea.height - 15), 12, LIGHTGRAY);
}