// RealtimeChartPage.h
#pragma once

#include <vector>
#include <deque>
#include <string>
#include <chrono>
// Include raylib for Font type
#include <raylib.h>
// Forward declarations to avoid including raylib in header
struct Font;
class Logger;
class GlobalDataStore;

class RealtimeChartPage {
public:
  struct DataPoint {
    double timestamp;
    float value;
  };

private:
  Logger* m_logger;
  GlobalDataStore* m_dataStore;
  Font m_customFont;
  bool m_fontLoaded;

  // Data management
  std::string m_dataChannel;
  std::deque<DataPoint> m_dataBuffer;
  float m_timeWindow; // 10 seconds

  // Display state
  float m_currentValue;
  std::string m_displayUnit;
  float m_scaledValue;

  // Helper functions
  void updateData();
  void cleanOldData();
  void calculateDisplayValue();
  void renderDigitalDisplay();
  void renderChart();
  std::pair<float, std::string> getScaledUnit(float value);

public:
  RealtimeChartPage(Logger* logger);
  ~RealtimeChartPage();

  void SetDataStore(GlobalDataStore* store);
  void Render();
};