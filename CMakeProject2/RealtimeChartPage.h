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
class MachineOperations;  // Change to MachineOperations instead of ScanningUI

// Use void* to avoid header conflicts with scanning system
// These will be cast to proper types in the implementation

class RealtimeChartPage {
public:
  struct DataPoint {
    double timestamp;
    float value;
  };

  // Button scanning states
  enum class ScanState {
    IDLE,
    SCANNING
  };

private:
  Logger* m_logger;
  GlobalDataStore* m_dataStore;
  Font m_customFont;
  bool m_fontLoaded;

  // Scanning system integration (using void* to avoid header conflicts)
  void* m_machineOperations;  // Change from scanningUI to machineOperations
  void* m_piControllerManager;

  // Data management
  std::string m_dataChannel;
  std::deque<DataPoint> m_dataBuffer;
  float m_timeWindow; // 10 seconds

  // Display state
  float m_currentValue;
  std::string m_displayUnit;
  float m_scaledValue;

  // Button states
  ScanState m_leftCoarseState;
  ScanState m_leftFineState;
  ScanState m_rightCoarseState;
  ScanState m_rightFineState;

  // Helper functions
  void updateData();
  void cleanOldData();
  void calculateDisplayValue();
  void renderDigitalDisplay();
  void renderChart();
  void renderButtons();
  void handleButtonClicks();
  std::pair<float, std::string> getScaledUnit(float value);

  // Button helper functions
  bool drawButton(Rectangle rect, const char* text, ScanState state);
  void startHexLeftCoarseScan();
  void startHexLeftFineScan();
  void startHexRightCoarseScan();
  void startHexRightFineScan();
  void stopAllScanning();

  // Scanning integration helpers
  bool isDeviceScanning(const std::string& deviceName);
  void updateButtonStatesFromScanning();

  // RunScanOperation execution helpers
  void executeRunScanOperation(const std::string& device,
    const std::vector<double>& stepSizes);

public:
  RealtimeChartPage(Logger* logger);
  ~RealtimeChartPage();

  void SetDataStore(GlobalDataStore* store);
  void SetMachineOperations(void* machineOps);  // Change from SetScanningUI
  void SetPIControllerManager(void* piManager);
  void Render();
};


// Add this near the end of machine_operations.h, after the class definition
extern "C" {
  bool MachineOperations_PerformScan(void* machineOpsPtr,
    const char* deviceName,
    const char* dataChannel,
    const double* stepSizes,
    int stepSizeCount,
    int settlingTimeMs,
    const char** axes,
    int axesCount,
    const char* callerContext);
}