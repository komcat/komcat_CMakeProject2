// include/scanning/scanning_ui.h
#pragma once

#include "include/scanning/scanning_algorithm.h"
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

class ScanningUI : public ITogglableUI {
public:
  // Constructor without MotionControlLayer
  ScanningUI(PIControllerManager& piControllerManager,
    GlobalDataStore& dataStore);
  ~ScanningUI();

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
  std::string m_windowTitle = "Hexapod Optimizer";

  // Device selection
  std::string m_selectedDevice;
  std::vector<std::string> m_hexapodDevices = { "hex-left", "hex-right" };

  // Data channel selection
  std::string m_selectedDataChannel = "GPIB-Current";
  std::vector<std::string> m_availableDataChannels;

  // Scan parameters
  ScanningParameters m_parameters;

  // Scanning state
  std::unique_ptr<ScanningAlgorithm> m_scanner;
  std::atomic<bool> m_isScanning = false;
  double m_scanProgress = 0.0;
  std::string m_scanStatus = "Ready";

  // Results storage
  bool m_hasResults = false;
  std::unique_ptr<ScanResults> m_lastResults;

  // Current measurement
  double m_currentValue = 0.0;
  PositionStruct m_currentPosition;

  // Best measurement
  double m_peakValue = 0.0;
  PositionStruct m_peakPosition;
  std::string m_peakContext;
  std::mutex m_dataMutex;

  // Simplified UI sections
  void RenderDeviceSelection();
  void RenderScanControls();
  void RenderScanStatus();

  // Event handlers
  void OnProgressUpdated(const ScanProgressEventArgs& args);
  void OnScanCompleted(const ScanCompletedEventArgs& args);
  void OnErrorOccurred(const ScanErrorEventArgs& args);
  void OnDataPointAcquired(double value, const PositionStruct& position);
  void OnPeakUpdated(double value, const PositionStruct& position, const std::string& context);

  // Helper methods
  void RefreshAvailableDevices();
  void RefreshAvailableDataChannels();
  void StartScan();
  void StopScan();
  std::string FormatPosition(const PositionStruct& position) const;

  // Get the PI controller for the selected device
  PIController* GetSelectedController() const;
};