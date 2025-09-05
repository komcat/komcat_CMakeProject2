// include/scanning/grid_volume_scanner_ui.h
#pragma once

#include "grid_volume_scanner.h"
#include "PIScanMotionAdapter.h"
#include "include/motions/pi_controller_manager.h"
#include "include/data/data_client_manager.h"
#include "imgui.h"
#include "implot/implot.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>

class GridVolumeScannerUI {
public:
  GridVolumeScannerUI();
  ~GridVolumeScannerUI() = default;

  // Main render function
  void Render();

  // Set the scanner instance
  void SetScanner(std::shared_ptr<GridVolumeScanner> scanner);

  // Set device and channel names for display
  void SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel);

  // Control visibility
  void Show() { m_visible = true; }
  void Hide() { m_visible = false; }
  bool IsVisible() const { return m_visible; }

  // Set managers for reconnection
  void SetManagers(PIControllerManager* piManager, DataClientManager* dataManager) {
    m_piManager = piManager;
    m_dataManager = dataManager;
  }

  void TestZMovement();

private:
  // Scanner instance
  std::shared_ptr<GridVolumeScanner> m_scanner;

  // Managers
  PIControllerManager* m_piManager = nullptr;
  DataClientManager* m_dataManager = nullptr;

  // Window state
  bool m_visible = false;

  // Device info
  std::string m_deviceName = "Unknown";
  std::string m_dataChannel = "Unknown";

  // Grid parameters (for UI controls)
  float m_xStep = 50.0f;     // µm
  float m_yStep = 10.0f;      // µm
  int m_xPoints = 5;
  int m_yPoints = 5;
  int m_settlingTime = 100;   // ms

  // NEW Z scan parameters
  int m_zDirection = 1;       // 0 = negative, 1 = positive
  float m_zStepSize = 5.0f;   // µm per step
  int m_zSteps = 10;          // number of steps (this replaces m_zLayers)

  // Volume data for display
  VolumeScanData m_volumeData;
  int m_currentLayer = 0;
  std::vector<std::vector<double>> m_currentLayerData;

  // Display settings
  float m_colorScaleMin = 0.0f;
  float m_colorScaleMax = 1.0f;
  bool m_autoScale = true;
  int m_colormapIndex = 0;

  // UI state
  bool m_scanInProgress = false;
  float m_scanProgress = 0.0f;
  int m_currentScanLayer = 0;
  double m_currentZ = 0.0;  // in mm from scanner

  // Results dialog
  bool m_showResultsDialog = false;
  VolumeGridPoint m_lastPeakPosition;
  double m_lastPeakValue = 0.0;
  double m_lastScanDuration = 0.0;

  // Internal methods
  void RenderControls();
  void RenderLayerView();
  void RenderVolumeStats();
  void RenderResultsDialog();
  void CheckAndReconnectHardware();
  void UpdateLayerDisplay(int layer);
  void ResetVolumeData();

  // Helper to calculate Z scan range
  void CalculateZRange(float& zStart, float& zEnd) const;


};