// include/scanning/scanning_algorithm.h
#pragma once

#include "include/motions/pi_controller.h"
#include "include/data/global_data_store.h"
#include "include/scanning/scanning_parameters.h"
#include "include/scanning/scan_data_collector.h"
#include "include/logger.h"

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Forward declarations
class ScanProgressEventArgs;
class ScanCompletedEventArgs;
class ScanErrorEventArgs;

class ScanningAlgorithm {
public:
  ScanningAlgorithm(
    PIController& piController,
    GlobalDataStore& dataStore,
    const std::string& deviceName,
    const std::string& dataChannel,
    const ScanningParameters& parameters);

  ~ScanningAlgorithm();

  // Start scanning (non-blocking)
  bool StartScan();

  // Stop the current scan
  void HaltScan();

  // Check if scanning is active
  bool IsScanningActive() const { return m_isScanningActive; }

  // Callback typedefs
  using ProgressCallback = std::function<void(const ScanProgressEventArgs&)>;
  using CompletionCallback = std::function<void(const ScanCompletedEventArgs&)>;
  using ErrorCallback = std::function<void(const ScanErrorEventArgs&)>;
  using DataPointCallback = std::function<void(double, const PositionStruct&)>;
  using PeakUpdateCallback = std::function<void(double, const PositionStruct&, const std::string&)>;

  // Set callbacks
  void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
  void SetCompletionCallback(CompletionCallback callback) { m_completionCallback = callback; }
  void SetErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }
  void SetDataPointCallback(DataPointCallback callback) { m_dataPointCallback = callback; }
  void SetPeakUpdateCallback(PeakUpdateCallback callback) { m_peakUpdateCallback = callback; }

private:
  // References to external systems
  PIController& m_piController;
  GlobalDataStore& m_dataStore;
  Logger* m_logger;

  // Scan configuration
  std::string m_deviceName;
  std::string m_dataChannel;
  ScanningParameters m_parameters;

  // Scan state
  std::atomic<bool> m_isScanningActive;
  std::atomic<bool> m_isHaltRequested;
  std::thread m_scanThread;
  std::mutex m_mutex;
  std::condition_variable m_cv;

  // Data collection
  ScanDataCollector m_dataCollector;

  // Peak information
  struct PeakData {
    double value;
    PositionStruct position;
    std::chrono::system_clock::time_point timestamp;
    std::string context;
  };
  PeakData m_globalPeak;

  // Baseline information
  struct BaselineData {
    double value;
    PositionStruct position;
    std::chrono::system_clock::time_point timestamp;
  };
  BaselineData m_baseline;

  // Callbacks
  ProgressCallback m_progressCallback;
  CompletionCallback m_completionCallback;
  ErrorCallback m_errorCallback;
  DataPointCallback m_dataPointCallback;
  PeakUpdateCallback m_peakUpdateCallback;

  // Scan execution methods
  void ScanThreadFunction();
  bool RecordBaseline();
  bool ExecuteScanSequence();

  // Axis scanning
  bool ScanAxis(const std::string& axis, double stepSize);
  std::pair<double, PositionStruct> ScanDirection(
    const std::string& axis, double stepSize, int direction);

  // Position and measurement methods
  bool MoveToPosition(const PositionStruct& position);
  bool MoveRelative(const std::string& axis, double distance);
  double GetMeasurement();
  bool ReturnToGlobalPeakIfBetter();
  bool GetCurrentPosition(PositionStruct& position);

  // Helper methods
  void UpdateGlobalPeak(double value, const PositionStruct& position, const std::string& context);
  double GetAxisValue(const PositionStruct& position, const std::string& axis);
  void HandleScanCancellation();
  void CleanupScan(bool success);

  // Event raising methods
  void OnProgressUpdated(double progress, const std::string& status);
  void OnScanCompleted(const ScanResults& results);
  void OnErrorOccurred(const std::string& message);
};

// Event arguments classes
class ScanProgressEventArgs {
public:
  ScanProgressEventArgs(double progress, const std::string& status)
    : m_progress(progress), m_status(status) {
  }

  double GetProgress() const { return m_progress; }
  const std::string& GetStatus() const { return m_status; }

private:
  double m_progress;
  std::string m_status;
};

class ScanCompletedEventArgs {
public:
  // Take a reference instead of creating a copy
  explicit ScanCompletedEventArgs(const ScanResults& results)
    : m_results(results) {
  }

  const ScanResults& GetResults() const { return m_results; }

private:
  // Store a const reference instead of a copy
  const ScanResults& m_results;
};

class ScanErrorEventArgs {
public:
  explicit ScanErrorEventArgs(const std::string& error)
    : m_error(error) {
  }

  const std::string& GetError() const { return m_error; }

private:
  std::string m_error;
};