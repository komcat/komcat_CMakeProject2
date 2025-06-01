// AdaptiveScanningUI.h
#pragma once

#include "AdaptivePowerScanner.h"
#include "include/motions/pi_controller_manager.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include "include/ui/ToolbarMenu.h"
#include "imgui.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

class AdaptiveScanningUI : public ITogglableUI {
public:
  // Constructor
  AdaptiveScanningUI(PIControllerManager& piControllerManager, GlobalDataStore& dataStore);
  ~AdaptiveScanningUI();

  // ITogglableUI implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Main UI rendering
  void RenderUI();

private:
  // System references
  PIControllerManager& m_piControllerManager;
  GlobalDataStore& m_dataStore;
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  std::string m_windowTitle = "Adaptive Power Scanner";

  // Device selection
  std::string m_selectedDevice;
  std::vector<std::string> m_hexapodDevices = { "hex-left", "hex-right" };
  std::string m_selectedDataChannel = "GPIB-Current";
  std::vector<std::string> m_availableDataChannels;

  // Scanner and configuration
  std::unique_ptr<AdaptivePowerScanner> m_scanner;
  AdaptivePowerScanner::ScanConfig m_scanConfig;

  // Scanning state
  std::atomic<bool> m_isScanning{ false };
  std::atomic<double> m_scanProgress{ 0.0 };
  std::atomic<double> m_currentValue{ 0.0 };
  std::atomic<double> m_peakValue{ 0.0 };

  std::string m_scanStatus = "Ready";
  std::mutex m_statusMutex;

  // Configuration UI state
  struct UIConfig {
    // Power range settings
    float minPowerUA = 2.0f;      // 2μA - minimum threshold power
    float maxPowerUA = 400.0f;    // 400μA - maximum expected power

    // Step size range settings
    float minStepMicrons = 0.2f;  // 0.2μm - smallest step size (high precision)
    float maxStepMicrons = 10.0f; // 10μm - largest step size (fast movement)

    // Direction constraints
    int zDirection = 1;           // 0=Both, 1=Negative only, 2=Positive only
    int xyDirection = 0;          // 0=Both, 1=Negative only, 2=Positive only

    // Safety and algorithm parameters
    float maxTravelMM = 5.0f;     // 5mm maximum travel distance
    float improvementThreshold = 0.5f; // 0.5% minimum improvement to continue

    // Feature toggles
    bool enablePhysicsConstraints = true;  // Use direction constraints
    bool enableAdaptiveSteps = true;       // Use power-based step sizing
  } m_uiConfig;

  // Scan results
  std::vector<ScanStep> m_scanHistory;
  ScanStep m_peakPosition;
  std::mutex m_dataMutex;

  // UI rendering methods
  void RenderDeviceSelection();
  void RenderConfiguration();
  void RenderScanControls();
  void RenderScanStatus();
  void RenderResults();

  // Configuration and setup
  void RefreshAvailableDevices();
  void RefreshAvailableDataChannels();
  void UpdateScanConfig();

  // Scan control
  void StartScan();
  void StopScan();

  // Hardware interface functions
  double PerformMeasurement(double x, double y, double z);
  bool IsPositionValid(double x, double y, double z);
  bool MoveToPosition(double x, double y, double z);

  // Helper methods
  PIController* GetSelectedController() const;
  std::string FormatPosition(const ScanStep& step) const;
};