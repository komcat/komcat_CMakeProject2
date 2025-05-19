// Modified scanning_ui.h with optimizations
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
#include <deque>

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
  std::atomic<bool> m_isScanning{ false };
  std::atomic<double> m_scanProgress{ 0.0 };

  // Status string requires mutex protection
  std::string m_scanStatus = "Ready";
  std::mutex m_statusMutex;

  // Results storage
  std::atomic<bool> m_hasResults{ false };
  std::unique_ptr<ScanResults> m_lastResults;

  // Measurement values - use atomics for thread safety
  std::atomic<double> m_currentValue{ 0.0 };
  std::atomic<double> m_peakValue{ 0.0 };

  // Position structures - need mutex protection
  PositionStruct m_currentPosition;
  PositionStruct m_peakPosition;
  std::string m_peakContext;

  // Data batching for measurement updates
  static constexpr size_t MAX_BATCH_SIZE = 10;
  std::deque<std::pair<double, PositionStruct>> m_recentMeasurements;
  std::mutex m_dataMutex; // Renamed from m_dataMutex to be more specific

  // Simplified UI sections
  void RenderDeviceSelection();
  void RenderScanControls();
  void RenderScanStatus();

  // Event handlers - optimized for reduced synchronization
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

  // Process batched measurements if needed
  void ProcessMeasurementBatch();

  // Step size presets
  struct StepSizePreset {
    std::string name;
    std::vector<double> stepSizes;
  };
  std::vector<StepSizePreset> m_stepSizePresets;
  int m_selectedPresetIndex = 0;

  // Initialize presets in the constructor
  void InitializeStepSizePresets();
};