// motion_ops_scanning.cpp
#include "motion_ops.h"
#include <algorithm>
#include <thread>

// Scanning methods implementation

bool MotionOps::PerformScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"data_channel", dataChannel},
        {"settling_time_ms", std::to_string(settlingTimeMs)},
        {"axes_count", std::to_string(axesToScan.size())},
        {"steps_count", std::to_string(stepSizes.size())}
    };

    // Add step sizes and axes to parameters
    for (size_t i = 0; i < stepSizes.size() && i < 3; ++i) {
      parameters["step_size_" + std::to_string(i)] = std::to_string(stepSizes[i]);
    }
    for (size_t i = 0; i < axesToScan.size() && i < 3; ++i) {
      parameters["axis_" + std::to_string(i)] = axesToScan[i];
    }

    opId = m_resultsManager->StartOperation("PerformScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Starting scan for device " + deviceName +
    " using data channel " + dataChannel +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Get the PI controller for the device
  PIController* controller = m_piControllerManager.GetController(deviceName);
  if (!controller || !controller->IsConnected()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "No connected PI controller for device " + deviceName);
    }

    m_logger->LogError("MotionOps: No connected PI controller for device " + deviceName);
    return false;
  }

  // Setup scanning parameters
  ScanningParameters params = ScanningParameters::CreateDefault();
  params.axesToScan = axesToScan;
  params.stepSizes = stepSizes;
  params.motionSettleTimeMs = settlingTimeMs;

  bool scanSuccess = false;
  std::string errorMessage;

  try {
    // Validate parameters
    params.Validate();

    // Create scanning algorithm
    GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
    ScanningAlgorithm scanner(*controller, *dataStore, deviceName, dataChannel, params);

    // Start the scan (blocking)
    m_logger->LogInfo("MotionOps: Executing scan");
    bool success = scanner.StartScan();

    if (success) {
      m_logger->LogInfo("MotionOps: Scan started for device " + deviceName);

      // Wait for scan to complete
      while (scanner.IsScanningActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      m_logger->LogInfo("MotionOps: Scan completed for device " + deviceName);
      scanSuccess = true;
    }
    else {
      errorMessage = "Failed to start scan for device " + deviceName;
      m_logger->LogError("MotionOps: " + errorMessage);
      scanSuccess = false;
    }
  }
  catch (const std::exception& e) {
    errorMessage = "Exception during scan: " + std::string(e.what());
    m_logger->LogError("MotionOps: " + errorMessage);
    scanSuccess = false;
  }

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "scan_type", "blocking_perform_scan");

    if (scanSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", errorMessage.empty() ? "PerformScan operation failed" : errorMessage);
    }
  }

  return scanSuccess;
}

bool MotionOps::StartScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"data_channel", dataChannel},
        {"settling_time_ms", std::to_string(settlingTimeMs)},
        {"axes_count", std::to_string(axesToScan.size())},
        {"steps_count", std::to_string(stepSizes.size())}
    };

    // Add step sizes and axes to parameters
    for (size_t i = 0; i < stepSizes.size() && i < 3; ++i) {
      parameters["step_size_" + std::to_string(i)] = std::to_string(stepSizes[i]);
    }
    for (size_t i = 0; i < axesToScan.size() && i < 3; ++i) {
      parameters["axis_" + std::to_string(i)] = axesToScan[i];
    }

    opId = m_resultsManager->StartOperation("StartScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Starting asynchronous scan for device " + deviceName +
    " using data channel " + dataChannel +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Check if a scan is already active for this device
  bool needsReset = false;
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    if (m_activeScans.find(deviceName) != m_activeScans.end()) {
      // Check if the scan is truly active or just stalled
      auto& scanner = m_activeScans[deviceName];
      if (scanner && scanner->IsScanningActive()) {
        auto endTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        if (m_resultsManager && !opId.empty()) {
          m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
          m_resultsManager->EndOperation(opId, "failed", "Scan already in progress for device " + deviceName);
        }

        m_logger->LogWarning("MotionOps: Scan already in progress for device " + deviceName);
        return false;
      }
      else {
        // The scanner exists but is not active - this is a stalled state
        needsReset = true;
        m_logger->LogWarning("MotionOps: Found stalled scanner for device " + deviceName + ", will reset");
      }
    }

    // Also check the scan info status
    auto infoIt = m_scanInfo.find(deviceName);
    if (infoIt != m_scanInfo.end() && infoIt->second.isActive.load()) {
      // The scan info shows active but we didn't find an active scanner
      // This is definitely a desynchronized state
      needsReset = true;
      m_logger->LogWarning("MotionOps: Scan info shows active but no active scanner for " + deviceName + ", will reset");
    }
  }

  // Reset state if needed
  if (needsReset) {
    ResetScanState(deviceName);
  }

  // Get the PI controller for the device
  PIController* controller = m_piControllerManager.GetController(deviceName);
  if (!controller || !controller->IsConnected()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "No connected PI controller for device " + deviceName);
    }

    m_logger->LogError("MotionOps: No connected PI controller for device " + deviceName);
    return false;
  }

  // Setup scanning parameters
  ScanningParameters params = ScanningParameters::CreateDefault();
  params.axesToScan = axesToScan;
  params.stepSizes = stepSizes;
  params.motionSettleTimeMs = settlingTimeMs;

  bool scanStartSuccess = false;
  std::string errorMessage;

  try {
    // Validate parameters
    params.Validate();

    // Create and configure the scanning algorithm
    auto scanner = std::make_unique<ScanningAlgorithm>(
      *controller,
      *GlobalDataStore::GetInstance(),
      deviceName,
      dataChannel,
      params
    );

    // Initialize scan info - create it in the map if it doesn't exist yet
    {
      std::lock_guard<std::mutex> lock(m_scanMutex);

      // Create the entry if it doesn't exist
      if (m_scanInfo.find(deviceName) == m_scanInfo.end()) {
        m_scanInfo.emplace(std::piecewise_construct,
          std::forward_as_tuple(deviceName),
          std::forward_as_tuple());
      }

      // Initialize the values directly
      m_scanInfo[deviceName].isActive.store(true);
      m_scanInfo[deviceName].progress.store(0.0);

      // Set the status with proper locking
      {
        std::lock_guard<std::mutex> statusLock(m_scanInfo[deviceName].statusMutex);
        m_scanInfo[deviceName].status = "Starting scan...";
      }
    }

    // Set callbacks to update status
    scanner->SetProgressCallback([this, deviceName](const ScanProgressEventArgs& args) {
      this->m_scanInfo[deviceName].progress.store(args.GetProgress());
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = args.GetStatus();
    });

    scanner->SetPeakUpdateCallback([this, deviceName](double value, const PositionStruct& position, const std::string& context) {
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].peakMutex);
      this->m_scanInfo[deviceName].peakValue = value;
      this->m_scanInfo[deviceName].peakPosition = position;
    });

    scanner->SetCompletionCallback([this, deviceName](const ScanCompletedEventArgs& args) {
      // Update status, but don't remove scanner here
      this->m_scanInfo[deviceName].isActive.store(false);
      this->m_scanInfo[deviceName].progress.store(1.0);
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = "Scan completed";

      // Schedule cleanup to happen later from the main thread
      // We can't erase from m_activeScans here since we're running in the scanner's thread
      // The scanner will be cleaned up when another scan is started or the program ends
    });

    scanner->SetErrorCallback([this, deviceName](const ScanErrorEventArgs& args) {
      // Update status, but don't remove scanner here
      this->m_scanInfo[deviceName].isActive.store(false);
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = "Error: " + args.GetError();

      // Same as above - don't erase from m_activeScans here
    });

    // Start the scan
    if (!scanner->StartScan()) {
      errorMessage = "Failed to start scan for device " + deviceName;
      m_logger->LogError("MotionOps: " + errorMessage);
      scanStartSuccess = false;
    }
    else {
      // Store the scanner
      {
        std::lock_guard<std::mutex> lock(m_scanMutex);
        m_activeScans[deviceName] = std::move(scanner);
      }

      m_logger->LogInfo("MotionOps: Scan started for device " + deviceName);
      scanStartSuccess = true;
    }
  }
  catch (const std::exception& e) {
    errorMessage = "Exception during scan setup: " + std::string(e.what());
    m_logger->LogError("MotionOps: " + errorMessage);
    scanStartSuccess = false;
  }

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "scan_type", "asynchronous_start_scan");

    if (scanStartSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", errorMessage.empty() ? "StartScan operation failed" : errorMessage);
    }
  }

  return scanStartSuccess;
}

bool MotionOps::StopScan(const std::string& deviceName, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName}
    };
    opId = m_resultsManager->StartOperation("StopScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Stopping scan for device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  std::unique_ptr<ScanningAlgorithm> scanner;
  bool success = false;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      // Even if the scanner isn't found, reset the scan info data
      // This is critical for recovery from abnormal states
      auto infoIt = m_scanInfo.find(deviceName);
      if (infoIt != m_scanInfo.end()) {
        infoIt->second.isActive.store(false);
        std::lock_guard<std::mutex> statusLock(infoIt->second.statusMutex);
        infoIt->second.status = "No active scan";
      }
      m_logger->LogWarning("MotionOps: No active scan found for device " + deviceName + ", but reset status anyway");
      success = true; // Return success since the end goal is achieved (no active scan)
    }
    else {
      scanner = std::move(it->second);
      m_activeScans.erase(it);
    }
  }

  // Stop the scan
  if (scanner) {
    scanner->HaltScan();
    m_logger->LogInfo("MotionOps: Scan stopped for device " + deviceName);
    // Update scan status
    auto it = m_scanInfo.find(deviceName);
    if (it != m_scanInfo.end()) {
      it->second.isActive.store(false);
      std::lock_guard<std::mutex> lock(it->second.statusMutex);
      it->second.status = "Scan stopped by user";
    }
    // Use our safe cleanup method
    success = SafelyCleanupScanner(deviceName);
  }
  else if (!success) {
    success = false;
  }

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "StopScan operation failed");
    }
  }

  return success;
}

// Scan status methods
bool MotionOps::IsScanActive(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.isActive;
  }
  return false;
}

double MotionOps::GetScanProgress(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.progress;
  }
  return 0.0;
}

std::string MotionOps::GetScanStatus(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.statusMutex);
    return it->second.status;
  }
  return "No scan information available";
}

bool MotionOps::GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.peakMutex);
    value = it->second.peakValue;
    position = it->second.peakPosition;
    return (value > 0.0); // Return true if a valid peak was found
  }
  return false;
}

// Scanner cleanup methods
bool MotionOps::CleanupAllScanners() {
  std::lock_guard<std::mutex> lock(m_scanMutex);

  bool success = true;
  // First, try to stop all active scanners
  for (auto& [deviceName, scanner] : m_activeScans) {
    if (scanner && scanner->IsScanningActive()) {
      m_logger->LogInfo("MotionOps: Halting lingering scan for " + deviceName);
      scanner->HaltScan();

      // Wait briefly for it to stop
      for (int i = 0; i < 10; i++) {
        if (!scanner->IsScanningActive()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }

      if (scanner->IsScanningActive()) {
        m_logger->LogWarning("MotionOps: Failed to halt scan for " + deviceName);
        success = false;
      }
    }
  }

  // Clear all active scans regardless
  m_activeScans.clear();

  // Reset all scan info states
  for (auto& [deviceName, info] : m_scanInfo) {
    info.isActive.store(false);
    std::lock_guard<std::mutex> statusLock(info.statusMutex);
    info.status = "Ready";
  }

  return success;
}

bool MotionOps::ResetScanState(const std::string& deviceName) {
  std::lock_guard<std::mutex> lock(m_scanMutex);

  // Reset active scans tracking
  auto scanIt = m_activeScans.find(deviceName);
  if (scanIt != m_activeScans.end()) {
    // Try to halt the scan if it's still active
    if (scanIt->second && scanIt->second->IsScanningActive()) {
      scanIt->second->HaltScan();

      // Wait briefly for it to stop
      for (int i = 0; i < 10; i++) {
        if (!scanIt->second->IsScanningActive()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    // Remove from active scans
    m_activeScans.erase(scanIt);
    m_logger->LogInfo("MotionOps: Removed stalled scan for " + deviceName);
  }

  // Reset scan info state
  auto infoIt = m_scanInfo.find(deviceName);
  if (infoIt != m_scanInfo.end()) {
    infoIt->second.isActive.store(false);
    std::lock_guard<std::mutex> statusLock(infoIt->second.statusMutex);
    infoIt->second.status = "Ready";
  }

  return true;
}

bool MotionOps::SafelyCleanupScanner(const std::string& deviceName) {
  std::unique_ptr<ScanningAlgorithm> scanner;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      return false;  // Nothing to clean up
    }

    scanner = std::move(it->second);
    m_activeScans.erase(it);
  }

  // Now safely stop the scan if it's running
  if (scanner) {
    if (scanner->IsScanningActive()) {
      scanner->HaltScan();

      // Wait a brief period to allow thread to exit
      for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds
        if (!scanner->IsScanningActive()) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    // The scanner will be destructed when this method returns
    return true;
  }

  return false;
}