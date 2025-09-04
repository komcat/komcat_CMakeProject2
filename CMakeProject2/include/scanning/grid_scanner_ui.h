// include/scanning/grid_scanner_ui.h
#pragma once
#include "include/scanning/PIScanMotionAdapter.h"  // Add this
#include "include/motions/pi_controller_manager.h"  // Add this
#include "include/scanning/grid_scanner.h"
#include "imgui.h"
#include "implot/implot.h"
#include <memory>
#include <vector>
#include <string>

class GridScannerUI {
public:
  GridScannerUI();
  ~GridScannerUI() = default;

  // Main render function
  void Render();

  // Set the scanner instance
  void SetScanner(std::shared_ptr<GridScanner> scanner);

  // Set device and channel names for display
  void SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel);

  // Control visibility
  void Show() { m_visible = true; }
  void Hide() { m_visible = false; }
  bool IsVisible() const { return m_visible; }
  // Add this method to set managers for reconnection
  void SetManagers(PIControllerManager* piManager, DataClientManager* dataManager) {
    m_piManager = piManager;
    m_dataManager = dataManager;
  }
private:
  std::shared_ptr<GridScanner> m_scanner;

  // Window state
  bool m_visible;

  // Device info
  std::string m_deviceName;
  std::string m_dataChannel;

  // Display data
  std::vector<std::vector<double>> m_heatmapData;
  GridPoint m_currentPosition;
  double m_currentValue;
  bool m_dataUpdated;

  // Grid parameters (for UI controls)
  float m_xStep;
  float m_yStep;
  int m_xPoints;
  int m_yPoints;
  int m_settlingTime;

  // Heatmap display settings
  float m_colorScaleMin;
  float m_colorScaleMax;
  bool m_autoScale;
  int m_colormapIndex;

  // UI state
  bool m_scanInProgress;
  float m_scanProgress;

  // Internal methods
  void UpdateHeatmap(const GridPoint& point, double value);
  void RenderControls();
  void RenderHeatmap();
  void RenderStatus();
  void ResetHeatmapData();

  // Add these member variables
  PIControllerManager* m_piManager = nullptr;
  DataClientManager* m_dataManager = nullptr;

  // Add this method declaration
  void CheckAndReconnectHardware();

  bool m_showResultsDialog = false;
  double m_lastPeakValue = 0.0;
  GridPoint m_lastPeakPosition;
  void RenderResultsDialog();
};