// optimized_scanning_ui.h
#pragma once

#include "SequentialOptimizedScanner.h"
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

class OptimizedScanningUI : public ITogglableUI {
public:
  // Constructor
  OptimizedScanningUI(PIControllerManager& piControllerManager,
    GlobalDataStore& dataStore);
  ~OptimizedScanningUI();

  // ITogglableUI implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Render UI
  void RenderUI();

private:
  // References to external systems
  PIControllerManager& m_piControllerManager;
  GlobalDataStore& m_dataStore;
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  std::string m_windowTitle = "Optimized Hexapod Scanner";

  // Device selection
  std::string m_selectedDevice;
  std::vector<std::string> m_hexapodDevices = { "hex-left", "hex-right" };

  // Data channel selection
  std::string m_selectedDataChannel = "GPIB-Current";
  std::vector<std::string> m_availableDataChannels;

  // Scanner
  std::unique_ptr<SequentialOptimizedScanner> m_scanner;

  // Scanning state
  std::atomic<bool> m_isScanning{ false };
  std::atomic<double> m_scanProgress{ 0.0 };
  std::string m_scanStatus = "Ready";
  std::mutex m_statusMutex;

  // Current scan data
  std::atomic<double> m_currentValue{ 0.0 };
  std::atomic<double> m_peakValue{ 0.0 };
  ScanStep m_currentPosition;
  ScanStep m_peakPosition;
  std::mutex m_dataMutex;

  // Scan results
  std::vector<ScanStep> m_scanHistory;

  // UI sections
  void RenderDeviceSelection();
  void RenderScanControls();
  void RenderScanStatus();
  void RenderResults();

  // Helper methods
  void RefreshAvailableDevices();
  void RefreshAvailableDataChannels();
  void StartScan();
  void StopScan();
  std::string FormatPosition(const ScanStep& step) const;
  PIController* GetSelectedController() const;

  // Hardware interface functions
  double PerformMeasurement(double x, double y, double z);
  bool IsPositionValid(double x, double y, double z);
  bool MoveToPosition(double x, double y, double z);


private:
  // Performance optimization members
  std::chrono::steady_clock::time_point m_lastUIUpdate;
  static constexpr int UI_UPDATE_INTERVAL_MS = 100; // Update UI every 100ms instead of every frame

  // Cached values to avoid repeated expensive calls
  double m_cachedCurrentValue = 0.0;
  bool m_cachedCanStartScan = false;
  bool m_cachedIsControllerMoving = false;
  std::string m_cachedStatusText = "Ready";

  // Helper methods
  bool ShouldUpdateUI();
  void UpdateCachedValues();
};